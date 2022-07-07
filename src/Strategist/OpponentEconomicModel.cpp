#include "OpponentEconomicModel.h"

#include "Map.h"
#include "UnitUtil.h"
#include "UpgradeOrTechType.h"
#include "Units.h"

#define MODEL_FRAME_LIMIT 20000

#if INSTRUMENTATION_ENABLED
#define OUTPUT_BOARD_VALUES true
#define OUTPUT_BUILD_ORDER true
#endif

namespace OpponentEconomicModel
{
    namespace
    {
        const double MINERALS_PER_WORKER_FRAME = 0.0465;
        const double GAS_PER_WORKER_FRAME = 0.071;

        // Assume a builder misses one mining run until frame 5500
        const int BUILDER_LOSS = 8;
        const int BUILDER_LOSS_UNTIL = 5500;

        // We simulate the early income using an assumed normal build
        const int PYLON_STARTED = 1115;
        const int PYLON_FINISHED = 1625;
        const int SCOUT_SENT = 1850;
        const int SCOUT_DIED = 4500;

        // When guessing at gas timings, use this frame as the earliest frame
        const int EARLIEST_GAS = 2500;

#if OUTPUT_BUILD_ORDER
        std::ostringstream observations;
#endif

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
            struct cmpByShiftedCreationFrame
            {
                bool operator()(const std::shared_ptr<EcoModelUnit> &a, const std::shared_ptr<EcoModelUnit> &b) const
                {
                    return a->shiftedCreationFrame < b->shiftedCreationFrame
                           || (b->shiftedCreationFrame >= a->shiftedCreationFrame && a->completionFrame < b->completionFrame)
                           || (b->shiftedCreationFrame >= a->shiftedCreationFrame && b->completionFrame >= a->completionFrame && a < b);
                }
            };

            // These fields are from observations
            BWAPI::UnitType type;
            int id;
            int creationFrame;
            int completionFrame;
            int deathFrame;
            bool creationFrameKnown; // If true, we know exactly when the unit was created

            // These fields are transient and used while simulating production 
            bool hasProduced;
            int canProduceAt;
            int prerequisitesAvailableFrame;
            int shiftedCreationFrame;

            EcoModelUnit(BWAPI::UnitType type, int id, int creationFrame, bool creationFrameKnown = false)
                    : type(type)
                    , id(id)
                    , creationFrame(creationFrame)
                    , completionFrame(creationFrame + UnitUtil::BuildTime(type))
                    , deathFrame(MODEL_FRAME_LIMIT + 1)
                    , creationFrameKnown(creationFrameKnown)
                    , hasProduced(false)
                    , canProduceAt(completionFrame)
                    , prerequisitesAvailableFrame(creationFrame)
                    , shiftedCreationFrame(creationFrame)
            {}
        };

        typedef std::multiset<std::shared_ptr<EcoModelUnit>, EcoModelUnit::cmp> EcoModelUnitSet;
        typedef std::multiset<std::shared_ptr<EcoModelUnit>, EcoModelUnit::cmpByShiftedCreationFrame> EcoModelUnitSetByShiftedCreationFrame;

        bool isEnabled;
        unsigned int workerLimit;

        bool changedThisFrame;

        std::map<BWAPI::UpgradeType, int> currentUpgradeLevels;

        EcoModelUnitSet observedUnits;
        EcoModelUnitSet impliedUnits;
        std::vector<int> _pylonFrames;

        std::unordered_map<int, std::shared_ptr<EcoModelUnit>> observedUnitsById;
        std::map<BWAPI::UnitType, EcoModelUnitSet> observedUnitsByType;
        std::map<BWAPI::UnitType, std::vector<std::shared_ptr<EcoModelUnit>>> producersByType;

        std::map<BWAPI::UnitType, int> firstOfTypeCreated;

        std::vector<std::pair<UpgradeOrTechType, int>> research;

        typedef std::array<int, MODEL_FRAME_LIMIT> ResourceArray;

        // These are the main arrays representing our observed state
        ResourceArray _minerals;
        ResourceArray _gas;
        ResourceArray _supplyAvailable;

        // These are alternate resource arrays for the early game when we haven't seen the enemy take gas yet
        bool gasTakenSimulated;
        ResourceArray _mineralsWithTakenGas;
        ResourceArray _gasWithTakenGas;
        ResourceArray _supplyAvailableWithTakenGas;

        // Caches for the expensive lookup methods
        std::map<std::pair<BWAPI::UnitType, int>, std::pair<int, int>> worstCaseUnitCountCache;
        std::map<BWAPI::UnitType, int> earliestUnitProductionFrameCache;

        bool ignoreUnit(BWAPI::UnitType type)
        {
            // We currently ignore workers and supply providers, just assuming they are built as needed
            return type.isWorker() || type.supplyProvided() > 0;
        }

