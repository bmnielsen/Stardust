#include "OpponentEconomicModel.h"

#include "Map.h"
#include "UnitUtil.h"

#define MODEL_FRAME_LIMIT 20000

namespace
{
    const double MINERALS_PER_WORKER_FRAME = 0.0445;
    const double GAS_PER_WORKER_FRAME = 0.071;

    // Represents an enemy unit in our economic model
    struct EcoModelUnit
    {
        struct cmp
        {
            bool operator()(const std::shared_ptr<EcoModelUnit> &a, const std::shared_ptr<EcoModelUnit> &b) const
            {
                return a->creationFrame < b->creationFrame
                       || (b->creationFrame >= a->creationFrame && a->completionFrame < b->completionFrame)
                       || (b->creationFrame >= a->creationFrame && b->completionFrame >= a->completionFrame && a < b);
            }
        };

        BWAPI::UnitType type;
        int id;
        int creationFrame;
        int completionFrame;
        int deathFrame;

        EcoModelUnit(BWAPI::UnitType type, int id, int creationFrame)
                : type(type)
                , id(id)
                , creationFrame(creationFrame)
                , completionFrame(creationFrame + UnitUtil::BuildTime(type))
                , deathFrame(INT_MAX)
        {}
    };

    typedef std::multiset<std::shared_ptr<EcoModelUnit>, EcoModelUnit::cmp> EcoModelUnitSet;

    bool isEnabled;
    unsigned int workerLimit;

    EcoModelUnitSet observedUnits;
    EcoModelUnitSet impliedUnits;

    std::unordered_map<int, std::shared_ptr<EcoModelUnit>> observedUnitsById;
    std::map<BWAPI::UnitType, EcoModelUnitSet> aliveUnitsByType;

    std::map<BWAPI::UnitType, int> firstOfTypeCreated;

    std::vector<std::pair<UpgradeOrTechType, int>> research;

    std::vector<int> minerals;
    std::vector<int> gas;
    std::vector<int> supplyAvailable;

    bool ignoreUnit(BWAPI::UnitType type)
    {
        // We currently ignore workers and supply providers, just assuming they are built as needed
        return type.isWorker() || type.supplyProvided() > 0;
    }

    void simulateIncome()
    {
        // Assumptions:
        // - Workers are built constantly
        // - Pylon is built at 8 to support worker production
        // - Scout is sent at 10 supply
        // - Three workers are moved to gas when an assimilator finishes
        // - Enemy stops building workers once they have saturated their resources

        // This simulation does not take later pylon building into account after the one at 8 supply, so it will initialize
        // supplyAvailable to negative numbers past a certain point, which will be accounted for later

        // We start mineral collection at frame 25 to account for the initial delay in getting the economy started

        // At frame 0 we have 4 mineral workers, no gas workers, and are building a worker that started after LF
        int mineralWorkers = 4;
        int gasWorkers = 0;
        int remainingWorkerBuildTime = 300 + BWAPI::Broodwar->getLatencyFrames();

        // Find the frames where a refinery finished
        std::queue<int> refineryFrames;
        for (const auto &unit : observedUnits)
        {
            if (unit->type.isRefinery())
            {
                refineryFrames.push(unit->completionFrame);
            }
        }
        refineryFrames.push(INT_MAX);

        double mineralCounter = 0.0;
        double gasCounter = 0.0;
        int supplyCounter = 8;
        for (int f = 25; f < MODEL_FRAME_LIMIT; f++)
        {
            // If a refinery finished at this frame, move workers from minerals to gas
            if (f == refineryFrames.front())
            {
                mineralWorkers -= 3;
                gasWorkers += 3;
                refineryFrames.pop();
            }

            // Completed worker goes to minerals
            if (remainingWorkerBuildTime == 0)
            {
                mineralWorkers++;
            }

            // Build a worker
            if (remainingWorkerBuildTime <= 0 && mineralCounter >= 50 && (mineralWorkers + gasWorkers) < workerLimit)
            {
                mineralCounter -= 50;
                remainingWorkerBuildTime = 300;
                supplyCounter -= 2;
            }
            remainingWorkerBuildTime--;

            // Build a pylon at frame 1115
            if (f == 1115)
            {
                mineralCounter -= 100;
            }
            if (f == 1625)
            {
                supplyCounter += 16;
            }

            // Send scout at frame 1850
            if (f == 1850)
            {
                mineralWorkers--;
            }

            // Scout dies at frame 4500
            if (f == 4500)
            {
                supplyCounter += 2;
            }

            // Set the resources at this frame
            mineralCounter += mineralWorkers * MINERALS_PER_WORKER_FRAME;
            gasCounter += gasWorkers * GAS_PER_WORKER_FRAME;
            minerals[f] = (int)mineralCounter;
            gas[f] = (int)gasCounter;
            supplyAvailable[f] = supplyCounter;

            if (f == currentFrame)
            {
                CherryVis::setBoardValue("modelled-workers", (std::ostringstream() << mineralWorkers << ":" << gasWorkers).str());
            }
        }
    }

