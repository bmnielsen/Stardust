#include "ScoutEnemyExpos.h"

#include "Workers.h"
#include "Map.h"
#include "Units.h"
#include "Players.h"
#include "Geo.h"

void ScoutEnemyExpos::update()
{
    // Clear the scout if it is dead
    if (scout && !scout->exists()) scout = nullptr;

    // Clear the target base if it has been scouted
    if (targetBase && (targetBase->owner != nullptr || (BWAPI::Broodwar->getFrameCount() - targetBase->lastScouted) < 1000))
    {
        targetBase = nullptr;
    }

    // Pick a target base
    // We score them based on three factors:
    // - How soon we expect the enemy to expand to the base (i.e. order of base in vector)
    // - How long it has been since we scouted the base
    // - How close our scout is to the base
    if (!targetBase)
    {
        usingSearchPath = true;

        double bestScore = 0.0;
        int scoreFactor = 0;
        for (auto base : Map::getUntakenExpansions(BWAPI::Broodwar->enemy()))
        {
            int framesSinceScouted = BWAPI::Broodwar->getFrameCount() - base->lastScouted;
            scoreFactor++;

            double score = (double) framesSinceScouted / (double) scoreFactor;

            if (scout)
            {
                int dist = scout->getDistance(base->getPosition());
                if (dist == -1) continue;
                score /= ((double) dist / 2000.0);
            }

            if (score > bestScore)
            {
                bestScore = score;
                targetBase = base;
            }
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

#if INSTRUMENTATION_ENABLED
        CherryVis::setBoardValue("ScoutEnemyExpos_target", "(none)");
#endif

        return;
    }

#if INSTRUMENTATION_ENABLED
    CherryVis::setBoardValue("ScoutEnemyExpos_target", (std::ostringstream() << targetBase->getTilePosition()).str());
#endif

    // TODO: Support scouting with a worker or other unit

    // We prefer to scout with an observer, so if we don't currently have one, request one
    // This play is lower-priority than combat plays, so it might get taken by another play if the enemy has cloaked units
    if (!scout || scout->type != BWAPI::UnitTypes::Protoss_Observer)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, targetBase->getPosition());
    }

    // If we don't have a scout, clear the target base for next time
    if (!scout)
    {
        targetBase = nullptr;
        return;
    }

    // This is a quick hack to prevent us from searching for non-existant paths every frame and timing out
    if (!usingSearchPath)
    {
        scout->moveTo(targetBase->getPosition());

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
        return;
    }

    // Search for a path that avoids enemy threats
    auto tile = BWAPI::TilePosition(targetBase->getPosition());
    auto grid = Players::grid(BWAPI::Broodwar->enemy());
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
            auto scaledVector = Geo::ScaleVector(scout->lastPosition - nearestThreat->lastPosition, 96);
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

    if (path.size() < 5)
    {
        usingSearchPath = false;
        scout->moveTo(targetBase->getPosition());

#if DEBUG_UNIT_ORDERS
        CherryVis::log(scout->id) << "Scout: target directly to scout tile " << BWAPI::WalkPosition(targetPos);
#endif
    }
    else
    {
        scout->moveTo(BWAPI::Position(path[4]) + BWAPI::Position(16, 16));

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

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Observer, 1, 1);
    }
}

void ScoutEnemyExpos::disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                              const std::function<void(const MyUnit &)> &movableUnitCallback)
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
