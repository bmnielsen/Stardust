#include "UnitCluster.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Geo.h"

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
 * TODO: Add some kind of collision with terrain boid
 */

namespace
{
    const double pi = 3.14159265358979323846;

    // TODO: These parameters need to be tuned
    const double goalWeight = 128.0;
    const double cohesionWeight = 64.0;
    const int cohesionIgnoreDistance = 300;
    const double separationDetectionLimitFactor = 2.0;
    const double separationWeight = 96.0;
}

void UnitCluster::move(BWAPI::Position targetPosition)
{
    // Hook to allow the map-specific override to perform the move
    if (Map::mapSpecificOverride()->clusterMove(*this, targetPosition)) return;

    // If any units are either in a leaf area or a narrow choke, do not do flocking
    // In these cases it is very likely for parts of the cluster to get stuck on buildings or terrain, which is not handled well
    bool shouldFlock = true;
    for (const auto &unit : units)
    {
        auto pos = unit->getTilePosition();
        if (Map::isInNarrowChoke(pos) || Map::isInLeafArea(pos))
        {
            shouldFlock = false;
            break;
        }
    }

    // Initialize flocking
    NavigationGrid *grid = nullptr;
    NavigationGrid::GridNode *centerNode = nullptr;
    double cohesionFactor;
    if (shouldFlock)
    {
        grid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));
        if (grid)
        {
            centerNode = &(*grid)[BWAPI::TilePosition(center)];
            while (centerNode && centerNode->cost == USHRT_MAX)
            {
                centerNode = centerNode->nextNode;
            }

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
            if (currentNode.nextNode && currentNode.nextNode->nextNode && currentNode.nextNode->nextNode->nextNode)
            {
                node = currentNode.nextNode->nextNode->nextNode;
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

        // We ignore the cohesion boid if the center grid node is at a greatly lower cost, as this indicates a probable cliff
        // between this unit and the rest of the cluster
        if (!centerNode || (node->cost < cohesionIgnoreDistance) || (centerNode->cost > (node->cost - cohesionIgnoreDistance)))
        {
            cohesionX = (int) ((double) (center.x - unit->lastPosition.x) * cohesionFactor);
            cohesionY = (int) ((double) (center.y - unit->lastPosition.y) * cohesionFactor);

            // For cases where we have no valid center node, check for unwalkable terrain directly
            if (!centerNode)
            {
                std::vector<BWAPI::TilePosition> tiles;
                Geo::FindTilesBetween(BWAPI::TilePosition(unit->lastPosition), BWAPI::TilePosition(center), tiles);
                for (auto tile : tiles)
                {
                    if (!Map::isWalkable(tile))
                    {
                        cohesionX = 0;
                        cohesionY = 0;
                        break;
                    }
                }
            }
        }

        // Separation
        int separationX = 0;
        int separationY = 0;
        for (const auto &other : units)
        {
            if (other == unit) continue;

            auto dist = unit->getDistance(other);
            double detectionLimit = std::max(unit->type.width(), other->type.width()) * separationDetectionLimitFactor;
            if (dist >= (int) detectionLimit) continue;

            // We are within the detection limit
            // Push away with maximum force at 0 distance, no force at detection limit
            double distFactor = 1.0 - (double) dist / detectionLimit;
            int centerDist = Geo::ApproximateDistance(unit->lastPosition.x, other->lastPosition.x, unit->lastPosition.y, other->lastPosition.y);
            double scalingFactor = distFactor * distFactor * separationWeight / centerDist;
            separationX -= (int) ((double) (other->lastPosition.x - unit->lastPosition.x) * scalingFactor);
            separationY -= (int) ((double) (other->lastPosition.y - unit->lastPosition.y) * scalingFactor);
        }

        // Put them all together to get the target direction
        int totalX = goalX + cohesionX + separationX;
        int totalY = goalY + cohesionY + separationY;

        // Cap it at 2 tiles away to edge of large unit
        auto dist = Geo::ApproximateDistance(totalX, 0, totalY, 0);
        if (dist > 80)
        {
            double scale = 80.0 / (double) dist;
            totalX = (int) ((double) totalX * scale);
            totalY = (int) ((double) totalY * scale);
        }

        auto pos = BWAPI::Position(unit->lastPosition.x + totalX, unit->lastPosition.y + totalY);

        // If the target position is in unwalkable terrain, use the grid directly
        if (!pos.isValid() || !Map::isWalkable(BWAPI::TilePosition(pos)))
        {
            pos = BWAPI::Position((node->x << 5U) + 16, (node->y << 5U) + 16);
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Movement boids towards " << BWAPI::WalkPosition(targetPosition)
                                 << "; nodes=[" << BWAPI::WalkPosition((*grid)[unit->getTilePosition()].nextNode->center())
                                 << "," << BWAPI::WalkPosition((*grid)[unit->getTilePosition()].nextNode->nextNode->center())
                                 << "," << BWAPI::WalkPosition((*grid)[unit->getTilePosition()].nextNode->nextNode->nextNode->center())
                                 << "]; cluster=" << BWAPI::WalkPosition(center)
                                 << ": goal=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(goalX, goalY))
                                 << "; cohesion=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(cohesionX, cohesionY))
                                 << "; separation=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(separationX, separationY))
                                 << "; total=" << BWAPI::WalkPosition(unit->lastPosition + BWAPI::Position(totalX, totalY))
                                 << "; target=" << BWAPI::WalkPosition(pos);
#endif

        unit->moveTo(pos, true);
    }
}
