#include "UnitCluster.h"

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

void UnitCluster::containStatic(std::set<Unit> &enemyUnits,
                                BWAPI::Position targetPosition)
{
    /* Perform target selection again to get targets we can attack safely */
    auto unitsAndTargets = selectTargets(enemyUnits, targetPosition, true);

    auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
    auto navigationGrid = Pathfinder::getNavigationGrid(targetPosition, true);

    /* Do an initial scan to filter out not-ready units and gather their nearest threats */
    /* The important thing to track is whether any of our units is in range of static defense */
    std::vector<std::tuple<OwnedUnit, Unit, Unit>> unitsAndTargetsAndThreats;
    bool anyInRangeOfStaticDefense = false;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;
        if (myUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;

        /* If the unit is stuck, unstick it */
        if (myUnit->unstick()) continue;

        /* If the unit is not ready (i.e. is already in the middle of an attack), don't touch it */
        if (!myUnit->isReady()) continue;

        /* Determine if this unit is being threatened by something */
        Unit threat = nullptr;
        bool staticDefenseThreat = false;
        {
            int closestDist = INT_MAX;
            int myRange = myUnit->groundRange();
            for (auto &gameUnit : enemyUnits)
            {
                if (!gameUnit->completed) continue;
                if (!gameUnit->canAttack(myUnit)) continue;

                int enemyRange = gameUnit->groundRange();

                /* For static defense, just take any we are almost in range of */
                if (gameUnit->isStaticGroundDefense())
                {
                    int span = myUnit->getDistance(gameUnit);
                    if (span > (enemyRange + 16)) continue;

                    if (span < closestDist || !staticDefenseThreat)
                    {
                        threat = gameUnit;
                        closestDist = span;
                        staticDefenseThreat = true;
                    }

                    continue;
                }

                /* Ignore non-static-defense if we have found a static defense threat */
                if (staticDefenseThreat) continue;

                /* Don't worry about units with a lower range */
                if (enemyRange <= myRange) continue;

                int span = myUnit->getDistance(gameUnit, gameUnit->simPosition);

                /* Don't worry about units that are further away from what we have already found */
                if (span >= closestDist) continue;

                /* Don't worry about units we can attack */
                if (span <= myRange) continue;

                /* Don't worry about units that are well out of their attack range */
                if (span > (enemyRange + 32)) continue;

                threat = gameUnit;
                closestDist = span;
            }
        }

        unitsAndTargetsAndThreats.emplace_back(myUnit, unitAndTarget.second, threat);
        anyInRangeOfStaticDefense = anyInRangeOfStaticDefense || staticDefenseThreat;
    }

    /* Now perform micro on the filtered unit list */
    for (const auto &unitAndTargetAndThreat : unitsAndTargetsAndThreats)
    {
        auto &myUnit = std::get<0>(unitAndTargetAndThreat);
        auto &target = std::get<1>(unitAndTargetAndThreat);
        auto &threat = std::get<2>(unitAndTargetAndThreat);

        /* If no units are in range of static defense and this unit is in range of its target, attack */
        if (!anyInRangeOfStaticDefense && target && myUnit->isInOurWeaponRange(target))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(myUnit->ident) << "Contain: Attacking " << *target;
#endif
            myUnit->attackUnit(target, unitsAndTargets, false, enemyAoeRadius);
            continue;
        }

        /* Move to maintain the contain */

        /* Goal boid: stay just out of range of enemy threats, oriented towards the target position */
        bool pullingBack = false;
        int goalX = 0;
        int goalY = 0;

        /* If in range of a threat, just move away from it */
        if (threat)
        {
            auto scaled = Geom::ScaleVector(myUnit->prevPos - threat->prevPos, goalWeightInRangeOfThreat);
            if (scaled != BWAPI::Positions::Invalid)
            {
                goalX = scaled.x;
                goalY = scaled.y;
            }
            pullingBack = true;
        }
        else
        {
            /* Detect if we are inside a narrow choke that is threatened at one end but not the other */
            /* In this case we would normally want to contain inside the choke, which is undesirable */
            Choke *insideChoke = nullptr;
            if (Map::isInNarrowChoke(myUnit->getTilePosition()))
            {
                for (const auto &choke : Map::allChokes())
                {
                    if (!choke->isNarrowChoke) continue;
                    if (myUnit->getDistance(choke->midpoint) > 640) continue;
                    if (choke->chokeTiles.contains(myUnit->getTilePosition()))
                    {
                        insideChoke = choke;
                    }
                }

                if (!insideChoke)
                {
                    AppLog::Get() << "ERROR: " << *myUnit << ": isInNarrowChoke without being able to find choke!";
                }
                else if (!(grid.staticGroundThreat(insideChoke->end1Center) == 0 && grid.staticGroundThreat(insideChoke->end2Center) > 0) &&
                         !(grid.staticGroundThreat(insideChoke->end2Center) == 0 && grid.staticGroundThreat(insideChoke->end1Center) > 0))
                {
                    insideChoke = nullptr;
                }
            }

            /* Get grid nodes if they are available */
            auto node = navigationGrid ? &(*navigationGrid)[myUnit->getTilePosition()] : nullptr;
            auto nextNode = node ? node->nextNode : nullptr;
            auto secondNode = nextNode ? nextNode->nextNode : nullptr;

            BWAPI::Position nextNodeCenter, secondNodeCenter;
            if (secondNode)
            {
                nextNodeCenter = nextNode->midpoint();
                secondNodeCenter = secondNode->midpoint();
#if DEBUG_UNIT_BOIDS
                Telemetry::log(myUnit->ident) << "Contain (goal boid):"
                                           << " nextNode=" << BWAPI::WalkPosition(nextNodeCenter) << " (" << nextNode->cost << ")"
                                           << "; secondNode=" << BWAPI::WalkPosition(secondNodeCenter) << " (" << secondNode->cost << ")";
#endif
            }
            else
            {
                secondNodeCenter = Pathfinder::NextGridOrChokeWaypoint(myUnit->prevPos, targetPosition, nullptr, 2);
                if (secondNodeCenter == BWAPI::Positions::Invalid)
                {
                    secondNodeCenter = myUnit->prevPos + Geom::ScaleVector(targetPosition - myUnit->prevPos, 64);
                    if (secondNodeCenter == BWAPI::Positions::Invalid)
                    {
                        secondNodeCenter = targetPosition;
                    }
                }

                nextNodeCenter = BWAPI::Position((myUnit->prevPos.x + secondNodeCenter.x) >> 1,
                                                 (myUnit->prevPos.y + secondNodeCenter.y) >> 1);

#if DEBUG_UNIT_BOIDS
                Telemetry::log(myUnit->ident) << "Contain (goal boid): No valid navigation path, moving relative to choke"
                                           << ": nextNode=" << BWAPI::WalkPosition(nextNodeCenter)
                                           << "; secondNode=" << BWAPI::WalkPosition(secondNodeCenter);
#endif
            }

            /* Move towards the second node if none are under threat or in a narrow choke */
            /* Move away from the second node if the next node is under threat or in a narrow choke */
            /* Do nothing if the first node is not under threat or in a narrow choke and the second node is */
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
                auto scaled = Geom::ScaleVector(secondNodeCenter - myUnit->prevPos, length);
                if (scaled != BWAPI::Positions::Invalid)
                {
                    goalX = scaled.x;
                    goalY = scaled.y;
                }
            }
        }

        /* Separation boid */
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

        auto pt = Boids::ComputePosition(myUnit.get(), {goalX, separationX}, {goalY, separationY}, 80);

