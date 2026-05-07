#include "MineralLockingOptimization.h"

#include "Map.h"
#include "Units.h"

namespace MineralLockingOptimization
{
    void initialize() {}

    void gameEnd() {}

    void update() {}

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit) return;

        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            worker->gather(resourceBwapiUnit);
        }
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource) {}

    std::map<MyWorker, std::tuple<Resource, Resource, std::optional<MiningOptimization::InitialSplitData>>> initialWorkerSplit()
    {
        std::map<MyWorker, std::tuple<Resource, Resource, std::optional<MiningOptimization::InitialSplitData>>> assignments;

        auto base = Map::getMyMain();

        // Gather the patches and workers
        std::vector<Resource> mineralPatches = Map::getMyMain()->mineralPatches();
        std::vector<MyWorker> workers;
        for (auto unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
        {
            workers.emplace_back(std::static_pointer_cast<MyWorkerImpl>(unit));
        }

        // Sort the mineral patches by proximity to the nexus, then by position
        std::sort(mineralPatches.begin(), mineralPatches.end(), [&](const Resource &a, const Resource &b) -> bool
        {
            auto aDist = a->getDistance(BWAPI::UnitTypes::Protoss_Nexus, base->getPosition());
            auto bDist = b->getDistance(BWAPI::UnitTypes::Protoss_Nexus, base->getPosition());
            return aDist < bDist || (aDist == bDist && a->tile < b->tile);
        });

        // We are only interested in the first four patches
        mineralPatches.resize(4);
        std::set<Resource> availablePatches(mineralPatches.begin(), mineralPatches.end());

        // Sort the workers by position
        std::sort(workers.begin(),
                  workers.end(),
                  [](const MyWorker &first, const MyWorker &second)
                  {
                      return first->lastPosition < second->lastPosition;
                  });

        // Greedily take the closest matches until all probes are assigned
        // TODO: Should really be optimizing for 7th collection
        for (int i = 0; i < 4; i++)
        {
            int bestDist = INT_MAX;
            MyWorker bestWorker = nullptr;
            Resource bestPatch = nullptr;
            for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                auto worker = std::static_pointer_cast<MyWorkerImpl>(unit);
                if (assignments.contains(worker)) continue;

                for (auto &patch : availablePatches)
                {
                    int dist = patch->getDistance(worker);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestWorker = worker;
                        bestPatch = patch;
                    }
                }
            }

            if (bestWorker && bestPatch)
            {
                assignments[bestWorker] = std::make_tuple(bestPatch, bestPatch, std::nullopt);
                availablePatches.erase(bestPatch);
            }
        }

        return assignments;
    }
}
