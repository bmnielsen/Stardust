#include "ScoutEnemyExpos.h"

#include "Workers.h"
#include "Map.h"
#include "Units.h"
#include "Players.h"
#include "Geo.h"
#include "UnitUtil.h"
#include "MyWorker.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_BOARD_VALUES true
#endif

namespace
{
    /* Moves the scout. For flying units, respects its halt distance. */
    void scoutMove(const OwnedUnit &scout, BWAPI::Position pt)
    {
        if (!scout->isFlying) scout->moveTo(pt);

        /* Always move towards a position at least halt distance away */
        auto scaledVector = Geom::ScaleVector(pt - scout->prevPos, UnitHelper::HaltDistance(scout->type) + 16);

        /* Move to the scaled position if it is valid, otherwise use the position directly */
        auto scaledPos = (scaledVector == BWAPI::Positions::Invalid) ? pt : scout->prevPos + scaledVector;
        if (!scaledPos.isValid()) scaledPos = pt;
        scout->moveTo(scaledPos);
    }
}

void ScoutEnemyExpos::update()
{
    /* Clear the scout if it is dead */
    if (scout && !scout->exists())
    {
        if (scout->type.isWorker()) workerScoutHasDied = true;
        scout = nullptr;
    }

    /* Clear the target base if it has been scouted */
    if (targetBase && (targetBase->owner != nullptr || (frameNow - targetBase->lastScouted) < 1000))
    {
        targetBase = nullptr;
    }

    /* Pick a target base */
    /* We score them based on three factors: */
    /* - How soon we expect the enemy to expand to the base (i.e. order of base in vector) */
    /* - How long it has been since we scouted the base */
    /* - How close our scout is to the base */
    /* Islands are considered in the same way, but sorted after other bases in priority */
    if (!targetBase)
    {
        usingSearchPath = true;

        double bestScore = 0.0;
        int scoreFactor = 0;
        auto handleBase = [&](Base *baseLoc)
        {
            int framesSinceScouted = frameNow - baseLoc->lastScouted;
            scoreFactor++;

            double score = (double) framesSinceScouted / (double) scoreFactor;

            int span;
            if (scout && scout->isFlying)
            {
                span = scout->getDistance(baseLoc->getPosition());
            }
            else if (scout)
            {
                span = Pathfinder::GetGroundDistance(scout->prevPos,
                                                      baseLoc->getPosition(),
                                                      scout->type,
                                                      Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
            }
            else
            {
                span = Pathfinder::GetGroundDistance(Map::getMyMain()->getPosition(),
                                                      baseLoc->getPosition(),
                                                      BWAPI::UnitTypes::Protoss_Probe,
                                                      Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
            }

            /* This effectively skips island expansions until we have an observer to scout with */
            if (span == -1) return;

            score /= ((double) span / 2000.0);

            if (score > bestScore)
            {
                bestScore = score;
                targetBase = baseLoc;
            }
        };
        for (auto baseLoc : Map::getUntakenExpansions(BWAPI::Broodwar->enemy()))
        {
            handleBase(baseLoc);
        }
        for (auto baseLoc : Map::getUntakenIslandExpansions(BWAPI::Broodwar->enemy()))
        {
            handleBase(baseLoc);
        }
    }

    if (!targetBase)
    {
        if (scout)
        {
            if (scout->type.isWorker())
            {
                WorkerMgr::releaseWorker(std::static_pointer_cast<MyWorkerImpl>(scout));
                scout = nullptr;
            }
            else
            {
                status.removedUnits.push_back(scout);
            }
        }

#if CVIS_BOARD_VALUES
        Telemetry::setBoardValue("ScoutEnemyExpos_target", "(none)");
#endif

        return;
    }

#if CVIS_BOARD_VALUES
    Telemetry::setBoardValue("ScoutEnemyExpos_target", (std::ostringstream() << targetBase->getTilePosition()).str());
#endif

    /* TODO: Support scouting with a worker or other unit */

    /* We prefer to scout with an observer, so if we don't currently have one, request one */
    /* This play is lower-priority than combat plays, so it might get taken by another play if the enemy has cloaked units */
    if (!scout || scout->type != BWAPI::UnitTypes::Protoss_Observer)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, targetBase->getPosition());
    }

    /* Request a worker scout if we have no scout and we haven't had a worker scout die on us before */
    if (!scout && !workerScoutHasDied)
    {
        auto wrk = WorkerMgr::getClosestReassignableWorker(targetBase->getPosition(), false);
        if (wrk)
        {
            WorkerMgr::reserveWorker(wrk);
            scout = wrk;
        }
    }

    /* If we don't have a scout, clear the target base for next time */
    if (!scout)
    {
        targetBase = nullptr;
        return;
    }

    /* Worker scouts just move to the target */
    /* TODO: Threat-aware pathing */
    if (!scout->isFlying)
    {
        scout->moveTo(targetBase->getPosition());
        return;
    }

    /* This is a quick hack to prevent us from searching for non-existant paths every frame and timing out */
    if (!usingSearchPath)
    {
        scoutMove(scout, targetBase->getPosition());

#if DEBUG_UNIT_ORDERS
        Telemetry::log(scout->ident) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
        return;
    }

    /* Search for a path that avoids enemy threats */
    auto tilePos = BWAPI::TilePosition(targetBase->getPosition());
    auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
    auto avoidThreatTiles = [&grid](BWAPI::TilePosition tilePos)
    {
        return grid.airThreat((tilePos.x << 2U) + 2, (tilePos.y << 2U) + 2) == 0;
    };

    /* If the current position of the observer is a threat, move away from whatever is threatening it */
    if (!avoidThreatTiles(scout->getTilePosition()))
    {
        Unit nearestThreat = nullptr;
        int nearestThreatDist = INT_MAX;
        for (auto &gameUnit : UnitMgr::allEnemy())
        {
            if (!gameUnit->lastPositionValid) continue;
            if (gameUnit->type.isDetector() || gameUnit->canAttackAir())
            {
                int span = scout->getDistance(gameUnit);
                if (span < nearestThreatDist)
                {
                    nearestThreat = gameUnit;
                    nearestThreatDist = span;
                }
            }
        }

        if (nearestThreat)
        {
            auto scaledVector = Geom::ScaleVector(scout->prevPos - nearestThreat->prevPos, UnitHelper::HaltDistance(scout->type) + 16);
            if (scaledVector.isValid())
            {
                scout->moveTo(scout->prevPos + scaledVector);

#if DEBUG_UNIT_ORDERS
                Telemetry::log(scout->ident) << "Scout: move to " << BWAPI::WalkPosition(scaledVector) << " to avoid " << *nearestThreat;
#endif
                return;
            }
        }

        scout->moveTo(Map::getMyMain()->getPosition());

#if DEBUG_UNIT_ORDERS
        Telemetry::log(scout->ident) << "Scout: move to main to avoid unknown threat";
#endif
        return;
    }

    auto path = Pathfinder::Search(scout->getTilePosition(), tilePos, avoidThreatTiles);

    if (path.size() < 3)
    {
        usingSearchPath = false;
        scoutMove(scout, targetBase->getPosition());

#if DEBUG_UNIT_ORDERS
        Telemetry::log(scout->ident) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
    }
    else
    {
        scoutMove(scout, BWAPI::Position(path[2]) + BWAPI::Position(16, 16));

#if DEBUG_UNIT_ORDERS
        Telemetry::log(scout->ident) << "Scout: target next path waypoint "
                                  << BWAPI::WalkPosition(BWAPI::Position(path[2]) + BWAPI::Position(16, 16));
#endif
    }
}

void ScoutEnemyExpos::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Produce an observer if we have a unit requirement for it and have the prerequisites */
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Observatory) == 0
            || UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Robotics_Facility) == 0)
        {
            continue;
        }

        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    }
}

void ScoutEnemyExpos::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                              const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    if (scout)
    {
        if (scout->type.isWorker())
        {
            WorkerMgr::releaseWorker(std::static_pointer_cast<MyWorkerImpl>(scout));
        }
        else
        {
            movableUnitCallback(scout);
        }
    }
}

void ScoutEnemyExpos::addUnit(const OwnedUnit &gameUnit)
{
    if (scout && scout->type.isWorker())
    {
        WorkerMgr::releaseWorker(std::static_pointer_cast<MyWorkerImpl>(scout));
    }

    scout = gameUnit;
}

void ScoutEnemyExpos::removeUnit(const OwnedUnit &gameUnit)
{
    if (scout == gameUnit) scout = nullptr;
}
