#include "ScoutEnemyExpos.h"

#include "Workers.h"
#include "Map.h"
#include "Units.h"
#include "Players.h"
#include "Geo.h"
#include "UnitUtil.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_BOARD_VALUES true
#endif

namespace
{
    // Moves the scout. For flying units, respects its halt distance.
    void scoutMove(const MyUnit &scout, BWAPI::Position pos)
    {
        if (!scout->isFlying) scout->moveTo(pos);

        // Always move towards a position at least halt distance away
        auto scaledVector = Geo::ScaleVector(pos - scout->lastPosition, UnitUtil::HaltDistance(scout->type) + 16);

        // Move to the scaled position if it is valid, otherwise use the position directly
        auto scaledPos = (scaledVector == BWAPI::Positions::Invalid) ? pos : scout->lastPosition + scaledVector;
        if (!scaledPos.isValid()) scaledPos = pos;
        scout->moveTo(scaledPos);
    }
}

void ScoutEnemyExpos::update()
{
    // Clear the scout if it is dead
    if (scout && !scout->exists())
    {
        if (scout->type.isWorker()) workerScoutHasDied = true;
        scout = nullptr;
    }

    // Clear the target base if it has been scouted
    if (targetBase && (targetBase->owner != nullptr || (currentFrame - targetBase->lastScouted) < 1000))
    {
        targetBase = nullptr;
    }

    // Pick a target base
    // We score them based on three factors:
    // - How soon we expect the enemy to expand to the base (i.e. order of base in vector)
    // - How long it has been since we scouted the base
    // - How close our scout is to the base
    // Islands are considered in the same way, but sorted after other bases in priority
    if (!targetBase)
    {
        usingSearchPath = true;

        double bestScore = 0.0;
        int scoreFactor = 0;
        auto handleBase = [&](Base *base)
        {
            int framesSinceScouted = currentFrame - base->lastScouted;
            scoreFactor++;

            double score = (double) framesSinceScouted / (double) scoreFactor;

            int dist;
            if (scout && scout->isFlying)
            {
                dist = scout->getDistance(base->getPosition());
            }
            else if (scout)
            {
                dist = PathFinding::GetGroundDistance(scout->lastPosition,
                                                      base->getPosition(),
                                                      scout->type,
                                                      PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
            }
            else
            {
                dist = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(),
                                                      base->getPosition(),
                                                      BWAPI::UnitTypes::Protoss_Probe,
                                                      PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
            }

            // This effectively skips island expansions until we have an observer to scout with
            if (dist == -1) return;

            score /= ((double) dist / 2000.0);

            if (score > bestScore)
            {
                bestScore = score;
                targetBase = base;
            }
        };
        for (auto base : Map::getUntakenExpansions(BWAPI::Broodwar->enemy()))
        {
            handleBase(base);
        }
        for (auto base : Map::getUntakenIslandExpansions(BWAPI::Broodwar->enemy()))
        {
            handleBase(base);
        }
    }

    if (!targetBase)
    {
        if (scout)
        {
            if (scout->type.isWorker())
            {
                Workers::releaseWorker(scout);
                scout = nullptr;
            }
            else
            {
                status.removedUnits.push_back(scout);
            }
        }

#if CVIS_BOARD_VALUES
        CherryVis::setBoardValue("ScoutEnemyExpos_target", "(none)");
#endif

        return;
    }

#if CVIS_BOARD_VALUES
    CherryVis::setBoardValue("ScoutEnemyExpos_target", (std::ostringstream() << targetBase->getTilePosition()).str());
#endif

    // TODO: Support scouting with a worker or other unit

    // We prefer to scout with an observer, so if we don't currently have one, request one
    // This play is lower-priority than combat plays, so it might get taken by another play if the enemy has cloaked units
    if (!scout || scout->type != BWAPI::UnitTypes::Protoss_Observer)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, targetBase->getPosition());
    }

    // Request a worker scout if we have no scout and we haven't had a worker scout die on us before
    if (!scout && !workerScoutHasDied)
    {
        scout = Workers::getClosestReassignableWorker(targetBase->getPosition(), false);
        if (scout)
        {
            Workers::reserveWorker(scout);
        }
    }

    // If we don't have a scout, clear the target base for next time
    if (!scout)
    {
        targetBase = nullptr;
        return;
    }

    // Worker scouts just move to the target
    // TODO: Threat-aware pathing
    if (!scout->isFlying)
    {
        scout->moveTo(targetBase->getPosition());
        return;
    }

    // This is a quick hack to prevent us from searching for non-existant paths every frame and timing out
    if (!usingSearchPath)
    {
        scoutMove(scout, targetBase->getPosition());

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
        return;
    }

    // Search for a path that avoids enemy threats
    auto tile = BWAPI::TilePosition(targetBase->getPosition());
    auto &grid = Players::grid(BWAPI::Broodwar->enemy());
    auto avoidThreatTiles = [&grid](BWAPI::TilePosition tile)
    {
        return grid.airThreat((tile.x << 2U) + 2, (tile.y << 2U) + 2) == 0;
    };

    // If the current position of the observer is a threat, move away from whatever is threatening it
    if (!avoidThreatTiles(scout->getTilePosition()))
    {
        Unit nearestThreat = nullptr;
        int nearestThreatDist = INT_MAX;
        for (auto &unit : Units::allEnemy())
        {
            if (!unit->lastPositionValid) continue;
            if (unit->type.isDetector() || unit->canAttackAir())
            {
                int dist = scout->getDistance(unit);
                if (dist < nearestThreatDist)
                {
                    nearestThreat = unit;
                    nearestThreatDist = dist;
                }
            }
        }

        if (nearestThreat)
        {
            auto scaledVector = Geo::ScaleVector(scout->lastPosition - nearestThreat->lastPosition, UnitUtil::HaltDistance(scout->type) + 16);
            if (scaledVector.isValid())
            {
                scout->moveTo(scout->lastPosition + scaledVector);

#if DEBUG_UNIT_ORDERS
                CherryVis::log(scout->id) << "Scout: move to " << BWAPI::WalkPosition(scaledVector) << " to avoid " << *nearestThreat;
#endif
                return;
            }
        }

        scout->moveTo(Map::getMyMain()->getPosition());

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: move to main to avoid unknown threat";
#endif
        return;
    }

    auto path = PathFinding::Search(scout->getTilePosition(), tile, avoidThreatTiles);

    if (path.size() < 3)
    {
        usingSearchPath = false;
        scoutMove(scout, targetBase->getPosition());

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
    }
    else
    {
        scoutMove(scout, BWAPI::Position(path[2]) + BWAPI::Position(16, 16));

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: target next path waypoint "
                                  << BWAPI::WalkPosition(BWAPI::Position(path[2]) + BWAPI::Position(16, 16));
#endif
    }
}

void ScoutEnemyExpos::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Produce an observer if we have a unit requirement for it and have the prerequisites
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observatory) == 0
            || Units::countCompleted(BWAPI::UnitTypes::Protoss_Robotics_Facility) == 0)
        {
            continue;
        }

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    }
}

void ScoutEnemyExpos::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                              const std::function<void(const MyUnit)> &movableUnitCallback)
{
    if (scout)
    {
        if (scout->type.isWorker())
        {
            Workers::releaseWorker(scout);
        }
        else
        {
            movableUnitCallback(scout);
        }
    }
}

void ScoutEnemyExpos::addUnit(const MyUnit &unit)
{
    if (scout && scout->type.isWorker())
    {
        Workers::releaseWorker(scout);
    }

    scout = unit;
}

void ScoutEnemyExpos::removeUnit(const MyUnit &unit)
{
    if (scout == unit) scout = nullptr;
}
