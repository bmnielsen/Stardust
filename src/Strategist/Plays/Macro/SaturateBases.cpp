#include "SaturateBases.h"

#include "Units.h"
#include "Workers.h"
#include "Map.h"
#include "PathFinding.h"
#include "Strategist.h"

void SaturateBases::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hard cap, defaults to 75 workers
    if (Units::countAll(BWAPI::UnitTypes::Protoss_Probe) >= workerLimit) return;

    // On the first scan, gather all owned bases and how many workers they need
    std::vector<Base *> fullBases;
    std::vector<std::pair<std::vector<Base *>, int>> baseClusters;
    for (auto &base : Map::getMyBases(BWAPI::Broodwar->self()))
    {
        int desiredWorkers = Workers::availableMineralAssignments(base) + Workers::availableGasAssignments(base);

        // Reduce by one if this base is already building a worker
        // It's OK if this base goes to a negative number of workers
        if (base->resourceDepot && base->resourceDepot->isTraining()) desiredWorkers--;

        // TODO: Reduce by workers in the area assigned to other duties

        if (desiredWorkers > 0)
        {
            baseClusters.emplace_back(std::vector<Base *>{base}, desiredWorkers);
        }
        else
        {
            fullBases.push_back(base);
        }
    }

    // Now assign full bases to base clusters if they can safely transfer workers
    for (auto &base : fullBases)
    {
        std::vector<Base *> *bestCluster = nullptr;
        int bestFrames = INT_MAX;
        for (auto &cluster : baseClusters)
        {
            int frames = PathFinding::ExpectedTravelTime(base->getPosition(),
                                                         (*cluster.first.begin())->getPosition(),
                                                         BWAPI::UnitTypes::Protoss_Probe,
                                                         PathFinding::PathFindingOptions::Default,
                                                         -1);
            if (frames == -1) continue;
            if (frames < bestFrames)
            {
                bestFrames = frames;
                bestCluster = &cluster.first;
            }
        }

        if (bestCluster && (bestFrames <= 400 || Strategist::isEnemyContained()))
        {
            bestCluster->push_back(base);
        }
    }

    // Now order the production
    for (auto &clusterAndRequiredWorkers : baseClusters)
    {
        // Balance the production amongst the bases
        size_t desiredProductionPerBase = clusterAndRequiredWorkers.second / clusterAndRequiredWorkers.first.size();
        size_t remainder = clusterAndRequiredWorkers.second % clusterAndRequiredWorkers.first.size();
        for (auto &base : clusterAndRequiredWorkers.first)
        {
            size_t count = desiredProductionPerBase;
            if (remainder > 0)
            {
                count++;
                remainder--;
            }

            if (count < 1) continue;
            if (!base->resourceDepot) continue;
            if (!base->resourceDepot->completed) continue;

            prioritizedProductionGoals[PRIORITY_WORKERS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                      label,
                                                                      BWAPI::UnitTypes::Protoss_Probe,
                                                                      count,
                                                                      1,
                                                                      base);
        }
    }
}
