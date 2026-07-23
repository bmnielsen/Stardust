#include "Squad.h"

#include "Players.h"
#include "Map.h"
#include "Geo.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"
#include "Opponent.h"
#include "Strategist.h"

/*
 * For now detectors just try to keep detection on the closest unit to them requiring detection. If there are no units requiring detection,
 * they try to stay on top of the squad vanguard unit (cluster vanguard unit closest to the target position).
 *
 * In both cases, they try to avoid being in areas where the enemy has both detection and an anti-air threat.
 */

namespace
{
    const int HALT_DISTANCE = UnitUtil::HaltDistance(BWAPI::UnitTypes::Protoss_Observer) + 16;

    BWAPI::Position scaledPosition(BWAPI::Position currentPosition, BWAPI::Position vector, int length)
    {
        auto scaledVector = Geo::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void moveAwayFrom(MyObserver &detector, BWAPI::Position target)
    {
        // Move in the opposite direction
        auto behind = scaledPosition(detector->lastPosition, detector->lastPosition - target, HALT_DISTANCE);
        if (behind.isValid())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(detector->id) << "Moving away from target @ " << BWAPI::WalkPosition(target);
#endif
            detector->moveTo(behind);
            return;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(detector->id) << "Cannot move away from target @ " << BWAPI::WalkPosition(target) << "; default to main";
#endif

        // Default to main base location when we don't have anywhere better to go
        detector->moveTo(Map::getMyMain()->getPosition());
    }

    void moveTowards(MyObserver &detector, BWAPI::Position target, BWAPI::Position threatDirection)
    {
        // Check for threats one-and-a-half tiles ahead
        auto ahead = scaledPosition(detector->lastPosition, target - detector->lastPosition, 48);
#if DEBUG_UNIT_ORDERS
        CherryVis::log(detector->id) << "Me " << detector->lastPosition << "; target " << target
                                     << "; diff " << (target - detector->lastPosition) << "; scaled " << ahead;
#endif
        if (ahead.isValid())
        {
            // Avoid all threats if detected, and avoid threats regardless if a Terran opponent has a comsat
            auto &grid = Players::grid(BWAPI::Broodwar->enemy());
            std::function<bool(BWAPI::Position)> isThreat;
            if (Players::hasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Scanner_Sweep))
            {
                isThreat = [&](BWAPI::Position pos)
                {
                    return grid.airThreat(pos) > 0;
                };
            }
            else
            {
                isThreat = [&](BWAPI::Position pos)
                {
                    return (grid.airThreat(pos) > 0 && grid.detection(pos) > 0);
                };
            }

            if (isThreat(ahead))
            {
                // Try to find the closest position to the target that is valid and safe
                std::vector<BWAPI::Position> candidates;
                auto vector = ahead - detector->lastPosition;
                auto perpendicular = BWAPI::Position(vector.y, vector.x);
                auto scaleAndAdd = [&](BWAPI::Position pos)
                {
                    for (int length = 48; length <= 112; length += 32)
                    {
                        auto scaledVector = Geo::ScaleVector(pos - detector->lastPosition, length);
                        if (scaledVector != BWAPI::Positions::Invalid)
                        {
                            candidates.emplace_back(detector->lastPosition + scaledVector);
                        }
                    }
                };
                auto processPerpendicularPosition = [&](BWAPI::Position pos)
                {
                    scaleAndAdd((ahead + pos) / 2);
                    scaleAndAdd((ahead + ahead + pos) / 3);
                    scaleAndAdd((ahead + pos + pos) / 3);
                    candidates.emplace_back(pos);
                };
                processPerpendicularPosition(detector->lastPosition + perpendicular);
                processPerpendicularPosition(detector->lastPosition - perpendicular);

                BWAPI::Position best = BWAPI::Positions::Invalid;
                int bestDist = INT_MAX;
                for (const auto &pos : candidates)
                {
                    if (!pos.isValid()) continue;
                    if (isThreat(pos)) continue;

                    int dist = pos.getApproxDistance(target);
                    if (dist < bestDist)
                    {
                        best = pos;
                        bestDist = dist;
                    }
                }
                if (best != BWAPI::Positions::Invalid)
                {
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(detector->id) << "Threat detected, moving towards safe position @ " << BWAPI::WalkPosition(best);
#endif

                    detector->moveTo(best);
                    return;
                }

#if DEBUG_UNIT_ORDERS
                CherryVis::log(detector->id) << "Threat detected, moving away from threat direction @ " << BWAPI::WalkPosition(threatDirection);
#endif
                moveAwayFrom(detector, threatDirection);
                return;
            }
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(detector->id) << "Moving towards target @ " << BWAPI::WalkPosition(target);
#endif
        // Scale to our halt distance
        auto scaledVector = Geo::ScaleVector(target - detector->lastPosition, HALT_DISTANCE);
        auto scaledTarget = (scaledVector == BWAPI::Positions::Invalid) ? target : detector->lastPosition + scaledVector;
        if (!scaledTarget.isValid()) scaledTarget = target;
        if (!scaledTarget.isValid()) scaledTarget = Map::getMyMain()->getPosition();
        detector->moveTo(scaledTarget);
    }
}

void Squad::executeDetectors()
{
    auto pendingDetectors = detectors;

    // Start by having detectors move towards any enemies requiring detection
    std::set<Unit> enemiesBeingDetected;
    for (auto detector : detectors)
    {
        // Try to find the nearest enemy requiring detection
        Unit closest = nullptr;
        int closestDist = INT_MAX;
        for (auto &enemy : enemiesNeedingDetection)
        {
            // Skip it if it is within 3 tiles of an enemy already being handled by another detector
            if (std::any_of(enemiesBeingDetected.begin(),
                            enemiesBeingDetected.end(),
                            [&enemy](const Unit &enemyBeingDetected)
                            {
                                return enemy->getDistance(enemyBeingDetected) < 96;
                            }))
            {
                continue;
            }

            int dist = detector->getDistance(enemy);
            if (dist < closestDist)
            {
                closest = enemy;
                closestDist = dist;
            }
        }

        // If we found one, either move towards it if it isn't in range, or away from it if we're too close
        if (closest)
        {
            Opponent::setHasBuiltUnitRequiringDetection();

            enemiesBeingDetected.insert(closest);

            if (closestDist > 64)
            {
                moveTowards(detector, closest->lastPosition, closest->lastPosition);
            }
            else
            {
                moveAwayFrom(detector, closest->lastPosition);
            }

            detector->setActivity(ObserverActivity::DetectingEnemy);

            pendingDetectors.erase(detector);
        }
    }
    if (pendingDetectors.empty()) return;

    // If we don't have a vanguard cluster, just move any remaining detectors to our main
    if (!currentVanguardCluster || !currentVanguardCluster->vanguard)
    {
        for (auto detector : pendingDetectors)
        {
            detector->setActivity(ObserverActivity::None);
            moveTowards(detector, Map::getMyMain()->getPosition(), targetPosition);
        }
        return;
    }

    auto moveToArmyVanguard = [&](MyObserver detector)
    {
        auto centerToVanguardVector = currentVanguardCluster->vanguard->lastPosition - currentVanguardCluster->center;
        moveTowards(
            detector,
            currentVanguardCluster->vanguard->lastPosition,
            currentVanguardCluster->center + centerToVanguardVector + centerToVanguardVector);
    };

    // Assign any detectors that are a long way away from the army, or have shield damage, to hang out with the vanguard
    bool assignedOneToEscort = false;
    for (auto it = pendingDetectors.begin(); it != pendingDetectors.end(); )
    {
        if ((*it)->lastShields < (*it)->type.maxShields())
        {
            assignedOneToEscort = true;
            (*it)->setActivity(ObserverActivity::EscortingArmy);
            moveToArmyVanguard(*it);
            it = pendingDetectors.erase(it);
            continue;
        }

        int distToTarget = (*it)->getDistance(targetPosition);
        if (distToTarget > (vanguardClusterDistToTargetPosition + 480))
        {
            int distToVanguard = (*it)->getDistance(currentVanguardCluster->vanguard);
            if (distToVanguard > 640)
            {
                assignedOneToEscort = true;
                (*it)->setActivity(ObserverActivity::EscortingArmy);
                moveToArmyVanguard(*it);
                it = pendingDetectors.erase(it);
                continue;
            }
        }

        ++it;
    }
    if (pendingDetectors.empty()) return;

    // If we have previously needed to detect an enemy, keep one detector with our vanguard in case we have to again
    if (!assignedOneToEscort && Opponent::hasBuiltUnitRequiringDetection())
    {
        // Pick the closest one, but keep an observer that is already assigned to it
        MyObserver best = nullptr;
        int bestDist = INT_MAX;
        for (const auto &detector : pendingDetectors)
        {
            if (assignedOneToEscort && detector->getActivity() != ObserverActivity::EscortingArmy) continue;

            int dist = detector->getDistance(currentVanguardCluster->vanguard);

            if (!assignedOneToEscort && detector->getActivity() == ObserverActivity::EscortingArmy)
            {
                assignedOneToEscort = true;
                best = detector;
                bestDist = dist;
                continue;
            }

            if (dist < bestDist)
            {
                best = detector;
                bestDist = dist;
            }
        }
        if (best)
        {
            best->setActivity(ObserverActivity::EscortingArmy);
            moveToArmyVanguard(best);
            pendingDetectors.erase(best);
        }
    }
    if (pendingDetectors.empty()) return;

    // We have one or more observers that don't have any other priorities, so try to get as much scouting information as possible
    // On open ground, we try to scout the rear of the enemy army to get full tabs on units in the fog and see reinforcements coming in
    // When containing the enemy at their natural, we try to scout their natural and into their main to see reinforcements, cliffed tanks, etc.
    if (Strategist::isEnemyContained())
    {
        // The two places we want to scout are, in order of priority, the area around the enemy's main choke and the target position
        auto choke = Map::getEnemyMainChoke();
        if (choke)
        {
            auto chokePosition = choke->center;
            auto enemyMain = Map::getEnemyMain();
            if (enemyMain)
            {
                auto scaled = scaledPosition(chokePosition, enemyMain->getPosition() - chokePosition, 160);
                if (scaled.isValid()) chokePosition = scaled;
            }

            // Get the detector closest to the position
            MyObserver best = nullptr;
            int bestDist = INT_MAX;
            for (const auto &detector : pendingDetectors)
            {
                int dist = detector->getDistance(chokePosition);
                if (dist < bestDist)
                {
                    best = detector;
                    bestDist = dist;
                }
            }
            if (best)
            {
                best->setActivity(ObserverActivity::ScoutingEnemyArmy);
                moveTowards(best, chokePosition, targetPosition);
                pendingDetectors.erase(best);
            }
        }

        auto threatDirection = targetPosition;
        if (Map::getEnemyStartingMain() && Map::getEnemyStartingNatural())
        {
            threatDirection = (Map::getEnemyStartingMain()->getPosition() + Map::getEnemyStartingNatural()->getPosition()) / 2;
        }

        for (auto detector : pendingDetectors)
        {
            detector->setActivity(ObserverActivity::ScoutingEnemyArmy);
            moveTowards(detector, targetPosition, threatDirection);
        }
    }
    else
    {
        int tilesAhead = 10;
        for (auto detector : pendingDetectors)
        {
            auto waypoint = PathFinding::NextGridOrChokeWaypoint(
                currentVanguardCluster->vanguard->lastPosition,targetPosition, nullptr, tilesAhead, false);
            if (waypoint.isValid())
            {
                detector->setActivity(ObserverActivity::ScoutingEnemyArmy);
                moveTowards(detector, waypoint, targetPosition);
            }
            else
            {
                detector->setActivity(ObserverActivity::EscortingArmy);
                moveToArmyVanguard(detector);
            }

            tilesAhead += 10;
        }
    }
}