        void simulateIncome(ResourceArray &minerals, ResourceArray &gas, ResourceArray &supplyAvailable, int overrideGasTiming = -1)
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
            if (overrideGasTiming > 0)
            {
                refineryFrames.push(overrideGasTiming + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Assimilator));
            }
            else
            {
                for (const auto &unit : observedUnits)
                {
                    if (unit->type.isRefinery())
                    {
                        refineryFrames.push(unit->completionFrame);
                    }
                }
            }
            refineryFrames.push(MODEL_FRAME_LIMIT + 1);

            std::priority_queue<int, std::vector<int>, std::greater<>> moveToGasFrames;
            moveToGasFrames.push(MODEL_FRAME_LIMIT + 1);

            double mineralCounter = 0.0;
            double gasCounter = 0.0;
            int supplyCounter = 8;
            for (int f = 0; f < MODEL_FRAME_LIMIT; f++)
            {
                // If a refinery finished at this frame, move workers from minerals to gas
                // They take 24 frames to switch
                if (f == refineryFrames.front())
                {
                    mineralWorkers -= 3;
                    moveToGasFrames.push(f + 24);
                    refineryFrames.pop();
                }
                if (f == moveToGasFrames.top())
                {
                    gasWorkers += 3;
                    moveToGasFrames.pop();
                }

                // Completed worker goes to minerals
                if (remainingWorkerBuildTime == 0)
                {
                    mineralWorkers++;
                }

                // Build a worker
                if (remainingWorkerBuildTime <= 0 && mineralCounter >= 50
                    && (mineralWorkers + gasWorkers + (moveToGasFrames.size() - 1) * 3) < workerLimit
                    && (f >= PYLON_FINISHED || supplyCounter >= 2))
                {
                    mineralCounter -= 50;
                    remainingWorkerBuildTime = 300;
                    supplyCounter -= 2;
                }
                remainingWorkerBuildTime--;

                // Build a pylon at frame 1115
                if (f == PYLON_STARTED)
                {
                    mineralCounter -= (100 + BUILDER_LOSS);
                }
                if (f == PYLON_FINISHED)
                {
                    supplyCounter += 16;
                }

                // Send scout at frame 1850
                if (f == SCOUT_SENT)
                {
                    mineralWorkers--;
                }

                // Scout dies at frame 4500
                if (f == SCOUT_DIED)
                {
                    supplyCounter += 2;
                }

                // Set the resources at this frame
                // Simulate a short delay to get going at the start
                if (f > 25)
                {
                    mineralCounter += mineralWorkers * MINERALS_PER_WORKER_FRAME;
                    gasCounter += gasWorkers * GAS_PER_WORKER_FRAME;
                }
                minerals[f] = (int)mineralCounter;
                gas[f] = (int)gasCounter;
                supplyAvailable[f] = supplyCounter;
            }
        }

        void spendResource(ResourceArray &resource, int amount, int fromFrame, int toFrame = MODEL_FRAME_LIMIT)
        {
            if (amount == 0) return;

            for (int f = fromFrame; f < toFrame; f++)
            {
                resource[f] -= amount;
            }
        }

        std::tuple<ResourceArray&, ResourceArray&, ResourceArray&> simulateTakingGasIfNeeded(BWAPI::UnitType type)
        {
            // Use normal arrays if the type doesn't require gas or we've seen when the enemy took gas
            auto it = observedUnitsByType.find(BWAPI::UnitTypes::Protoss_Assimilator);
            if (it != observedUnitsByType.end() || type.gasPrice() < 2)
            {
                return std::tie(_minerals, _gas, _supplyAvailable);
            }

            // If we've already simulated gas being taken, return the current data
            if (gasTakenSimulated)
            {
                return std::tie(_mineralsWithTakenGas, _gasWithTakenGas, _supplyAvailableWithTakenGas);
            }
            gasTakenSimulated = true;

            // Use our scouting information to determine when the opponent could have taken their gas at the earliest
            int geyserLastScouted = EARLIEST_GAS;
            auto base = Map::getEnemyStartingMain();
            if (base)
            {
                for (auto &geyserTile : base->geyserLocations())
                {
                    geyserLastScouted = std::max(geyserLastScouted, Map::lastSeen(geyserTile));
                }
            }

            // Simulate the income with the different gas timing
            simulateIncome(_mineralsWithTakenGas, _gasWithTakenGas, _supplyAvailableWithTakenGas, geyserLastScouted);
            return std::tie(_mineralsWithTakenGas, _gasWithTakenGas, _supplyAvailableWithTakenGas);
        }

        int getPrerequisites(std::vector<std::pair<int, BWAPI::UnitType>> &prerequisites, BWAPI::UnitType type, int frame = 0);

        int addPrerequiste(std::vector<std::pair<int, BWAPI::UnitType>> &prerequisites, BWAPI::UnitType type, int frame)
        {
            if (type == BWAPI::UnitTypes::None) return 0;

            auto it = observedUnitsByType.find(type);
            if (it != observedUnitsByType.end())
            {
                for (auto &unit : it->second)
                {
                    if (frame < 0 || frame < unit->deathFrame)
                    {
                        return unit->completionFrame;
                    }
                }
            }

            int startFrame = frame - UnitUtil::BuildTime(type);
            prerequisites.emplace_back(std::make_pair(startFrame, type));

            return getPrerequisites(prerequisites, type, startFrame);
        }

        int getPrerequisites(std::vector<std::pair<int, BWAPI::UnitType>> &prerequisites, BWAPI::UnitType type, int frame)
        {
            if (type == BWAPI::UnitTypes::None) return 0;

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

    bool enabled(int frame)
    {
        return isEnabled && frame < MODEL_FRAME_LIMIT;
    }

    void initialize()
    {
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran ||
            BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
        {
            Log::Get() << "Disabling opponent economic model, as opponent is not Protoss";
            isEnabled = false;
            return;
        }

        Log::Get() << "Initializing opponent economic model";

#if OUTPUT_BUILD_ORDER
        observations = std::ostringstream();
#endif

        isEnabled = true;
        workerLimit = Map::getMyMain()->mineralPatchCount() * 2 + Map::getMyMain()->geyserCount() * 3;
        changedThisFrame = true;
        observedUnitsById.clear();
        observedUnits.clear();
        observedUnitsByType.clear();
        firstOfTypeCreated.clear();
        research.clear();

        currentUpgradeLevels.clear();
        for (const auto &type : BWAPI::UpgradeTypes::allUpgradeTypes())
        {
            if (type.getRace() != BWAPI::Races::Protoss) continue;
            currentUpgradeLevels[type] = 0;
        }
    }

    void update()
    {
        if (!isEnabled) return;

        if (currentFrame >= (MODEL_FRAME_LIMIT - 5000))
        {
            Log::Get() << "Disabling opponent economic model, reached frame limit";
            isEnabled = false;
            return;
        }
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran ||
            BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
        {
            Log::Get() << "Disabling opponent economic model, as opponent is not Protoss";
            isEnabled = false;
            return;
        }
        auto &enemyNexuses = Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Nexus);
        if (std::count_if(enemyNexuses.begin(), enemyNexuses.end(), [](const Unit &nexus) { return nexus->completed; }) > 1)
        {
            Log::Get() << "Disabling opponent economic model, as opponent has a completed natural nexus";
            isEnabled = false;
            return;
        }

        // Detect upgrades
        for (auto &[type, currentLevel] : currentUpgradeLevels)
        {
            int level = BWAPI::Broodwar->enemy()->getUpgradeLevel(type);
            if (level > currentLevel)
            {
#if INSTRUMENTATION_ENABLED
                Log::Get() << "Enemy has upgraded " << type << " to " << level;
#endif
                currentLevel = level;
                research.emplace_back(std::make_pair(type, currentFrame - type.upgradeTime(level)));
                changedThisFrame = true;

#if OUTPUT_BUILD_ORDER
                observations << "\nOpponentEconomicModel::opponentUpgraded(BWAPI::UpgradeTypes::"
                    << type << ", " << level << ", " << (currentFrame - type.upgradeTime(level)) << ");";
#endif
            }
        }

        auto &minerals = _minerals;
        auto &gas = _gas;
        auto &supplyAvailable = _supplyAvailable;
        auto &pylonFrames = _pylonFrames;

#if OUTPUT_BOARD_VALUES
        auto outputBoardValues = [&]()
        {
            CherryVis::setBoardValue("modelled-minerals", (std::ostringstream() << minerals[currentFrame]).str());
            CherryVis::setBoardValue("modelled-gas", (std::ostringstream() << gas[currentFrame]).str());
            CherryVis::setBoardValue("modelled-supply", (std::ostringstream() << supplyAvailable[currentFrame]).str());
            CherryVis::setBoardValue("modelled-pylons", (std::ostringstream() << pylonFrames.size()).str());
            std::ostringstream unitCounts;
            for (auto &[type, unitSet] : observedUnitsByType)
            {
                unitCounts << unitSet.size() << " " << type << "\n";
            }
            CherryVis::setBoardValue("modelled-unitcounts", unitCounts.str());
            std::ostringstream cvisImplied;
            for (auto &unit : impliedUnits)
            {
                cvisImplied << unit->type << "@" << unit->creationFrame << "\n";
            }
            CherryVis::setBoardValue("modelled-implied", cvisImplied.str());
            std::ostringstream cvisProducerCounts;
            for (auto &[type, producers] : producersByType)
            {
                cvisProducerCounts << producers.size() << " " << type << "\n";
            }
            CherryVis::setBoardValue("modelled-producers", cvisProducerCounts.str());
        };
#endif

        if (!changedThisFrame)
        {
#if OUTPUT_BOARD_VALUES
            outputBoardValues();
#endif
            return;
        }
        changedThisFrame = false;
        gasTakenSimulated = false;
        worstCaseUnitCountCache.clear();
        earliestUnitProductionFrameCache.clear();

        // Start by initializing with the opponent's income
        simulateIncome(minerals, gas, supplyAvailable);

        // Compute units we haven't seen that are prerequisites to what we have seen
        impliedUnits.clear();
        std::vector<std::pair<int, BWAPI::UnitType>> prerequisites;
        for (auto &[type, frameFirstCreated] : firstOfTypeCreated)
        {
            getPrerequisites(prerequisites, type, frameFirstCreated);
        }
        for (const auto &[upgradeOrTechType, frame] : research)
        {
            getPrerequisites(prerequisites, upgradeOrTechType.whatsRequired(), frame);
            getPrerequisites(prerequisites, upgradeOrTechType.whatUpgradesOrResearches(), frame);
        }
        removeDuplicates(prerequisites);
        for (auto &[frame, type] : prerequisites)
        {
            if (frame < 0)
            {
                Log::Get() << "ERROR: Prerequisite " << type << " would need to have been built at frame " << frame
                    << "; assuming this is a test and disabling econ model";
                isEnabled = false;
                return;
            }

            auto impliedUnit = std::make_shared<EcoModelUnit>(type, 0, frame);
            impliedUnits.insert(impliedUnit);
        }

        EcoModelUnitSet allUnits;
        allUnits.insert(observedUnits.begin(), observedUnits.end());
        allUnits.insert(impliedUnits.begin(), impliedUnits.end());

        // Simulate resource spend for all units
        auto spend = [&](const std::shared_ptr<EcoModelUnit> &unit)
        {
            spendResource(
                    minerals,
                    (unit->type.mineralPrice() + ((unit->type.isBuilding() && unit->creationFrame < BUILDER_LOSS_UNTIL) ? BUILDER_LOSS : 0)),
                    unit->creationFrame);
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
        for (const auto &unit : allUnits)
        {
            spend(unit);
        }

        // Simulate resource spend for upgrades
        // Only considers level 1, which is probably fine given the constraints of the model
        for (const auto &researchItem : research)
        {
            spendResource(minerals, researchItem.first.mineralPrice(), researchItem.second);
            spendResource(gas, researchItem.first.gasPrice(), researchItem.second);
        }

        // Insert supply wherever needed
        // Keep track of the frames, as we will potentially manipulate this later when simulating production
        pylonFrames = {PYLON_STARTED};
        for (int f = PYLON_FINISHED; f < MODEL_FRAME_LIMIT; f++)
        {
            if (supplyAvailable[f] < 0)
            {
                int startFrame = f - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);
                spendResource(minerals, 100 + ((startFrame < BUILDER_LOSS_UNTIL) ? BUILDER_LOSS : 0), startFrame);
                spendResource(supplyAvailable, -16, f);
                pylonFrames.push_back(startFrame);
            }
        }

        // Now simulate production

        // Do an initial scan to initialize the data structures we will use for the search
        // Since the prerequisite availability frame is the same for all units of the same type, keep a cache of what we have computed
        // Prefill with gateway and zealot since they are given by our assumed early opening
        std::map<BWAPI::UnitType, int> prerequisitesAvailableByType = {
                {BWAPI::UnitTypes::Protoss_Gateway, PYLON_FINISHED},
                {BWAPI::UnitTypes::Protoss_Zealot, PYLON_FINISHED + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Gateway)}
        };
        producersByType.clear();
        std::map<BWAPI::UnitType, EcoModelUnitSet> unitsByProducerType;

        for (const auto &unit : allUnits)
        {
            // Determine when this unit type can be started
            if (prerequisitesAvailableByType.find(unit->type) == prerequisitesAvailableByType.end())
            {
                if (unit->creationFrameKnown)
                {
                    prerequisitesAvailableByType[unit->type] = unit->creationFrame;
                }
                else
                {
                    int prerequisitesDone = 0;
                    for (auto &[type, count] : unit->type.requiredUnits())
                    {
                        if (!type.isBuilding()) continue;

                        prerequisitesDone = std::max(prerequisitesDone, prerequisitesAvailableByType[type] + UnitUtil::BuildTime(type));
                    }

                    // Check when we actually had the resources
                    int f;
                    for (f = unit->creationFrame - 1; f >= prerequisitesDone; f--)
                    {
                        if (minerals[f] < unit->type.mineralPrice()) break;
                        if (gas[f] < unit->type.gasPrice()) break;
                    }
                    f++;

                    prerequisitesAvailableByType[unit->type] = f;
                }
            }

            // No longer need to consider tech buildings
            if (unit->type.isBuilding() && !unit->type.canProduce()) continue;

            // Add to an appropriate set
            unit->prerequisitesAvailableFrame = prerequisitesAvailableByType[unit->type];
            if (unit->type.canProduce())
            {
                producersByType[unit->type].push_back(unit);
            }
            else
            {
                unitsByProducerType[unit->type.whatBuilds().first].insert(unit);
            }
        }

        // Now search for a solution for each producer type giving the fewest number of producers
        // This is a tricky problem because we usually don't know the exact creation frames of what we have observed
        // For now we are assuming that units were created in the order that we observed them
        // This might cause issues though, as we might see a zealot after a dragoon that was produced before the core was finished
        // Another option could be to track "holes" in producer production that we can fill with later units

        for (auto &[producerType, units] : unitsByProducerType)
        {
            // Back up our state so we can roll back whenever we need to add a new implied producer
            auto mineralBackup = minerals;
            auto gasBackup = gas;
            auto supplyBackup = supplyAvailable;
            auto pylonFramesBackup = pylonFrames;

            while (true)
            {
                auto &producers = producersByType[producerType];

                // Guard against an endless loop
                if (producers.size() > 30)
                {
                    Log::Get() << "ERROR: Endless loop in opponent economic model; disabling economic model";
#if OUTPUT_BUILD_ORDER
                    Log::Get() << "Observations:" << observations.str();
#endif
                    isEnabled = false;
                    return;
                }

                for (auto &producer : producers)
                {
                    producer->hasProduced = false;
                    producer->canProduceAt = producer->completionFrame;
                    producer->shiftedCreationFrame = producer->creationFrame;
                }
                std::sort(producers.begin(), producers.end(), EcoModelUnit::cmp());
                if (producers.empty()) break; // Shouldn't happen, since each unit needs its prerequisites

                for (auto &unit : units)
                {
                    // Reset shifted creation frame as it might have been changed in a previous iteration
                    unit->shiftedCreationFrame = unit->creationFrame;

                    auto shiftEarlier = [&](int toFrame)
                    {
                        if (toFrame >= unit->shiftedCreationFrame) return;

                        spendResource(minerals, unit->type.mineralPrice(), toFrame, unit->shiftedCreationFrame);
                        spendResource(gas, unit->type.gasPrice(), toFrame, unit->shiftedCreationFrame);
                        spendResource(supplyAvailable, unit->type.supplyRequired(), toFrame, unit->shiftedCreationFrame);

                        unit->shiftedCreationFrame = toFrame;
                    };

                    // Shift earlier until hitting missing resources or supply
                    int firstSupplyBlockFrame = 0;
                    int lastSupplyBlockFrame = 0;
                    {
                        int f;
                        for (f = unit->creationFrame - 1; f >= unit->prerequisitesAvailableFrame; f--)
                        {
                            if (minerals[f] < unit->type.mineralPrice()) break;
                            if (gas[f] < unit->type.gasPrice()) break;
                            if (supplyAvailable[f] < unit->type.supplyRequired())
                            {
                                if (firstSupplyBlockFrame == 0) firstSupplyBlockFrame = f;
                                lastSupplyBlockFrame = f;
                            }
                        }

                        // Shift as far as we could
                        shiftEarlier(std::max(f + 1, firstSupplyBlockFrame + 1));
                    }

                    // Try to resolve a supply block
                    if (firstSupplyBlockFrame != 0)
                    {
                        int pylonBuildTime = UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);

                        // Find the earliest pylon that finishes on or after the unit's creation frame
                        int pylonIndex;
                        for (pylonIndex = 0; pylonIndex < pylonFrames.size(); pylonIndex++)
                        {
                            if ((pylonFrames[pylonIndex] + pylonBuildTime) >= unit->shiftedCreationFrame)
                            {
                                break;
                            }
                        }
                        if (pylonIndex < pylonFrames.size())
                        {
                            // Pull both the unit and the pylon back as far as possible
                            int earliestPylonFrame = std::max(unit->prerequisitesAvailableFrame, lastSupplyBlockFrame) - pylonBuildTime;
                            int pylonCost = 100;
                            if (earliestPylonFrame < BUILDER_LOSS_UNTIL) pylonCost += BUILDER_LOSS;

                            int nowMinerals = 0;
                            int nowGas = 0;
                            int offsetMinerals = pylonCost;
                            int f;
                            for (f = pylonFrames[pylonIndex] + pylonBuildTime; f >= unit->prerequisitesAvailableFrame; f--)
                            {
                                // When we cross the creation frame (or start there), begin considering the unit itself
                                if (f == unit->shiftedCreationFrame)
                                {
                                    nowMinerals = unit->type.mineralPrice() + pylonCost;
                                    nowGas = unit->type.gasPrice();
                                }

                                if (minerals[f] < nowMinerals) break;
                                if (minerals[f] < nowGas) break;
                                if (minerals[f - pylonBuildTime] < offsetMinerals) break;

                                // When we reach the beginning of the supply block, we no longer need to pull the pylon
                                if (f == lastSupplyBlockFrame)
                                {
                                    offsetMinerals = 0;
                                }
                                if (f == earliestPylonFrame)
                                {
                                    nowMinerals -= pylonCost;
                                }
                            }
                            f++;

                            // Shift the pylon
                            if (f < unit->shiftedCreationFrame)
                            {
                                int newStartFrame = std::max(f - pylonBuildTime, earliestPylonFrame);
                                spendResource(minerals, pylonCost, newStartFrame, pylonFrames[pylonIndex]);
                                spendResource(supplyAvailable, -16, newStartFrame + pylonBuildTime, pylonFrames[pylonIndex] + pylonBuildTime);
                                pylonFrames[pylonIndex] = newStartFrame;
                                std::sort(pylonFrames.begin(), pylonFrames.end());
                            }

                            shiftEarlier(f);
                        }
                        else
                        {
                            // There wasn't a pylon to shift, so try creating one instead
                            int earliestPylonFrame = unit->prerequisitesAvailableFrame - pylonBuildTime;
                            int mineralCost = 100;
                            if (earliestPylonFrame < BUILDER_LOSS_UNTIL) mineralCost += BUILDER_LOSS;
                            int gasCost = 0;
                            int f;
                            for (f = MODEL_FRAME_LIMIT - 1; f >= earliestPylonFrame; f--)
                            {
                                if (minerals[f] < mineralCost) break;
                                if (minerals[f] < gasCost) break;

                                if (f == unit->prerequisitesAvailableFrame)
                                {
                                    mineralCost -= unit->type.mineralPrice();
                                    gasCost -= unit->type.gasPrice();
                                }
                                if (f == unit->shiftedCreationFrame)
                                {
                                    mineralCost += unit->type.mineralPrice();
                                    gasCost += unit->type.gasPrice();
                                }
                            }
                            f++;
                            if (f < (unit->shiftedCreationFrame - pylonBuildTime))
                            {
                                pylonFrames.push_back(f);
                                std::sort(pylonFrames.begin(), pylonFrames.end());
                                spendResource(minerals, 100 + ((f < BUILDER_LOSS_UNTIL) ? BUILDER_LOSS : 0), f);
                                spendResource(supplyAvailable, -16, f);

                                shiftEarlier(f + pylonBuildTime);
                            }
                        }
                    }

                    // Find the earliest producer that can produce this unit
                    // For producers that haven't simulated producing anything yet, check if they can be moved earlier than one that has
                    std::shared_ptr<EcoModelUnit> earliestProducer = nullptr;
                    int earliestProducerFrame = MODEL_FRAME_LIMIT + 1;
                    bool attemptedProducerShift = false;
                    for (auto &producer : producers)
                    {
                        // If this producer can be moved earlier, try it
                        // Only do this for the first one though, as others are redundant
                        if (!producer->hasProduced && !producer->creationFrameKnown)
                        {
                            if (attemptedProducerShift) continue;
                            attemptedProducerShift = true;

                            // Find an earlier frame with enough resources
                            int earliestFrame = std::max(unit->shiftedCreationFrame, producer->prerequisitesAvailableFrame);
                            int mineralPrice = producer->type.mineralPrice() + ((earliestFrame < BUILDER_LOSS_UNTIL) ? BUILDER_LOSS : 0);
                            int f;
                            for (f = producer->creationFrame - 1; f >= earliestFrame; f--)
                            {
                                if (minerals[f] < mineralPrice) break;
                                if (gas[f] < producer->type.gasPrice()) break;
                            }
                            producer->canProduceAt = f + 1 + UnitUtil::BuildTime(producer->type);
                        }

                        if (producer->canProduceAt > unit->creationFrame) continue;
                        if (producer->canProduceAt > earliestProducerFrame) continue;

                        earliestProducerFrame = producer->canProduceAt;
                        earliestProducer = producer;
                    }
                    if (earliestProducer)
                    {
                        // Shift the producer earlier if needed
                        if (!earliestProducer->hasProduced && !earliestProducer->creationFrameKnown
                            && earliestProducer->canProduceAt < earliestProducer->completionFrame)
                        {
                            earliestProducer->shiftedCreationFrame = earliestProducer->canProduceAt - UnitUtil::BuildTime(earliestProducer->type);
                            int mineralPrice = earliestProducer->type.mineralPrice()
                                    + ((earliestProducer->shiftedCreationFrame < BUILDER_LOSS_UNTIL) ? BUILDER_LOSS : 0);
                            spendResource(minerals,
                                          mineralPrice,
                                          earliestProducer->shiftedCreationFrame,
                                          earliestProducer->creationFrame);
                            spendResource(gas,
                                          earliestProducer->type.gasPrice(),
                                          earliestProducer->shiftedCreationFrame,
                                          earliestProducer->creationFrame);
                        }

                        int productionFrame = std::max(earliestProducer->canProduceAt, unit->shiftedCreationFrame);

                        // Shift the unit later if the producer isn't available early enough
                        if (productionFrame > unit->shiftedCreationFrame)
                        {
                            spendResource(minerals, -unit->type.mineralPrice(), unit->shiftedCreationFrame, productionFrame);
                            spendResource(gas, -unit->type.gasPrice(), unit->shiftedCreationFrame, productionFrame);
                            spendResource(supplyAvailable, -unit->type.supplyRequired(), unit->shiftedCreationFrame, productionFrame);
                            unit->shiftedCreationFrame = productionFrame;
                        }

                        earliestProducer->canProduceAt = productionFrame + UnitUtil::BuildTime(unit->type);
                        earliestProducer->hasProduced = true;

                        continue;
                    }

                    // We have no producer for this unit
                    // Add a new one as early as possible

                    // Start by restoring our backed-up state
                    minerals = mineralBackup;
                    gas = gasBackup;
                    supplyAvailable = supplyBackup;
                    pylonFrames = pylonFramesBackup;

                    // Now find the first frame
                    int mineralPrice = producerType.mineralPrice() + ((unit->creationFrame < BUILDER_LOSS_UNTIL) ? BUILDER_LOSS : 0);
                    int f;
                    for (f = unit->creationFrame; f >= prerequisitesAvailableByType[producerType]; f--)
                    {
                        if (minerals[f] < mineralPrice) break;
                        if (gas[f] < producerType.gasPrice()) break;
                        if (f == BUILDER_LOSS_UNTIL) mineralPrice += BUILDER_LOSS;
                    }
                    f++;
                    if (f > (unit->creationFrame - UnitUtil::BuildTime(producerType)))
                    {
                        // The simulation couldn't figure it out, which probably means units were produced at very different frames compared
                        // to our observations
                        // It may also mean there has been some kind of proxy play that confuses the model
                        // We do know that the opponent must have had an additional production facility, so just add it as early as possible
                        // The resources won't match up, but it will at least give us a lower bound on how many producers the enemy has
                        f = std::min(prerequisitesAvailableByType[producerType], unit->creationFrame - UnitUtil::BuildTime(producerType));
                    }

                    auto producer = std::make_shared<EcoModelUnit>(producerType, 0, f);
                    impliedUnits.insert(producer);
                    allUnits.insert(producer);
                    producers.push_back(producer);
                    spend(producer);
                    goto nextIteration;
                }

                // All units were handled, or we found a situation we couldn't resolve, so break the loop
                break;

                nextIteration:;
            }
        }