    void spendResource(std::vector<int> &resource, int amount, int fromFrame)
    {
        if (amount == 0) return;

        for (int f = fromFrame; f < MODEL_FRAME_LIMIT; f++)
        {
            resource[f] -= amount;
        }
    }

    int getPrerequisites(std::vector<std::pair<int, BWAPI::UnitType>> &prerequisites, BWAPI::UnitType type, int frame = 0);

    int addPrerequiste(std::vector<std::pair<int, BWAPI::UnitType>> &prerequisites, BWAPI::UnitType type, int frame)
    {
        auto it = aliveUnitsByType.find(type);
        if (it != aliveUnitsByType.end() && !it->second.empty())
        {
            return (*it->second.begin())->completionFrame;
        }

        int startFrame = frame - UnitUtil::BuildTime(type);
        prerequisites.emplace_back(std::make_pair(startFrame, type));

        return getPrerequisites(prerequisites, type, startFrame);
    }

    int getPrerequisites(std::vector<std::pair<int, BWAPI::UnitType>> &prerequisites, BWAPI::UnitType type, int frame)
    {
        int earliestFrame = 0;
        for (auto typeAndCount : type.requiredUnits())
        {
            if (!typeAndCount.first.isBuilding()) continue;
            if (typeAndCount.first.isResourceDepot()) continue;

            earliestFrame = std::max(earliestFrame, addPrerequiste(prerequisites, typeAndCount.first, frame));
        }

        // For units, always include what builds them in case it isn't a part of the dependency tree
        if (!type.isBuilding())
        {
            earliestFrame = std::max(earliestFrame, addPrerequiste(prerequisites, type.whatBuilds().first, frame));
        }

        return earliestFrame;
    }

    void removeDuplicates(std::vector<std::pair<int, BWAPI::UnitType>> &prerequisites)
    {
        // If there are needed buildings, remove duplicates
        if (prerequisites.empty()) return;

        // Start by sorting to get them in the right order
        std::sort(prerequisites.begin(), prerequisites.end());

        // Now remove duplicates
        std::set<BWAPI::UnitType> seenTypes;
        for (auto it = prerequisites.begin(); it != prerequisites.end(); )
        {
            if (seenTypes.find(it->second) != seenTypes.end())
            {
                it = prerequisites.erase(it);
            }
            else
            {
                seenTypes.insert(it->second);
                it++;
            }
        }
    }
}

namespace OpponentEconomicModel
{
    bool enabled(int frame)
    {
        return isEnabled && frame < MODEL_FRAME_LIMIT;
    }

    void initialize()
    {
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran ||
            BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
        {
            isEnabled = false;
            return;
        }

        isEnabled = true;
        workerLimit = Map::getMyMain()->mineralPatchCount() * 2 + Map::getMyMain()->geyserCount() * 3;
        observedUnitsById.clear();
        observedUnits.clear();
        aliveUnitsByType.clear();
        firstOfTypeCreated.clear();
        research.clear();
        minerals.assign(MODEL_FRAME_LIMIT, 0);
        gas.assign(MODEL_FRAME_LIMIT, 0);
        supplyAvailable.assign(MODEL_FRAME_LIMIT, 8);
    }

