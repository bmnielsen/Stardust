#include "UnitCluster.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Geo.h"

/*
 * Cluster movement
 *
 * When we have a navigation grid (which should always be the case), we implement flocking with three boids:
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
    const double cohesionWeight = 64.0 / sqrt(5.0);
    const double separationDetectionLimitFactor = 2.0;
    const double separationWeight = 96.0;
}

void UnitCluster::move(BWAPI::Position targetPosition)
{
    auto grid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));
    if (!grid)
    {
        Log::Get() << "WARNING: Cluster @ " << BWAPI::WalkPosition(center) << " moving towards position without grid @ "
                   << BWAPI::TilePosition(targetPosition);
    }

    // Get the node at the cluster center
    NavigationGrid::GridNode *centerNode = nullptr;
    if (grid)
    {
        centerNode = &(*grid)[BWAPI::TilePosition(center)];
        while (centerNode && centerNode->cost == USHRT_MAX) centerNode = centerNode->nextNode;
    }

    // Compute scaling factor for cohesion boid, based on the size of the squad
    double cohesionFactor = cohesionWeight * pi / (double) sqrt(area);

    for (auto unit : units)
    {
        auto &myUnit = Units::getMine(unit);

        // If the unit is stuck, unstick it
        if (myUnit.unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit.isReady()) continue;

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
            CherryVis::log(unit) << "Move to target: Moving to " << BWAPI::WalkPosition(targetPosition);
#endif
            myUnit.moveTo(targetPosition);
            continue;
        }

        // We have a grid node, so compute our boids

        // Goal

        // Initialize with the offset to the node
        int goalX = (node->x - myUnit.tilePositionX) * 32;
        int goalY = (node->y - myUnit.tilePositionY) * 32;

        // Then scale to the desired weight
        double goalScale = goalWeight / (double) Geo::ApproximateDistance(goalX, 0, goalY, 0);
        goalX = (int) ((double) goalX * goalScale);
        goalY = (int) ((double) goalY * goalScale);

        // Cohesion

        int cohesionX = 0;
        int cohesionY = 0;

        // We ignore the cohesion boid if the center grid node is at a greatly lower cost, as this indicates a probable cliff
        // between this unit and the rest of the cluster
        if (!centerNode || (node->cost < 150) || (centerNode->cost > (node->cost - 150)))
        {
            cohesionX = (int) ((double) (center.x - unit->getPosition().x) * cohesionFactor);
            cohesionY = (int) ((double) (center.y - unit->getPosition().y) * cohesionFactor);

            // For cases where we have no valid center node, check for unwalkable terrain directly
            if (!centerNode)
            {
                std::vector<BWAPI::TilePosition> tiles;
                Geo::FindTilesBetween(BWAPI::TilePosition(unit->getPosition()), BWAPI::TilePosition(center), tiles);
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
        for (auto other : units)
        {
            if (other == unit) continue;

            auto dist = unit->getDistance(other);
            double detectionLimit = std::max(unit->getType().width(), other->getType().width()) * separationDetectionLimitFactor;
            if (dist >= (int) detectionLimit) continue;

            // We are within the detection limit
            // Push away with maximum force at 0 distance, no force at detection limit
            double distFactor = 1.0 - (double) dist / detectionLimit;
            int centerDist = Geo::ApproximateDistance(unit->getPosition().x, other->getPosition().x, unit->getPosition().y, other->getPosition().y);
            double scalingFactor = distFactor * distFactor * separationWeight / centerDist;
            separationX -= (int) ((double) (other->getPosition().x - unit->getPosition().x) * scalingFactor);
            separationY -= (int) ((double) (other->getPosition().y - unit->getPosition().y) * scalingFactor);
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

        auto pos = BWAPI::Position(unit->getPosition().x + totalX, unit->getPosition().y + totalY);

        // If the target position is in unwalkable terrain, use the grid directly
        if (!Map::isWalkable(BWAPI::TilePosition(pos)))
        {
            pos = BWAPI::Position((node->x << 5U) + 16, (node->y << 5U) + 16);
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Movement boids; cluster=" << BWAPI::WalkPosition(center)
                             << ": goal=" << BWAPI::WalkPosition(unit->getPosition() + BWAPI::Position(goalX, goalY))
                             << "; cohesion=" << BWAPI::WalkPosition(unit->getPosition() + BWAPI::Position(cohesionX, cohesionY))
                             << "; separation=" << BWAPI::WalkPosition(unit->getPosition() + BWAPI::Position(separationX, separationY))
                             << "; total=" << BWAPI::WalkPosition(unit->getPosition() + BWAPI::Position(totalX, totalY))
                             << "; target=" << BWAPI::WalkPosition(pos);
#endif

        myUnit.move(pos);
    }
}
