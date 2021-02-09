#include "UnitCluster.h"

#include "Map.h"
#include "Geo.h"
#include "Boids.h"

#include "DebugFlag_UnitOrders.h"

/*
 * Stands ground out of range of the enemy.
 *
 * Attempts to form an arc away from the enemy army.
 */

namespace
{
    const double separationDetectionLimitFactor = 1.5;
    const double separationWeight = 64.0;
}

void UnitCluster::standGround(std::set<Unit> &enemyUnits, BWAPI::Position targetPosition)
{
    // Flee if we aren't in a location where we can make a decent formation
    if (!vanguard || Map::walkableWidth(vanguard->tilePositionX, vanguard->tilePositionY) < 7)
    {
        move(Map::getMyMain()->getPosition());
        return;
    }

    // Get the pivot point
    // Normally the closest enemy ground unit to the vanguard
    auto pivot = BWAPI::Positions::Invalid;
    int closestDist = INT_MAX;
    for (const auto &unit : enemyUnits)
    {
        if (unit->isFlying) continue;
        if (!unit->lastPositionValid) continue;

        int dist = vanguard->getDistance(unit);
        if (dist < closestDist)
        {
            closestDist = dist;
            pivot = unit->lastPosition;
        }
    }

    // If there was no enemy unit to use, pick a spot ahead of the vanguard towards the target position
    if (pivot == BWAPI::Positions::Invalid)
    {
        auto waypoint = PathFinding::NextGridOrChokeWaypoint(vanguard->lastPosition,
                targetPosition,
                PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition)),
                3,
                true);
        if (waypoint != BWAPI::Positions::Invalid)
        {
            pivot = vanguard->lastPosition + Geo::ScaleVector(waypoint - vanguard->lastPosition, 8 * 32);
        }
    }

    // Flee if we couldn't find a good pivot
    if (!pivot.isValid())
    {
        move(Map::getMyMain()->getPosition());
        return;
    }

    int desiredDistance = std::max(15 * 32, vanguard->lastPosition.getApproxDistance(pivot));

    // Now micro the units
    for (auto &myUnit : units)
    {
        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        // Flying units go to the vanguard
        if (myUnit->isFlying)
        {
            myUnit->moveTo(vanguard->lastPosition);
            continue;
        }

        // Goal boid: attempt to keep the desired distance from the pivot
        int goalX = 0;
        int goalY = 0;
        auto vector = Geo::ScaleVector(pivot - myUnit->lastPosition,
                                       myUnit->lastPosition.getApproxDistance(pivot) - desiredDistance);
        if (vector != BWAPI::Positions::Invalid)
        {
            goalX = vector.x;
            goalY = vector.y;
        }

        // Separation boid: push away from friendly units
        int separationX = 0;
        int separationY = 0;
        for (const auto &other : units)
        {
            if (other == myUnit) continue;
            if (other->isFlying) continue;

            Boids::AddSeparation(myUnit.get(),
                                 other,
                                 separationDetectionLimitFactor,
                                 separationWeight,
                                 separationX,
                                 separationY);
        }

        auto pos = Boids::ComputePosition(myUnit.get(), {goalX, separationX}, {goalY, separationY}, 0, 4);

#if DEBUG_UNIT_BOIDS
        CherryVis::log(myUnit->id) << "StandGround boids towards " << BWAPI::WalkPosition(targetPosition)
                                   << "; cluster=" << BWAPI::WalkPosition(center)
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(separationX, separationY))
                                   << "; total="
                                   << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX + separationX, goalY + separationY))
                                   << "; target=" << BWAPI::WalkPosition(pos);
#elif DEBUG_UNIT_ORDERS
        if (pos == BWAPI::Positions::Invalid)
        {
            CherryVis::log(myUnit->id) << "StandGround boids: Invalid; moving to "
                                       << BWAPI::WalkPosition(vanguard->lastPosition);
        }
        else
        {
            CherryVis::log(myUnit->id) << "StandGround boids: Moving to " << BWAPI::WalkPosition(pos);
        }
#endif

        // If the unit can't move in the desired direction, move towards the vanguard unit
        if (pos == BWAPI::Positions::Invalid)
        {
            myUnit->moveTo(vanguard->lastPosition);
        }
        else
        {
            myUnit->moveTo(pos, true);
        }
    }
}
