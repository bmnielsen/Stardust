#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"

/*
 * Contains a base, here defined as somewhere the enemy has static defense.
 *
 * We basically just order our units to fan out just outside of enemy threat range, attacking anything that comes into range of our units.
 */

namespace
{
    const int goalWeight = 64;
    const int goalWeightInRangeOfStaticDefense = 128;
    const double separationDetectionLimitFactor = 1.5;
    const double separationWeight = 96.0;
}

void UnitCluster::containBase(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                              std::set<Unit> &enemyUnits,
                              BWAPI::Position targetPosition)
{
    auto &grid = Players::grid(BWAPI::Broodwar->enemy());
    auto navigationGrid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));

    for (const auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;

        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        bool inRangeOfStaticDefense = false;
        int closestStaticDefenseDist = INT_MAX;
        Unit closestStaticDefense = nullptr;
        for (auto &unit : enemyUnits)
        {
            if (!unit->isStaticGroundDefense()) continue;

            int dist = myUnit->getDistance(unit);
            if (dist < closestStaticDefenseDist)
            {
                closestStaticDefenseDist = dist;
                closestStaticDefense = unit;
            }

            if (myUnit->isInEnemyWeaponRange(unit)) inRangeOfStaticDefense = true;
        }

        // If this unit is not in range of static defense and is in range of its target, attack
        if (unitAndTarget.second && !inRangeOfStaticDefense && myUnit->isInOurWeaponRange(unitAndTarget.second))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Contain: Attacking " << *unitAndTarget.second;
#endif
            myUnit->attackUnit(unitAndTarget.second, unitsAndTargets, false);
            continue;
        }

        // Move to maintain the contain

        // Goal boid: stay just out of range of enemy threats, oriented towards the target position
        bool pullingBack = false;
        int goalX = 0;
        int goalY = 0;

        // If in range of static defense, just move away from it
        if (inRangeOfStaticDefense)
        {
            auto scaled = Geo::ScaleVector(myUnit->lastPosition - closestStaticDefense->lastPosition,
                                           goalWeightInRangeOfStaticDefense);
            if (scaled != BWAPI::Positions::Invalid)
            {
                goalX = scaled.x;
                goalY = scaled.y;
            }
            pullingBack = true;

#if DEBUG_UNIT_ORDERS
            CherryVis::log(myUnit->id) << "Contain (goal boid): Moving away from " << BWAPI::WalkPosition(closestStaticDefense->lastPosition);
#endif
        }
        else
        {
            Choke *insideChoke = nullptr;
            if (Map::isInNarrowChoke(myUnit->getTilePosition()))
            {
                // We don't want to contain inside a narrow choke, so if we are in one, we may want to move back
                for (const auto &choke : Map::allChokes())
                {
                    if (!choke->isNarrowChoke) continue;
                    if (myUnit->getDistance(choke->center) > 640) continue;
                    if (choke->chokeTiles.find(myUnit->getTilePosition()) != choke->chokeTiles.end())
                    {
                        insideChoke = choke;
                    }
                }

                if (!insideChoke)
                {
                    Log::Get() << "ERROR: " << *myUnit << ": isInNarrowChoke without being able to find choke!";
                }
                else if (!(grid.staticGroundThreat(insideChoke->end1Center) == 0 && grid.staticGroundThreat(insideChoke->end2Center) > 0) &&
                         !(grid.staticGroundThreat(insideChoke->end2Center) == 0 && grid.staticGroundThreat(insideChoke->end1Center) > 0))
                {
                    insideChoke = nullptr;
                }
            }

            // Get grid nodes if they are available
            auto node = navigationGrid ? &(*navigationGrid)[myUnit->getTilePosition()] : nullptr;
            auto nextNode = node ? node->nextNode : nullptr;
            auto secondNode = nextNode ? nextNode->nextNode : nullptr;

            BWAPI::Position nextNodeCenter, secondNodeCenter;
            if (secondNode)
            {
                nextNodeCenter = nextNode->center();
                secondNodeCenter = secondNode->center();
                CherryVis::log(myUnit->id) << "Contain (goal boid):"
                                           << " nextNode=" << BWAPI::WalkPosition(nextNodeCenter) << " (" << nextNode->cost << ")"
                                           << "; secondNode=" << BWAPI::WalkPosition(secondNodeCenter) << " (" << secondNode->cost << ")";
            }
            else
            {
                // We hit this if we have no valid path to the base we are attacking, which might happen if the enemy walls it off
                // So we use a vector towards the next choke instead
                // If the next choke is very close, use the one after that instead
                auto path = PathFinding::GetChokePointPath(myUnit->lastPosition, targetPosition, myUnit->type);
                BWAPI::Position waypoint = BWAPI::Positions::Invalid;
                for (const auto &bwemChoke : path)
                {
                    auto chokeCenter = Map::choke(bwemChoke)->center;
                    if (myUnit->getDistance(chokeCenter) > 128)
                    {
                        waypoint = chokeCenter;
                        break;
                    }
                }
                if (!waypoint.isValid()) waypoint = targetPosition;

                auto vector = waypoint - myUnit->lastPosition;
                nextNodeCenter = myUnit->lastPosition + Geo::ScaleVector(vector, 32);
                secondNodeCenter = myUnit->lastPosition + Geo::ScaleVector(vector, 64);

#if DEBUG_UNIT_ORDERS
                CherryVis::log(myUnit->id) << "Contain (goal boid): No valid navigation path, moving relative to " << BWAPI::WalkPosition(waypoint)
                                           << " nextNode=" << BWAPI::WalkPosition(nextNodeCenter)
                                           << "; secondNode=" << BWAPI::WalkPosition(secondNodeCenter);
#endif
            }

            // Move towards the second node if none are under threat or in a narrow choke
            // Move away from the second node if the next node is under threat or in a narrow choke
            // Do nothing if the first node is not under threat or in a narrow choke and the second node is
            int length = goalWeight;
            if (grid.staticGroundThreat(nextNodeCenter) > 0 || (insideChoke && Map::isInNarrowChoke(BWAPI::TilePosition(nextNodeCenter))))
            {
                length = -goalWeight;
                pullingBack = true;
            }
            else if (grid.staticGroundThreat(secondNodeCenter) > 0 || (insideChoke && Map::isInNarrowChoke(BWAPI::TilePosition(nextNodeCenter))))
            {
                length = 0;
            }
            if (length != 0)
            {
                auto scaled = Geo::ScaleVector(secondNodeCenter - myUnit->lastPosition, length);
                if (scaled != BWAPI::Positions::Invalid)
                {
                    goalX = scaled.x;
                    goalY = scaled.y;
                }
            }
        }

        // Separation boid
        int separationX = 0;
        int separationY = 0;
        for (const auto &otherUnitAndTarget : unitsAndTargets)
        {
            if (otherUnitAndTarget.first == myUnit) continue;

            auto other = otherUnitAndTarget.first;

            auto dist = myUnit->getDistance(other);
            double detectionLimit = std::max(myUnit->type.width(), other->type.width()) * separationDetectionLimitFactor;
            if (dist >= (int) detectionLimit) continue;

            // We are within the detection limit
            // Push away with maximum force at 0 distance, no force at detection limit
            double distFactor = 1.0 - (double) dist / detectionLimit;
            auto vector = Geo::ScaleVector(myUnit->lastPosition - other->lastPosition,
                                           (int) (distFactor * distFactor * separationWeight));
            if (vector != BWAPI::Positions::Invalid)
            {
                separationX += vector.x;
                separationY += vector.y;
            }
        }

        // Put them all together to get the target direction
        int totalX = goalX + separationX;
        int totalY = goalY + separationY;
        auto total = Geo::ScaleVector(BWAPI::Position(totalX, totalY), 80);
        auto pos = total == BWAPI::Positions::Invalid ? myUnit->lastPosition : (myUnit->lastPosition + total);

#if DEBUG_UNIT_ORDERS
        CherryVis::log(myUnit->id) << "Contain boids towards " << BWAPI::WalkPosition(targetPosition)
                                   << "; cluster=" << BWAPI::WalkPosition(center)
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(separationX, separationY))
                                   << "; total=" << BWAPI::WalkPosition(myUnit->lastPosition + BWAPI::Position(totalX, totalY))
                                   << "; target=" << BWAPI::WalkPosition(pos)
                                   << "; pullingBack=" << pullingBack;
#endif

        // If the position is invalid or unwalkable, either move towards the target or towards our main depending on whether we are pulling back
        if (!pos.isValid() || !Map::isWalkable(BWAPI::TilePosition(pos)))
        {
            myUnit->moveTo(pullingBack ? Map::getMyMain()->getPosition() : targetPosition);
        }
        else
        {
            myUnit->moveTo(pos, true);
        }
    }

}
