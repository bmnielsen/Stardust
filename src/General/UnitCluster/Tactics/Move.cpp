#include "UnitCluster.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Geo.h"
#include "Boids.h"

#include "DebugFlag_UnitOrders.h"

/*
 * Cluster movement
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

void UnitCluster::move(BWAPI::Position targetPosition)
{
    // Hook to allow the map-specific override to perform the move
    if (Map::mapSpecificOverride()->clusterMove(*this, targetPosition)) return;

    // Determine if the cluster should flock
    // Criteria:
    // - Must be the vanguard cluster
    // - No units may be in a leaf area or narrow choke (as this makes it likely that they will get stuck on buildings or terrain)
    bool shouldFlock = isVanguardCluster;
    if (shouldFlock)
    {
        for (const auto &unit : units)
        {
            auto pos = unit->getTilePosition();
            if (Map::isInNarrowChoke(pos) || Map::isInLeafArea(pos))
            {
                shouldFlock = false;
                break;
            }
        }
    }

    // Initialize flocking
    NavigationGrid *grid = nullptr;
    double cohesionFactor;
    if (shouldFlock)
    {
        grid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));
        if (grid)
        {
            // Scaling factor for cohesion boid is based on the size of the squad
            cohesionFactor = cohesionWeight / sqrt(area / pi);
        }
    }

    for (const auto &unit : units)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

        // Get the grid node to move towards
        // We attempt to move towards the third node ahead of us
        NavigationGrid::GridNode *node = nullptr;
        if (grid)
        {
            auto &currentNode = (*grid)[unit->getTilePosition()];
            if (currentNode.nextNode && currentNode.nextNode->nextNode)
            {
                node = currentNode.nextNode->nextNode;
            }
        }

        // If we don't have a navigation grid or a valid node to navigate with, defer to normal unit move
        // TODO: Perhaps use A* or similar to get a path to the next choke? Expectation though is that we always have a grid.
        if (!node)
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
        int goalX = (node->x - unit->tilePositionX) * 32;
        int goalY = (node->y - unit->tilePositionY) * 32;

        // Then scale to the desired weight
        double goalScale = goalWeight / (double) Geo::ApproximateDistance(goalX, 0, goalY, 0);
        goalX = (int) ((double) goalX * goalScale);
        goalY = (int) ((double) goalY * goalScale);

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
            pos = BWAPI::Position((node->x << 5U) + 16, (node->y << 5U) + 16);
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Movement boids towards " << BWAPI::WalkPosition(targetPosition)
                                 << "; nodes=[" << BWAPI::WalkPosition((*grid)[unit->getTilePosition()].nextNode->center())
                                 << "," << BWAPI::WalkPosition((*grid)[unit->getTilePosition()].nextNode->nextNode->center())
                                 << "]; cluster=" << BWAPI::WalkPosition(center)
                                 << ": goal=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(goalX, goalY))
                                 << "; cohesion=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(cohesionX, cohesionY))
                                 << "; separation=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(separationX, separationY))
                                 << "; total=" << BWAPI::WalkPosition(
                unit->lastPosition + BWAPI::Position(goalX + cohesionX + separationX, goalY + cohesionY + separationY))
                                 << "; target=" << BWAPI::WalkPosition(pos);
#endif

        unit->moveTo(pos, true);
    }
}