#if OUTPUT_BOARD_VALUES
        outputBoardValues();
#endif
#if OUTPUT_BUILD_ORDER
        std::sort(research.begin(), research.end(), [](const auto& a, const auto& b)
        {
            return a.second > b.second;
        });

        EcoModelUnitSetByShiftedCreationFrame unitsByShiftedCreation;
        unitsByShiftedCreation.insert(observedUnits.begin(), observedUnits.end());
        unitsByShiftedCreation.insert(impliedUnits.begin(), impliedUnits.end());

        std::ostringstream buildOrder;
        auto pylon = 0;
        auto researchIndex = 0;
        for (auto &unit : unitsByShiftedCreation)
        {
            if (pylon < pylonFrames.size() && pylonFrames[pylon] < unit->shiftedCreationFrame)
            {
                buildOrder << "\n" << pylonFrames[pylon]
                           << " (" << minerals[pylonFrames[pylon]]
                           << "," << gas[pylonFrames[pylon]]
                           << "," << supplyAvailable[pylonFrames[pylon]]
                           << "): " << BWAPI::UnitTypes::Protoss_Pylon;
                pylon++;
            }
            if (researchIndex < research.size() && research[researchIndex].second < unit->shiftedCreationFrame)
            {
                buildOrder << "\n" << research[researchIndex].second
                           << " (" << minerals[research[researchIndex].second]
                           << "," << gas[research[researchIndex].second]
                           << "," << supplyAvailable[research[researchIndex].second]
                           << "): " << research[researchIndex].first;
                researchIndex++;
            }

            buildOrder << "\n" << unit->shiftedCreationFrame
                       << " (" << minerals[unit->shiftedCreationFrame]
                       << "," << gas[unit->shiftedCreationFrame]
                       << "," << supplyAvailable[unit->shiftedCreationFrame]
                       << "): " << unit->type
                       << " (" << unit->creationFrame << ")";
        }

        Log::Debug() << "\n\nObservations:" << observations.str()
                     << "\n\nBuild order:" << buildOrder.str()
                     << "\n";
