#include "UnitCluster.h"

#include "Geo.h"
#include "Boids.h"
#include "Map.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    const double separationDetectionLimitFactor = 1.25;
    const double separationWeight = 32.0;
}

bool UnitCluster::formArc(BWAPI::Position pivot, int desiredDistance)
{
    // Don't form an arc if the cluster only consists of flying units
    if (vanguard->isFlying) return false;

    // Don't form an arc if the walkable width at the vanguard unit is too small
    // This indicates we are in a narrow space that isn't well-suited to forming an arc
    if (Map::walkableWidth(vanguard->tilePositionX, vanguard->tilePositionY) < 7)
    {
        return false;
    }

    int vanguardDistToPivot = vanguard->lastPosition.getApproxDistance(pivot);

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

        // Units that are too far away move towards the vanguard
        int distToPivot = myUnit->lastPosition.getApproxDistance(pivot);
        if (distToPivot - vanguardDistToPivot > 128)
        {
            myUnit->moveTo(vanguard->lastPosition);
            continue;
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

        // Goal boid: attempt to keep the desired distance from the pivot
        int goalX = 0;
        int goalY = 0;

        // If we have a separation component, weight the goal at least twice as high
        int separationLength = Geo::ApproximateDistance(separationX, 0, separationY, 0);
        int desiredDistChange = myUnit->lastPosition.getApproxDistance(pivot) - desiredDistance + (UnitUtil::IsRangedUnit(myUnit->type) ? 0 : 32);
        BWAPI::Position vector;
        if (desiredDistChange > 0)
        {
            vector = Geo::ScaleVector(pivot - myUnit->lastPosition, std::max(desiredDistChange, separationLength * 2));
        }
        else
        {
            vector = Geo::ScaleVector(pivot - myUnit->lastPosition, -std::max(-desiredDistChange, separationLength * 2));
        }

        if (vector != BWAPI::Positions::Invalid)
        {
            goalX = vector.x;
            goalY = vector.y;
        }

        auto pos = Boids::ComputePosition(myUnit.get(), {goalX, separationX}, {goalY, separationY}, 0, 4);

#if DEBUG_UNIT_BOIDS
        CherryVis::log(myUnit->id) << "Arc boids " << desiredDistance << " from " << BWAPI::WalkPosition(pivot)
                                   << ", change=" << desiredDistChange
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(separationX, separationY))
                                   << "; total="
                                   << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX + separationX, goalY + separationY))
                                   << "; target=" << BWAPI::WalkPosition(pos);
#elif DEBUG_UNIT_ORDERS
        if (pos == BWAPI::Positions::Invalid)
        {
            CherryVis::log(myUnit->id) << "Arc boids: Invalid; moving to "
                                       << BWAPI::WalkPosition(vanguard->lastPosition);
        }
        else
        {
            CherryVis::log(myUnit->id) << "Arc boids: Moving to " << BWAPI::WalkPosition(pos);
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

    return true;
}
