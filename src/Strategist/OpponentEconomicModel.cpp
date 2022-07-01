#include "OpponentEconomicModel.h"

#include "Map.h"
#include "UnitUtil.h"

#define MODEL_FRAME_LIMIT 20000

namespace
{
    const double MINERALS_PER_WORKER_FRAME = 0.0445;
    const double GAS_PER_WORKER_FRAME = 0.071;

    // Represents an enemy unit in our economic model
    struct ObservedUnit
    {
        struct cmp
        {
            bool operator()(const std::shared_ptr<ObservedUnit> &a, const std::shared_ptr<ObservedUnit> &b) const
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

        ObservedUnit(BWAPI::UnitType type, int id, int creationFrame)
                : type(type)
                , id(id)
                , creationFrame(creationFrame)
                , completionFrame(creationFrame + UnitUtil::BuildTime(type))
                , deathFrame(INT_MAX)
        {}
    };

    typedef std::multiset<std::shared_ptr<ObservedUnit>, ObservedUnit::cmp> ObservedUnitSet;

    bool isEnabled;
    unsigned int workerLimit;

    std::unordered_map<int, std::shared_ptr<ObservedUnit>> observedUnitsById;
    ObservedUnitSet observedUnitsByCreation;

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
        for (const auto &unit : observedUnitsByCreation)
        {
            if (unit->type.isRefinery())
            {
                refineryFrames.push(unit->completionFrame);
            }
        }
        refineryFrames.push(INT_MAX);

        double mineralCounter = 0.0;
        double gasCounter = 0.0;
        int supplyCounter = 4;
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
                supplyCounter--;
            }
            remainingWorkerBuildTime--;

            // Build a pylon at frame 1115
            if (f == 1115)
            {
                mineralCounter -= 100;
            }
            if (f == 1625)
            {
                supplyCounter += 8;
            }

            // Send scout at frame 1850
            if (f == 1850)
            {
                mineralWorkers--;
            }

            // Scout dies at frame 4500
            if (f == 4500)
            {
                supplyCounter++;
            }

            // Set the resources at this frame
            mineralCounter += mineralWorkers * MINERALS_PER_WORKER_FRAME;
            gasCounter += gasWorkers * GAS_PER_WORKER_FRAME;
            minerals[f] = (int)mineralCounter;
            gas[f] = (int)gasCounter;
            supplyAvailable[f] = supplyCounter;
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
        observedUnitsByCreation.clear();
        minerals.assign(MODEL_FRAME_LIMIT, 0);
        gas.assign(MODEL_FRAME_LIMIT, 0);
        supplyAvailable.assign(MODEL_FRAME_LIMIT, 4);
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

        // Now subtract what we have directly observed to have been built

        // Now check if anything we've seen requires tech we haven't seen

        // Now simulate the production facilities to figure out if there must exist some we haven't already observed


    }

    void opponentUnitCreated(BWAPI::UnitType type, int id, int estimatedCreationFrame)
    {
        if (!isEnabled) return;
        if (ignoreUnit(type)) return;

        auto observedUnit = std::make_shared<ObservedUnit>(type, id, estimatedCreationFrame);
        observedUnitsById[id] = observedUnit;
        observedUnitsByCreation.insert(observedUnit);
    }

    void opponentUnitDestroyed(BWAPI::UnitType type, int id)
    {
        if (!isEnabled) return;
        if (ignoreUnit(type)) return;

        auto it = observedUnitsById.find(id);
        if (it != observedUnitsById.end())
        {
            it->second->deathFrame = currentFrame;
            return;
        }

        // We don't expect to see a unit die that hasn't already been observed, but handle it gracefully
        Log::Get() << "Non-observed unit died: " << type << "#" << id;
        auto observedUnit = std::make_shared<ObservedUnit>(type, id, currentFrame - UnitUtil::BuildTime(type));
        observedUnit->deathFrame = currentFrame;
        observedUnitsById[id] = observedUnit;
        observedUnitsByCreation.insert(observedUnit);
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

        return -1;
    }
}