#if DEBUG_UNIT_BOIDS
        Telemetry::log(myUnit->ident) << "Contain boids towards " << BWAPI::WalkPosition(targetPosition)
                                   << "; cluster=" << BWAPI::WalkPosition(midpoint)
                                   << ": goal=" << BWAPI::WalkPosition(myUnit->prevPos + BWAPI::Position(goalX, goalY))
                                   << "; separation=" << BWAPI::WalkPosition(myUnit->prevPos + BWAPI::Position(separationX, separationY))
                                   << "; total="
                                   << BWAPI::WalkPosition(myUnit->prevPos + BWAPI::Position(goalX + separationX, goalY + separationY))
                                   << "; target=" << BWAPI::WalkPosition(pt)
                                   << "; pullingBack=" << pullingBack;
#elif DEBUG_UNIT_ORDERS
        if (pt == BWAPI::Positions::Invalid)
        {
            Telemetry::log(myUnit->ident) << "Contain boids: Invalid; moving to "
                                       << BWAPI::WalkPosition(pullingBack ? Map::getMyMain()->getPosition() : targetPosition);
        }
        else
        {
            Telemetry::log(myUnit->ident) << "Contain boids: Moving to " << BWAPI::WalkPosition(pt);
        }
#endif

        /* If the unit can't move in the desired direction, either move towards the target or towards our main */
        /* depending on whether we are pulling back */
        if (pt == BWAPI::Positions::Invalid)
        {
            myUnit->moveTo(pullingBack ? Map::getMyMain()->getPosition() : targetPosition);
        }
        else if (pullingBack && grid.staticGroundThreat(pt) > 0)
        {
            /* This handles the case where the unit wants to pull back, but the position is threatened */
            myUnit->moveTo(Map::getMyMain()->getPosition());
        }
        else
        {
            myUnit->moveTo(pt, true);
        }
    }
}
