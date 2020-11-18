#include "UnitCluster.h"

#include "Units.h"
#include "Players.h"
#include "Map.h"
#include "Geo.h"
#include "Boids.h"

#include "DebugFlag_UnitOrders.h"

/*
 * Contains a base, here defined as somewhere the enemy has static defense.
 *
 * We basically just order our units to fan out just outside of enemy threat range, attacking anything that comes into range of our units.
 */

namespace
{
    const int goalWeight = 64;
    const int goalWeightInRangeOfThreat = 128;
    const double separationDetectionLimitFactor = 1.5;
    const double separationWeight = 96.0;
}

void UnitCluster::containBase(std::set<Unit> &enemyUnits,
                              BWAPI::Position targetPosition)
{
    // Perform target selection again to get targets we can attack safely
    auto unitsAndTargets = selectTargets(enemyUnits, targetPosition, true);

    auto &grid = Players::grid(BWAPI::Broodwar->enemy());
    auto navigationGrid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));

    for (const auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;

        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        // Determine if this unit is being threatened by something
        Unit threat = nullptr;
        bool staticDefenseThreat = false;
        {
            int closestDist = INT_MAX;
            int myRange = myUnit->groundRange();
            for (auto &unit : enemyUnits)
            {
                if (!unit->completed) continue;
                if (!unit->canAttack(myUnit)) continue;

                int enemyRange = unit->groundRange();

                // For static defense, just take any we are almost in range of
                if (unit->isStaticGroundDefense())
                {
                    int dist = myUnit->getDistance(unit);
                    if (dist > (enemyRange + 16)) continue;

                    if (dist < closestDist || !staticDefenseThreat)
                    {
                        threat = unit;
                        closestDist = dist;
                        staticDefenseThreat = true;
                    }

                    continue;
                }

                // Ignore non-static-defense if we have found a static defense threat
                if (staticDefenseThreat) continue;

                // Don't worry about units with a lower range
                if (enemyRange <= myRange) continue;

                int dist = myUnit->getDistance(unit);

                // Don't worry about units that are further away from what we have already found
                if (dist >= closestDist) continue;

                // Don't worry about units we can attack
                if (dist <= myRange) continue;

                // Don't worry about units that are well out of their attack range
                if (dist > (enemyRange + 32)) continue;

                threat = unit;
                closestDist = dist;
            }
        }

        // If this unit is not in range of static defense and is in range of its target, attack
        if (unitAndTarget.second && !staticDefenseThreat && myUnit->isInOurWeaponRange(unitAndTarget.second))
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

        // If in range of a threat, just move away from it
        if (threat)
        {
            auto scaled = Geo::ScaleVector(myUnit->lastPosition - threat->lastPosition, goalWeightInRangeOfThreat);
            if (scaled != BWAPI::Positions::Invalid)
            {
                goalX = scaled.x;
                goalY = scaled.y;
            }
            pullingBack = true;
        }
        else
        {
            // Detect if we are inside a narrow choke that is threatened at one end but not the other
            // In this case we would normally want to contain inside the choke, which is undesirable
            Choke *insideChoke = nullptr;
            if (Map::isInNarrowChoke(myUnit->getTilePosition()))
            {
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
            if (nextNodeCenter.isValid() &&
                (grid.staticGroundThreat(nextNodeCenter) > 0 || (insideChoke && Map::isInNarrowChoke(BWAPI::TilePosition(nextNodeCenter)))))
            {
                length = -goalWeight;
                pullingBack = true;
            }
            else if (secondNodeCenter.isValid() &&
                     (grid.staticGroundThreat(secondNodeCenter) > 0 || (insideChoke && Map::isInNarrowChoke(BWAPI::TilePosition(nextNodeCenter)))))
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
        if (!pullingBack)
        {
            for (const auto &otherUnitAndTarget : unitsAndTargets)
            {
                if (otherUnitAndTarget.first == myUnit) continue;

                Boids::AddSeparation(myUnit.get(),
                                     otherUnitAndTarget.first,
                                     separationDetectionLimitFactor,
                                     separationWeight,
                                     separationX,
                                     separationY);
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
