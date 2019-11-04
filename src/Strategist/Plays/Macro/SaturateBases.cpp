#include "SaturateBases.h"

#include "Units.h"
#include "Workers.h"
#include "Map.h"

void SaturateBases::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hard cap of 75 workers
    if (Units::countAll(BWAPI::UnitTypes::Protoss_Probe) >= 75) return;

    // Start by getting our bases and sorting them by the number of mineral assignments they have available
    auto comp = [](const std::pair<Base *, int> &a, const std::pair<Base *, int> &b)
    {
        return a.second < b.second;
    };
    std::set<std::pair<Base *, int>, decltype(comp)> basesAndWorkers(comp);
    for (auto &base : Map::getMyBases())
    {
        basesAndWorkers.insert(std::make_pair(base, Workers::availableMineralAssignments(base)));
    }

    // TODO: Rebalance the production to e.g. produce probes from our main to be transferred to our natural

    // Order the production
    for (auto &baseAndWorkers : basesAndWorkers)
    {
        if (baseAndWorkers.second < 1) continue;
        if (!baseAndWorkers.first->resourceDepot) continue;
        if (!baseAndWorkers.first->resourceDepot->exists()) continue;
        if (!baseAndWorkers.first->resourceDepot->isCompleted()) continue;

        prioritizedProductionGoals[10].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                    BWAPI::UnitTypes::Protoss_Probe,
                                                    baseAndWorkers.second,
                                                    1,
                                                    baseAndWorkers.first);
    }
}