    void update()
    {
        if (!isEnabled) return;

        if (currentFrame >= 15000 ||
            BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran ||
            BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg ||
            Map::getEnemyBases().size() > 1)
        {
            isEnabled = false;
            return;
        }

        // Start by initializing with the opponent's income
        simulateIncome();

        // Compute units we haven't seen that are prerequisites to what we have seen
        impliedUnits.clear();
        std::vector<std::pair<int, BWAPI::UnitType>> prerequisites;
        for (auto &typeCreated : firstOfTypeCreated)
        {
            getPrerequisites(prerequisites, typeCreated.first, typeCreated.second);
        }
        for (const auto &researchItem : research)
        {
            getPrerequisites(prerequisites, researchItem.first.whatsRequired(), researchItem.second);
            getPrerequisites(prerequisites, researchItem.first.whatUpgradesOrResearches(), researchItem.second);
        }
        removeDuplicates(prerequisites);
        for (auto &prerequisite : prerequisites)
        {
            if (prerequisite.second < 0)
            {
                Log::Get() << "ERROR: Prerequisite " << prerequisite.first << " would need to have been built at frame " << prerequisite.second
                    << "; assuming this is a test and disabling econ model";
                isEnabled = false;
                return;
            }

            impliedUnits.emplace(std::make_shared<EcoModelUnit>(prerequisite.first, 0, prerequisite.second));
        }

        // Simulate resource spend for all units
        auto spend = [&](const std::shared_ptr<EcoModelUnit> &unit)
        {
            spendResource(minerals, unit->type.mineralPrice(), unit->creationFrame);
            spendResource(gas, unit->type.gasPrice(), unit->creationFrame);
            spendResource(supplyAvailable, unit->type.supplyRequired(), unit->creationFrame);
            if (unit->deathFrame < MODEL_FRAME_LIMIT)
            {
                spendResource(supplyAvailable, -unit->type.supplyRequired(), unit->deathFrame);

                // Refund cancelled buildings
                if (unit->type.isBuilding() && unit->completionFrame > unit->deathFrame)
                {
                    spendResource(minerals, (unit->type.mineralPrice() * -3) / 4, unit->deathFrame);
                    spendResource(gas, (unit->type.gasPrice() * -3) / 4, unit->deathFrame);
                }
            }
        };
        for (const auto &unit : observedUnits)
        {
            spend(unit);
        }
        for (const auto &unit : impliedUnits)
        {
            spend(unit);
        }

        // Simulate resource spend for upgrades
        for (const auto &researchItem : research)
        {
            spendResource(minerals, researchItem.first.mineralPrice(), researchItem.second);
            spendResource(gas, researchItem.first.gasPrice(), researchItem.second);
        }

        // Now insert supply wherever needed
        int pylons = 1;
        for (int f = 0; f < MODEL_FRAME_LIMIT; f++)
        {
            if (supplyAvailable[f] < 0)
            {
                spendResource(minerals, 100, f - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon));
                spendResource(supplyAvailable, -16, f);
                pylons++;
            }
        }

        // TODO: Simulate production facilities

