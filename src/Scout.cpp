#include "Scout.h"

#include "Map.h"
#include "Workers.h"
#include "Units.h"
#include "PathFinding.h"

namespace Scout
{
#ifndef _DEBUG
    namespace
    {
#endif
        ScoutingMode scoutingMode;
        BWAPI::Unit  workerScout;
        Base*        targetBase;

        void assignWorkerScout()
        {
            if (workerScout && workerScout->exists()) return;

            // Clear if worker scout is dead, in case we can't find a new one
            if (workerScout && !workerScout->exists()) workerScout = nullptr;

            // Get the set of bases we need to scout
            std::set<Base*> locations;
            if (!Map::getEnemyMain())
            {
                locations = Map::unscoutedStartingLocations();
            }
            else
            {
                locations.insert(Map::getEnemyMain());
            }

            // Find the available worker closest to any of these bases
            int bestTravelTime = INT_MAX;
            BWAPI::Unit bestWorker = nullptr;
            for (auto & unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!Workers::isAvailableForReassignment(unit)) continue;

                int unitBestTravelTime = INT_MAX;
                for (auto base : locations)
                {
                    int travelTime =
                        PathFinding::ExpectedTravelTime(unit->getPosition(), base->getPosition(), unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
                    if (travelTime != -1 && travelTime < unitBestTravelTime)
                    {
                        unitBestTravelTime = travelTime;
                    }
                }

                if (unitBestTravelTime < bestTravelTime)
                {
                    bestTravelTime = unitBestTravelTime;
                    bestWorker = unit;
                }
            }

            if (bestWorker)
            {
                Workers::reserveWorker(bestWorker);
                workerScout = bestWorker;
            }
        }

        void assignTargetBase()
        {
            if (targetBase && !targetBase->owner && targetBase->lastScouted == -1) return;

            targetBase = nullptr;

            // Pick the closest remaining base to the worker
            // TODO: On four player maps with buildable centers, it may be a good idea to cross the center to uncover proxies
            int bestTravelTime = INT_MAX;
            for (auto base : Map::unscoutedStartingLocations())
            {
                int travelTime =
                    PathFinding::ExpectedTravelTime(workerScout->getPosition(), base->getPosition(), workerScout->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
                if (travelTime != -1 && travelTime < bestTravelTime)
                {
                    bestTravelTime = travelTime;
                    targetBase = base;
                }
            }
        }

        void moveToTargetBase()
        {
            if (!targetBase) return;

            Units::getMine(workerScout).moveTo(targetBase->getPosition());
        }

        void releaseWorkerScout()
        {
            if (!workerScout || !workerScout->exists()) return;

            Workers::releaseWorker(workerScout);
            workerScout = nullptr;
        }

#ifndef _DEBUG
    }
#endif

    void update()
    {
        if (scoutingMode == ScoutingMode::None) return;

        // If we have scouted the enemy location, stop scouting
        auto enemyMain = Map::getEnemyMain();
        if (scoutingMode == ScoutingMode::Location && enemyMain)
        {
            scoutingMode = ScoutingMode::None;
            releaseWorkerScout();
            return;
        }

        // Ensure a worker scout is assigned
        assignWorkerScout();
        if (!workerScout) return;

        if (enemyMain)
        {
            // TODO: Implement full scouting
        }
        else
        {
            assignTargetBase();
            moveToTargetBase();
        }
    }

    void setScoutingMode(ScoutingMode mode)
    {
        scoutingMode = mode;
        CherryVis::log() << "Set scouting mode to " << mode;
    }
}