#include "UnitCluster.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Boids.h"
#include "Map.h"

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
    const int collisionWeight = 64;
    const double cohesionWeight = 64.0;
    const double defaultSeparationDetectionLimitFactor = 2.0;
    const double defaultSeparationWeight = 96.0;
}

bool UnitCluster::moveAsBall(BWAPI::Position targetPosition, std::set<MyUnit> &ballUnits) const
{
    // We require a grid
    auto grid = PathFinding::getNavigationGrid(targetPosition);
    if (!grid) return false;

    // Scaling factor for cohesion boid is based on the size of the squad
    double cohesionFactor = cohesionWeight / sqrt(area / pi);

    // Separation depends on whether the enemy has AOE attackers
    double separationDetectionLimitFactor = defaultSeparationDetectionLimitFactor;
    double separationWeight = defaultSeparationWeight;
    if (enemyAoeRadius > 0)
    {
        separationDetectionLimitFactor = ((double)enemyAoeRadius + 16.0) / 32.0;
        separationWeight = 160;
    }

    for (const auto &unit : ballUnits)
    {
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

        // Collision
        int collisionX = 0;
        int collisionY = 0;

        auto collisionVector = Map::collisionVector(waypoint.x >> 5, waypoint.y >> 5);
        collisionVector = Geo::ScaleVector(collisionVector, collisionWeight);
        if (collisionVector != BWAPI::Positions::Invalid)
        {
            auto collisionTarget = waypoint + collisionVector;
            if (Map::collisionVector(collisionTarget.x >> 5, collisionTarget.y >> 5) == BWAPI::Positions::Origin)
            {
                collisionX = collisionVector.x;
                collisionY = collisionVector.y;
            }
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
        for (const auto &other : ballUnits)
        {
            if (other == unit) continue;
            if (other->isFlying) continue;
            if (other->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;

            Boids::AddSeparation(unit.get(), other, separationDetectionLimitFactor, separationWeight, separationX, separationY);
        }

        auto pos = Boids::ComputePosition(unit.get(),
                                          {goalX, collisionX, separationX, cohesionX},
                                          {goalY, collisionY, separationY, cohesionY},
                                          80,
                                          48);

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
                                 << "; collision=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(collisionX, collisionY))
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
