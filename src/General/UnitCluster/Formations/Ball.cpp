#include "UnitCluster.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Boids.h"

#include "DebugFlag_UnitOrders.h"

/*
 * Cluster movement as a ball
 *
 * When we have a navigation grid (which is usually the case), we implement flocking with three boids:
 * - Goal: moves the unit towards the target position
 * - Cohesion: keeps the cluster together
 * - Separation: keeps some space between the units
 *
 * Collisions with unwalkable terrain are handled by having the unit ignore the boids and move only using the
 * grid. These cases are hopefully lessened by the fact that our navigation grid favours paths away from
 * unwalkable tiles.
 */

namespace
{
    const double pi = 3.14159265358979323846;

    // TODO: These parameters need to be tuned
    const double goalWeight = 128.0;
    const double cohesionWeight = 64.0;
    const double separationDetectionLimitFactor = 2.0;
    const double separationWeight = 96.0;
}

bool UnitCluster::moveAsBall(BWAPI::Position targetPosition)
{
    // We require a grid
    auto grid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));
    if (!grid) return false;

    // Scaling factor for cohesion boid is based on the size of the squad
    double cohesionFactor = cohesionWeight / sqrt(area / pi);

    for (const auto &unit : units)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

        // Flying units just move for now
        if (unit->isFlying)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Move to target: Moving to " << BWAPI::WalkPosition(targetPosition);
#endif
            unit->moveTo(targetPosition);
            continue;
        }

        // Get the waypoint to move towards
        // We attempt to move towards the third node ahead of us
        auto waypoint = PathFinding::NextGridOrChokeWaypoint(unit->lastPosition, targetPosition, grid, 3, true);

        // If a valid waypoint couldn't be found, defer to normal unit move
        if (!waypoint.isValid())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Move to target: Moving to " << BWAPI::WalkPosition(targetPosition);
#endif
            unit->moveTo(targetPosition);
            continue;
        }

        // We have a grid node, so compute our boids

        // Goal

        // Initialize with the offset to the node
        int goalX = waypoint.x - unit->lastPosition.x;
        int goalY = waypoint.y - unit->lastPosition.y;

        // Then scale to the desired weight
        if (goalX != 0 || goalY != 0)
        {
            double goalScale = goalWeight / (double) Geo::ApproximateDistance(goalX, 0, goalY, 0);
            goalX = (int) ((double) goalX * goalScale);
            goalY = (int) ((double) goalY * goalScale);
        }

        // Cohesion

        int cohesionX = 0;
        int cohesionY = 0;

        // We ignore the cohesion boid if this unit is separated from the vanguard unit by a narrow choke
        if (unit == vanguard || !PathFinding::SeparatingNarrowChoke(unit->lastPosition,
                                                                    vanguard->lastPosition,
                                                                    unit->type,
                                                                    PathFinding::PathFindingOptions::UseNeighbouringBWEMArea))
        {
            cohesionX = (int) ((double) (center.x - unit->lastPosition.x) * cohesionFactor);
            cohesionY = (int) ((double) (center.y - unit->lastPosition.y) * cohesionFactor);
        }

        // Separation
        int separationX = 0;
        int separationY = 0;
        for (const auto &other : units)
        {
            if (other == unit) continue;

            Boids::AddSeparation(unit.get(), other, separationDetectionLimitFactor, separationWeight, separationX, separationY);
        }

        auto pos = Boids::ComputePosition(unit.get(), {goalX, separationX, cohesionX}, {goalY, separationY, cohesionY}, 80, 48);

        // Default to the goal node if the unit can't move in the direction it wants to
        if (pos == BWAPI::Positions::Invalid)
        {
            pos = waypoint;
        }

#if DEBUG_UNIT_BOIDS
        std::ostringstream nodes;
        auto node = (*grid)[unit->getTilePosition()];
        if (node.nextNode)
        {
            nodes << "; nodes=[" << BWAPI::WalkPosition(node.nextNode->center());
            if (node.nextNode->nextNode)
            {
                nodes << "," << BWAPI::WalkPosition(node.nextNode->nextNode->center());
                if (node.nextNode->nextNode->nextNode)
                {
                    nodes << "," << BWAPI::WalkPosition(node.nextNode->nextNode->nextNode->center());
                }
            }
            nodes << "]";
        }

        CherryVis::log(unit->id) << "Movement boids towards " << BWAPI::WalkPosition(targetPosition)
                                 << "; waypoint=" << BWAPI::WalkPosition(waypoint)
                                 << nodes.str()
                                 << "; cluster=" << BWAPI::WalkPosition(center)
                                 << ": goal=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(goalX, goalY))
                                 << "; cohesion=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(cohesionX, cohesionY))
                                 << "; separation=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(separationX, separationY))
                                 << "; total=" << BWAPI::WalkPosition(
                unit->lastPosition + BWAPI::Position(goalX + cohesionX + separationX, goalY + cohesionY + separationY))
                                 << "; target=" << BWAPI::WalkPosition(pos);
#elif DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Move boids: Moving to " << BWAPI::WalkPosition(pos);
#endif

        unit->moveTo(pos, true);
    }

    return true;
}