        CherryVis::setBoardValue("modelled-minerals", (std::ostringstream() << minerals[currentFrame]).str());
        CherryVis::setBoardValue("modelled-gas", (std::ostringstream() << gas[currentFrame]).str());
        CherryVis::setBoardValue("modelled-supply", (std::ostringstream() << supplyAvailable[currentFrame]).str());
        CherryVis::setBoardValue("modelled-pylons", (std::ostringstream() << pylons).str());
        std::ostringstream unitCounts;
        for (auto &typeAndUnits : aliveUnitsByType)
        {
            unitCounts << typeAndUnits.second.size() << " " << typeAndUnits.first << "\n";
        }
        CherryVis::setBoardValue("modelled-alive", unitCounts.str());
        std::ostringstream cvisImplied;
        for (auto &unit : impliedUnits)
        {
            unitCounts << unit->type << "@" << unit->creationFrame << "\n";
        }
        CherryVis::setBoardValue("modelled-implied", cvisImplied.str());
    }

    void opponentUnitCreated(BWAPI::UnitType type, int id, int estimatedCreationFrame)
    {
        if (!isEnabled) return;
        if (ignoreUnit(type)) return;

        auto observedUnit = std::make_shared<EcoModelUnit>(type, id, estimatedCreationFrame);
        observedUnitsById[id] = observedUnit;
        observedUnits.insert(observedUnit);
        aliveUnitsByType[type].insert(observedUnit);
        if (firstOfTypeCreated.find(type) == firstOfTypeCreated.end())
        {
            firstOfTypeCreated[type] = estimatedCreationFrame;
        }
        else
        {
            firstOfTypeCreated[type] = std::min(firstOfTypeCreated[type], estimatedCreationFrame);
        }
    }

    void opponentUnitDestroyed(BWAPI::UnitType type, int id)
    {
        if (!isEnabled) return;
        if (ignoreUnit(type)) return;

        auto it = observedUnitsById.find(id);
        if (it != observedUnitsById.end())
        {
            it->second->deathFrame = currentFrame;
            aliveUnitsByType[type].erase(it->second);

            return;
        }

        // We don't expect to see a unit die that hasn't already been observed, but handle it gracefully
        Log::Get() << "Non-observed unit died: " << type << "#" << id;
        auto observedUnit = std::make_shared<EcoModelUnit>(type, id, currentFrame - UnitUtil::BuildTime(type));
        observedUnit->deathFrame = currentFrame;
        observedUnitsById[id] = observedUnit;
        observedUnits.insert(observedUnit);
    }

    void opponentResearched(UpgradeOrTechType type)
    {
        if (!isEnabled) return;

        research.emplace_back(std::make_pair(type, currentFrame - type.upgradeOrResearchTime()));
    }

    int worstCaseUnitCount(BWAPI::UnitType type, int frame)
    {
        if (!isEnabled)
        {
            Log::Get() << "ERROR: Trying to use opponent economic model when it is not enabled";
            return 0;
        }

        if (frame == -1) frame = currentFrame;

        return -1;
    }

    int minimumProducerCount(BWAPI::UnitType producer)
    {
        if (!isEnabled)
        {
            Log::Get() << "ERROR: Trying to use opponent economic model when it is not enabled";
            return 0;
        }

        return -1;
    }

    // The earliest frame the enemy could start building the given unit type
    int earliestUnitProductionFrame(BWAPI::UnitType type)
    {
        if (!isEnabled)
        {
            Log::Get() << "ERROR: Trying to use opponent economic model when it is not enabled";
            return 0;
        }

        // Get a list of the buildings we need at relative frame offset from building the given unit type
        std::vector<std::pair<int, BWAPI::UnitType>> prerequisites;
        int startFrame = getPrerequisites(prerequisites, type);
        removeDuplicates(prerequisites);

        // Now figure out the earliest frames we could produce everything
        int f;

        // Simple case: no prerequisites
        if (prerequisites.empty())
        {
            // Find the first frame scanning backwards where we don't have enough of the resource, then advance one
            for (f = MODEL_FRAME_LIMIT - 1; f >= startFrame; f--)
            {
                if (minerals[f] < type.mineralPrice()) break;
                if (gas[f] < type.gasPrice()) break;
            }

            return f + 1;
        }

        // Get the total resource cost of the needed buildings
        int totalMineralCost = 0;
        int totalGasCost = 0;
        for (auto &neededBuilding : prerequisites)
        {
            totalMineralCost += neededBuilding.second.mineralPrice();
            totalGasCost += neededBuilding.second.gasPrice();
        }

        // Compute the frame stops and how much of the resource we need at each one
        std::vector<std::tuple<int, int, int>> frameStopsAndResourcesNeeded;
        frameStopsAndResourcesNeeded.emplace_back(0, totalMineralCost + type.mineralPrice(), totalGasCost + type.gasPrice());
        for (auto it = prerequisites.rbegin(); it != prerequisites.rend(); it++)
        {
            frameStopsAndResourcesNeeded.emplace_back(it->first, totalMineralCost, totalGasCost);
            totalMineralCost -= it->second.mineralPrice();
            totalGasCost -= it->second.gasPrice();
        }

        // Find the frame where we can meet resource requirements at all frame stops
        for (f = MODEL_FRAME_LIMIT - 1; f >= startFrame; f--)
        {
            for (auto &frameStopAndResourcesNeeded : frameStopsAndResourcesNeeded)
            {
                int frame = f - std::get<0>(frameStopAndResourcesNeeded);
                if (frame >= MODEL_FRAME_LIMIT) continue;
                if (frame < 0
                    || minerals[frame] < std::get<1>(frameStopAndResourcesNeeded)
                    || gas[frame] < std::get<2>(frameStopAndResourcesNeeded))
                {
                    return f + 1;
                }
            }
        }
        return f + 1;
    }
}
