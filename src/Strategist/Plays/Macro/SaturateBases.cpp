#include "SaturateBases.h"

#include "Units.h"
#include "Workers.h"
#include "Map.h"
#include "PathFinding.h"

void SaturateBases::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hard cap of 75 workers
    if (Units::countAll(BWAPI::UnitTypes::Protoss_Probe) >= 75) return;

    // Define a set of bases sorted by the number of desired workers, highest to lowest
    auto comp = [](const std::pair<Base *, int> &a, const std::pair<Base *, int> &b)
    {
        return a.second < b.second;
    };
    std::set<std::pair<Base *, int>, decltype(comp)> basesAndWorkers(comp);

    // Loop through all base clusters - these are bases close enough together that we can treat their worker production together
    for (auto &baseCluster : Map::allBaseClusters())
    {
        // Gather the bases in the cluster that are owned by us and how many additional workers they require
        int totalWorkers = 0;
        basesAndWorkers.clear();
        for (auto &base : baseCluster)
        {
            if (base->owner != BWAPI::Broodwar->self()) continue;
            int desiredWorkers = Workers::availableMineralAssignments(base);
            totalWorkers += desiredWorkers;
            basesAndWorkers.insert(std::make_pair(base, desiredWorkers));
        }

        if (totalWorkers == 0) continue;

        // Balance the production amongst the bases
        int desiredProductionPerBase = totalWorkers / basesAndWorkers.size();
        int remainder = totalWorkers % basesAndWorkers.size();
        for (auto &pair : basesAndWorkers)
        {
            int count = desiredProductionPerBase;
            if (remainder > 0)
            {
                count++;
                remainder--;
            }

            if (!pair.first->resourceDepot) continue;
            if (!pair.first->resourceDepot->exists()) continue;
            if (!pair.first->resourceDepot->isCompleted()) continue;

            prioritizedProductionGoals[10].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                        BWAPI::UnitTypes::Protoss_Probe,
                                                        count,
                                                        1,
                                                        pair.first);
        }
    }
}
