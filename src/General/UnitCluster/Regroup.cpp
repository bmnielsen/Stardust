#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"

/*
 * Orders a cluster to regroup.
 *
 * There are several regrouping strategies that can be chosen based on the situation:
 * - Contain: The enemy is considered to be mainly static, so retreat to a safe distance and attack anything that comes into range.
 * - Pull back: Retreat to a location that is more easily defensible (choke, high ground) and set up appropriately.
 * - Flee: Move back towards our main base until we are reinforced.
 * - Explode: Send the cluster units in multiple directions to confuse the enemy.
 *
 * TODO: Consider how a fleeing cluster should move (does it make sense to use flocking?)
 */

namespace
{
    // TODO: These parameters need to be tuned
//    const double goalWeight = 128.0;
}

void UnitCluster::regroup(std::set<Unit> &enemyUnits, BWAPI::Position targetPosition)
{
    auto &grid = Players::grid(BWAPI::Broodwar->enemy());

    // Temporary logic: regroup to either the unit closest to or furthest from to the order position that isn't under threat
    MyUnit closest = nullptr;
    MyUnit furthest = nullptr;
    int closestDist = INT_MAX;
    int furthestDist = 0;
    bool unitUnderThreat = false;
    for (auto &unit : units)
    {
        if (grid.groundThreat(unit->lastPosition) > 0)
        {
            unitUnderThreat = true;
            continue;
        }

        int dist = PathFinding::GetGroundDistance(unit->lastPosition, targetPosition, unit->type);
        if (dist < closestDist)
        {
            closestDist = dist;
            closest = unit;
        }
        if (dist > furthestDist)
        {
            furthestDist = dist;
            furthest = unit;
        }
    }

    auto regroupPosition = Map::getMyMain()->getPosition();
    if (unitUnderThreat && furthest) regroupPosition = furthest->lastPosition;
    else if (!unitUnderThreat && closest) regroupPosition = closest->lastPosition;

    for (const auto &unit : units)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Regroup: Moving to " << BWAPI::WalkPosition(regroupPosition);
#endif
        unit->moveTo(regroupPosition);
    }

    /*

    // First determine what type of regroup strategy we want to use


    auto &grid = Players::grid(BWAPI::Broodwar->enemy());

    // First try to find one of our units that is close to the order position but isn't under threat
    MyUnit best = nullptr;
    int bestDist = INT_MAX;
    for (auto &unit : units)
    {
        if (grid.groundThreat(unit->lastPosition) > 0) continue;

        int dist = PathFinding::GetGroundDistance(unit->lastPosition, targetPosition, unit->type);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = unit;
        }
    }

    // If such a unit did not exist, retreat to our main base
    // TODO: Avoid retreating through enemy army, proper pathing, link up with other clusters, etc.
    if (!best)
    {
        auto regroupPosition = Map::getMyMain()->getPosition();
        for (const auto &unit : units)
        {
            // If the unit is stuck, unstick it
            if (unit->unstick()) continue;

            // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
            if (!unit->isReady()) continue;

#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Regroup: Moving to " << BWAPI::WalkPosition(regroupPosition);
#endif
            unit->moveTo(regroupPosition);
        }

        return;
    }

    // Move our units towards the found location
    // Use boids to have the units keep distance from each other, but stay on non-threatened tiles

    for (const auto &unit : units)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

        // If the unit is not close to the target position, just move towards it
        // We get the choke path here since we may need it later anyway
        int dist;
        auto chokePath = PathFinding::GetChokePointPath(unit->lastPosition,
                                                        best->lastPosition,
                                                        unit->type,
                                                        PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                        &dist);
        if (dist > 128)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Regroup: Moving to " << BWAPI::WalkPosition(best->lastPosition);
#endif
            unit->moveTo(best->lastPosition);
            continue;
        }

        // Goal

        // The position we want to move to is either the best unit's position or the first narrow choke on the way to it
        auto goalPos = best->lastPosition;
        if (!chokePath.empty())
        {
            auto choke = Map::choke(*chokePath.begin());
            if (choke->width < 96 && unit->getDistance(choke->Center()) > 32) goalPos = choke->Center();
        }

        // Initialize the goal vector to the difference between the positions
        int goalX = goalPos.x - unit->lastPosition.x;
        int goalY = goalPos.y - unit->lastPosition.y;

        // Then scale to the desired weight
        double goalScale = goalWeight / (double) Geo::ApproximateDistance(goalX, 0, goalY, 0);
        goalX = (int) ((double) goalX * goalScale);
        goalY = (int) ((double) goalY * goalScale);

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

    }



    // Find a suitable retreat location:
    // - Unit furthest from the order position that is not under threat
    // - If no such unit exists, our main base
    // TODO: Avoid retreating through enemy army, proper pathing, link up with other clusters, etc.
    BWAPI::Position rallyPoint = Map::getMyMain()->getPosition();
    int bestDist = 0;
    for (auto &unit : cluster.units)
    {
        if (grid.groundThreat(unit->lastPosition) > 0) continue;

        int dist = PathFinding::GetGroundDistance(unit->lastPosition, targetPosition, unit->type);
        if (dist > bestDist)
        {
            bestDist = dist;
            rallyPoint = unit->lastPosition;
        }
    }

    cluster.regroup(rallyPoint);

    for (const auto &unit : units)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Regroup: Moving to " << BWAPI::WalkPosition(regroupPosition);
#endif
        unit->moveTo(regroupPosition);
    }

     */
}
