#include "SaturateBases.h"

#include "Config.h"
#include "Units.h"
#include "Workers.h"
#include "Map.h"
#include "PathFinding.h"
#include "Strategist.h"

void SaturateBases::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Hard cap, defaults to 75 workers */
    if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Probe) >= workerLimit) return;

    /* window_push (2026-10-08): once the one-shot push has fired, every round
     * of production goes to the army - stop making workers */
    if (Config::WindowPush::fired) return;

    /* On the first scan, gather all owned bases and how many workers they need */
    std::vector<Base *> fullBases;
    std::vector<std::pair<std::vector<Base *>, int>> baseClusters;
    for (auto &baseLoc : Map::getMyBases(BWAPI::Broodwar->self()))
    {
        int desiredWorkers = WorkerMgr::availableMineralAssignments(baseLoc, workersPerPatch) + WorkerMgr::availableGasAssignments(baseLoc);

        /* Reduce by one if this base is already building a worker */
        /* It's OK if this base goes to a negative number of workers */
        if (baseLoc->resourceDepot && baseLoc->resourceDepot->isTraining()) desiredWorkers--;

        /* TODO: Reduce by workers in the area assigned to other duties */

        if (desiredWorkers > 0)
        {
            baseClusters.emplace_back(std::vector<Base *>{baseLoc}, desiredWorkers);
        }
        else
        {
            fullBases.push_back(baseLoc);
        }
    }

    /* Now assign full bases to base clusters if they can safely transfer workers */
    for (auto &baseLoc : fullBases)
    {
        std::vector<Base *> *bestCluster = nullptr;
        int bestFrames = INT_MAX;
        for (auto &clump : baseClusters)
        {
            int frames = Pathfinder::ExpectedTravelTime(baseLoc->getPosition(),
                                                         (*clump.first.begin())->getPosition(),
                                                         BWAPI::UnitTypes::Protoss_Probe,
                                                         Pathfinder::PathFindingOptions::Default,
                                                         -1);
            if (frames == -1) continue;
            if (frames < bestFrames)
            {
                bestFrames = frames;
                bestCluster = &clump.first;
            }
        }

        if (bestCluster && (bestFrames <= 400 || Director::isEnemyContained()))
        {
            bestCluster->push_back(baseLoc);
        }
    }

    /* Now order the production */
    for (auto &clusterAndRequiredWorkers : baseClusters)
    {
        /* Balance the production amongst the bases */
        size_t desiredProductionPerBase = clusterAndRequiredWorkers.second / clusterAndRequiredWorkers.first.size();
        size_t remainder = clusterAndRequiredWorkers.second % clusterAndRequiredWorkers.first.size();
        for (auto &baseLoc : clusterAndRequiredWorkers.first)
        {
            size_t count = desiredProductionPerBase;
            if (remainder > 0)
            {
                count++;
                remainder--;
            }

            if (count < 1) continue;
            if (!baseLoc->resourceDepot) continue;
            if (!baseLoc->resourceDepot->completed) continue;

            prioProdGoals[PRIORITY_WORKERS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                      label,
                                                                      BWAPI::UnitTypes::Protoss_Probe,
                                                                      count,
                                                                      1,
                                                                      baseLoc);
        }
    }
}