#endif
    }

    void opponentUnitCreated(BWAPI::UnitType type, int id, int estimatedCreationFrame, bool creationFrameKnown)
    {
        if (!isEnabled) return;
        if (ignoreUnit(type)) return;

        changedThisFrame = true;

        auto observedUnit = std::make_shared<EcoModelUnit>(type, id, estimatedCreationFrame, creationFrameKnown);
        observedUnitsById[id] = observedUnit;
        observedUnits.insert(observedUnit);
        observedUnitsByType[type].insert(observedUnit);
        if (firstOfTypeCreated.find(type) == firstOfTypeCreated.end())
        {
            firstOfTypeCreated[type] = estimatedCreationFrame;
        }
        else
        {
            firstOfTypeCreated[type] = std::min(firstOfTypeCreated[type], estimatedCreationFrame);
        }

#if OUTPUT_BUILD_ORDER
        observations << "\nOpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::"
                     << type << ", " << id << ", " << estimatedCreationFrame << ", " << (creationFrameKnown ? "true" : "false") << ");";
#endif
    }

    void opponentUnitDestroyed(BWAPI::UnitType type, int id, int frameDestroyed)
    {
        if (!isEnabled) return;
        if (ignoreUnit(type)) return;

        changedThisFrame = true;

        int frame = frameDestroyed;
        if (frame == -1) frame = currentFrame;

#if OUTPUT_BUILD_ORDER
        observations << "\nOpponentEconomicModel::opponentUnitDestroyed(BWAPI::UnitTypes::"
                     << type << ", " << id << ", " << frame << ");";
#endif

        auto it = observedUnitsById.find(id);
        if (it != observedUnitsById.end())
        {
            it->second->deathFrame = frame;
            return;
        }

        // We don't expect to see a unit die that hasn't already been observed, but handle it gracefully
        Log::Get() << "Non-observed unit died: " << type << "#" << id;
        auto observedUnit = std::make_shared<EcoModelUnit>(type, id, frame - UnitUtil::BuildTime(type));
        observedUnit->deathFrame = frame;
        observedUnitsById[id] = observedUnit;
        observedUnits.insert(observedUnit);
    }

    void opponentResearched(BWAPI::TechType type, int frameStarted)
    {
        if (!isEnabled) return;

        changedThisFrame = true;

#if OUTPUT_BUILD_ORDER
        observations << "\nOpponentEconomicModel::opponentResearched(BWAPI::TechTypes::"
                     << type << ", " << frameStarted << ");";
#endif

        int startFrame = frameStarted;
        if (startFrame == -1) startFrame = currentFrame - type.researchTime();
        research.emplace_back(std::make_pair(type, startFrame));
    }

    void opponentUpgraded(BWAPI::UpgradeType type, int level, int frameStarted)
    {
        if (!isEnabled) return;

        changedThisFrame = true;

        currentUpgradeLevels[type] = level;
        research.emplace_back(std::make_pair(type, frameStarted));
    }

    std::pair<int, int> worstCaseUnitCount(BWAPI::UnitType type, int frame)
    {
        if (!isEnabled)
        {
            Log::Get() << "ERROR: Trying to use opponent economic model when it is not enabled";
            return std::make_pair(0, 0);
        }

        auto cacheIt = worstCaseUnitCountCache.find(std::make_pair(type, frame));
        if (cacheIt != worstCaseUnitCountCache.end())
        {
            return cacheIt->second;
        }

        if (frame == -1) frame = currentFrame;

        // Our simulation has already pulled all observed units as early as possible and simulated producers we know must have been there
        // So this just needs to first fill in additional production from the producers, then simulate if the enemy could have built
        // more producers

        // Start by counting how many of the unit type are alive at the given frame
        int currentCount = 0;
        for (auto &unit : observedUnits)
        {
            if (unit->type != type) continue;
            if (unit->shiftedCreationFrame > frame) continue;
            if (unit->deathFrame > frame) currentCount++;
        }

        auto createResult = [&](int additionalUnitCount)
        {
            auto result = std::make_pair(currentCount, currentCount + additionalUnitCount);
            worstCaseUnitCountCache[std::make_pair(type, frame)] = result;
            return result;
        };

        // Use our other method to figure out when the first additional unit of this type could be produced
        // This takes prerequisites into account
        // Jump out now if no further units can be produced before the given frame
        int earliestFrame = earliestUnitProductionFrame(type);
        if (earliestFrame > frame)
        {
            return createResult(0);
        }

        int additionalUnitCount = 0;

        BWAPI::UnitType producerType = type.whatBuilds().first;
        int buildTime = UnitUtil::BuildTime(type);
        int producerBuildTime = UnitUtil::BuildTime(producerType);

        // Take a copy of the resource arrays, since we might be changing them
        auto pylonFrames = _pylonFrames;
        auto resources = simulateTakingGasIfNeeded(type);
        ResourceArray minerals = std::get<0>(resources);
        ResourceArray gas = std::get<1>(resources);
        ResourceArray supplyAvailable = std::get<2>(resources);

        // Computes when the unit can next be produced at
        // Shifts pylons earlier if needed
        auto canProduceUnitAt = [&]()
        {
            int firstSupplyBlockFrame = 0;
            int lastSupplyBlockFrame = 0;
            int resourcesAvailableFrame;
            {
                int f;
                for (f = MODEL_FRAME_LIMIT - 1; f >= earliestFrame; f--)
                {
                    if (minerals[f] < type.mineralPrice()) break;
                    if (gas[f] < type.gasPrice()) break;
                    if (supplyAvailable[f] < type.supplyRequired())
                    {
                        if (firstSupplyBlockFrame == 0) firstSupplyBlockFrame = f;
                        lastSupplyBlockFrame = f;
                    }
                }
                resourcesAvailableFrame = f + 1;
            }

            if (firstSupplyBlockFrame == 0)
            {
                return resourcesAvailableFrame;
            }

            // Try to resolve a supply block
            int pylonBuildTime = UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);

            // Find the earliest pylon that finishes after the supply block frame
            int pylonIndex;
            for (pylonIndex = 0; pylonIndex < pylonFrames.size(); pylonIndex++)
            {
                if ((pylonFrames[pylonIndex] + pylonBuildTime) > firstSupplyBlockFrame)
                {
                    break;
                }
            }
            if (pylonIndex < pylonFrames.size())
            {
                // Pull the pylon back as far as possible, but ensuring we also have resources for the unit
                int earliestPylonFrame = std::max(resourcesAvailableFrame, lastSupplyBlockFrame) - pylonBuildTime;
                int pylonCost = 100;
                if (earliestPylonFrame < BUILDER_LOSS_UNTIL) pylonCost += BUILDER_LOSS;

                int nowMinerals = 0;
                int nowGas = 0;
                int offsetMinerals = pylonCost;
                int f;
                for (f = pylonFrames[pylonIndex] + pylonBuildTime; f >= resourcesAvailableFrame; f--)
                {
                    // When we cross the supply block frame (or start there), begin considering the unit itself
                    if (f == firstSupplyBlockFrame)
                    {
                        nowMinerals = type.mineralPrice() + pylonCost;
                        nowGas = type.gasPrice();
                    }

                    if (minerals[f] < nowMinerals) break;
                    if (minerals[f] < nowGas) break;
                    if (minerals[f - pylonBuildTime] < offsetMinerals) break;

                    // When we reach the beginning of the supply block, we no longer need to pull the pylon
                    if (f == lastSupplyBlockFrame)
                    {
                        offsetMinerals = 0;
                    }
                    if (f == earliestPylonFrame)
                    {
                        nowMinerals -= pylonCost;
                    }
                }
                f++;

                // Shift the pylon
                if (f < firstSupplyBlockFrame)
                {
                    int newStartFrame = std::max(f - pylonBuildTime, earliestPylonFrame);
                    spendResource(minerals, pylonCost, newStartFrame, pylonFrames[pylonIndex]);
                    spendResource(supplyAvailable, -16, newStartFrame + pylonBuildTime, pylonFrames[pylonIndex] + pylonBuildTime);
                    pylonFrames[pylonIndex] = newStartFrame;
                    std::sort(pylonFrames.begin(), pylonFrames.end());
                }

                return f;
            }

            // There wasn't a pylon to shift, so try creating one instead
            int earliestPylonFrame = resourcesAvailableFrame - pylonBuildTime;
            int mineralCost = 100 + type.mineralPrice();
            if (earliestPylonFrame < BUILDER_LOSS_UNTIL) mineralCost += BUILDER_LOSS;
            int gasCost = type.gasPrice();
            int f;
            for (f = MODEL_FRAME_LIMIT - 1; f >= earliestPylonFrame; f--)
            {
                if (minerals[f] < mineralCost) break;
                if (minerals[f] < gasCost) break;

                if (f == resourcesAvailableFrame)
                {
                    mineralCost -= type.mineralPrice();
                    gasCost -= type.gasPrice();
                }
            }
            f++;
            if (f < (firstSupplyBlockFrame - pylonBuildTime))
            {
                pylonFrames.push_back(f);
                std::sort(pylonFrames.begin(), pylonFrames.end());
                spendResource(minerals, 100 + ((f < BUILDER_LOSS_UNTIL) ? BUILDER_LOSS : 0), f);
                spendResource(supplyAvailable, -16, f);
            }
            return f + pylonBuildTime;
        };

        // Frames a producer can produce at
        std::priority_queue<int, std::vector<int>, std::greater<>> producerFrames;
        auto it = producersByType.find(producerType);
        if (it != producersByType.end())
        {
            for (const auto &producer : it->second)
            {
                if (producer->canProduceAt > frame) continue;
                producerFrames.push(producer->canProduceAt);
            }
        }

        while (true)
        {
            // Guard against an endless loop
            if (additionalUnitCount > 100)
            {
                Log::Get() << "ERROR: Endless loop in worstCaseUnitCount; disabling economic model";
                isEnabled = false;
                return std::make_pair(currentCount, currentCount);
            }

            earliestFrame = canProduceUnitAt();
            if (earliestFrame > frame) break;

            // Try to get an existing producer that can produce it
            if (!producerFrames.empty())
            {
                int productionFrame = std::max(earliestFrame, producerFrames.top());
                producerFrames.pop();

                spendResource(minerals, type.mineralPrice(), productionFrame);
                spendResource(gas, type.gasPrice(), productionFrame);
                spendResource(supplyAvailable, type.supplyRequired(), productionFrame);

                additionalUnitCount++;

                productionFrame += buildTime;
                if (productionFrame < frame) producerFrames.push(productionFrame);
                continue;
            }

            // Try to create a new producer that can produce it
            int mineralCost = type.mineralPrice() + producerType.mineralPrice();
            int gasCost = type.gasPrice() + producerType.gasPrice();
            int f;
            for (f = MODEL_FRAME_LIMIT - 1; f >= earliestFrame - producerBuildTime; f--)
            {
                if (f == BUILDER_LOSS_UNTIL)
                {
                    mineralCost += BUILDER_LOSS;
                }

                if (minerals[f] < mineralCost) break;
                if (gas[f] < gasCost) break;

                if (f == earliestFrame)
                {
                    mineralCost -= type.mineralPrice();
                    gasCost -= type.gasPrice();
                }
            }
            f++;

            int productionFrame = f + producerBuildTime;
            if (productionFrame > frame) break;

            spendResource(minerals, producerType.mineralPrice(), f);
            spendResource(gas, producerType.gasPrice(), f);

            spendResource(minerals, type.mineralPrice(), productionFrame);
            spendResource(gas, type.gasPrice(), productionFrame);
            spendResource(supplyAvailable, type.supplyRequired(), productionFrame);

            additionalUnitCount++;

            productionFrame += buildTime;
            if (productionFrame < frame) producerFrames.push(productionFrame);
        }

        return createResult(additionalUnitCount);
    }

    int minimumProducerCount(BWAPI::UnitType producerType)
    {
        if (!isEnabled)
        {
            Log::Get() << "ERROR: Trying to use opponent economic model when it is not enabled";
            return 0;
        }

        auto it = producersByType.find(producerType);
        if (it == producersByType.end()) return 0;
        return (int)it->second.size();
    }

    bool hasBuilt(BWAPI::UnitType type)
    {
        if (!isEnabled)
        {
            Log::Get() << "ERROR: Trying to use opponent economic model when it is not enabled";
            return false;
        }

        auto it = observedUnitsByType.find(type);
        if (it != observedUnitsByType.end()) return true;

        return std::any_of(
                impliedUnits.begin(),
                impliedUnits.end(),
                [&type](auto &unit) { return unit->type == type; });
    }

    // The earliest frame the enemy could start building the given unit type
    int earliestUnitProductionFrame(BWAPI::UnitType type)
    {
        if (!isEnabled)
        {
            Log::Get() << "ERROR: Trying to use opponent economic model when it is not enabled";
            return 0;
        }

        auto cacheIt = earliestUnitProductionFrameCache.find(type);
        if (cacheIt != earliestUnitProductionFrameCache.end())
        {
            return cacheIt->second;
        }
        auto createResult = [&](int frame)
        {
            earliestUnitProductionFrameCache[type] = frame;
            return frame;
        };

        auto &&[minerals, gas, supplyAvailable] = simulateTakingGasIfNeeded(type);

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

            return createResult(f + 1);
        }

        // Get the total resource cost of the needed buildings
        int totalMineralCost = 0;
        int totalGasCost = 0;
        for (auto &[frameOffset, prerequisiteType] : prerequisites)
        {
            totalMineralCost += prerequisiteType.mineralPrice();
            totalGasCost += prerequisiteType.gasPrice();
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
            for (auto & [frameOffset, mineralCost, gasCost] : frameStopsAndResourcesNeeded)
            {
                int frame = f - frameOffset;
                if (frame >= MODEL_FRAME_LIMIT) continue;
                if (frame < 0
                    || minerals[frame] < mineralCost
                    || gas[frame] < gasCost)
                {
                    return createResult(f + 1);
                }
            }
        }
        return createResult(f + 1);
    }
}
