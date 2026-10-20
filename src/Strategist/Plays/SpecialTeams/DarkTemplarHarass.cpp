#include "DarkTemplarHarass.h"

#include "Map.h"
#include "Players.h"
#include "PathFinding.h"
#include "Units.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    bool hasPathWithoutDetection(BWAPI::Position start, BWAPI::Position end)
    {
        auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
        auto chokePath = Pathfinder::GetChokePointPath(start,
                                                        end,
                                                        BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
        return std::all_of(chokePath.begin(), chokePath.end(), [&grid](const auto &bwemChoke)
        {
            auto choke = Map::choke(bwemChoke);
            if (choke->width > 300) return true;
            if (grid.detection(choke->midpoint) > 0) return false;
            return true;
        });
    }

    Base *getBaseToHarass(OwnedUnit &gameUnit)
    {
        std::vector<std::pair<Base *, bool>> harassableBases;
        for (auto baseLoc : Map::getEnemyBases())
        {
            if (baseLoc->island) continue;
            if (!baseLoc->resourceDepot || !baseLoc->resourceDepot->completed) continue;

            bool hasCannon = false;
            for (const auto &cannon : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
            {
                if (!cannon->completed && cannon->estimatedCompletionFrame > (frameNow + 120))
                {
                    continue;
                }
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(cannon->prevPos)) == baseLoc->getArea())
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(gameUnit->ident) << "Discounting base @ " << BWAPI::WalkPosition(baseLoc->getPosition()) << " because of " << *cannon;
#endif
                    hasCannon = true;
                    break;
                }
            }
            if (hasCannon) continue;

            bool hasObserver = false;
            for (const auto &observer : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Observer))
            {
                /* Don't go into the enemy main if they have an observer - we will get trapped */
                if (baseLoc == Map::getEnemyStartingMain())
                {
                    hasObserver = true;
                    break;
                }

                /* Don't count observers we haven't seen in a while */
                if (!observer->lastPositionValid || observer->lastSeen < (frameNow - 120)) continue;

                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(observer->prevPos)) == baseLoc->getArea() ||
                    observer->getDistance(baseLoc->getPosition()) < 320)
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(gameUnit->ident) << "Discounting base @ " << BWAPI::WalkPosition(baseLoc->getPosition()) << " because of " << *observer;
#endif
                    hasObserver = true;
                    break;
                }
            }
            if (hasObserver) continue;

            if (!hasPathWithoutDetection(gameUnit->prevPos, baseLoc->getPosition()))
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(gameUnit->ident) << "Discounting base @ " << BWAPI::WalkPosition(baseLoc->getPosition()) << " because path has detection";
#endif
                continue;
            }

            int workerCount = 0;
            for (const auto &wrk : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (!wrk->lastPositionValid) continue;

                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(wrk->prevPos)) == baseLoc->getArea())
                {
                    workerCount++;
                }
            }

            harassableBases.emplace_back(baseLoc, workerCount > 2);
        }

        int bestDist = INT_MAX;
        Base *bestBase = nullptr;
        for (auto baseLoc : harassableBases)
        {
            int span = gameUnit->getDistance(baseLoc.first->getPosition());
            if (!baseLoc.second) span *= 2; // Penalize bases without workers
            if (span < bestDist)
            {
                bestDist = span;
                bestBase = baseLoc.first;
            }
        }

        return bestBase;
    }

    Unit getTarget(const OwnedUnit &myUnit,
                   const std::unordered_set<Unit> &enemyUnits,
                   bool allowRetreating = true,
                   int distThreshold = INT_MAX,
                   const std::function<bool(const Unit &)> &predicate = nullptr)
    {
        auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());

        Unit bestTarget = nullptr;
        int bestTargetDist = INT_MAX;
        int bestTargetHealth = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            if (!enemy->lastPositionValid) continue;
            if (enemy->undetected) continue;
            if (grid.detection(enemy->prevPos) > 0 && grid.groundThreat(enemy->prevPos) > 0) continue;
            if (!myUnit->canAttack(enemy)) continue;

            int span = myUnit->getDistance(enemy);
            if (span > distThreshold) continue;

            if (predicate && !predicate(enemy)) continue;

            if (span < bestTargetDist || (span == bestTargetDist && (enemy->lastHealth + enemy->lastShields) < bestTargetHealth))
            {
                if (!allowRetreating)
                {
                    auto predictedEnemyPosition = enemy->predictPosition(1);
                    if (myUnit->getDistance(enemy, predictedEnemyPosition) > span)
                    {
                        continue;
                    }
                }

                bestTarget = enemy;
                bestTargetDist = span;
                bestTargetHealth = enemy->lastHealth + enemy->lastShields;
            }
        }

        if (bestTargetDist > distThreshold) return nullptr;
        return bestTarget;
    }

    void moveToBase(OwnedUnit &gameUnit, Base *baseLoc)
    {
        /* TODO: Avoid detection / threats somehow */

        auto navigationGrid = Pathfinder::getNavigationGrid(baseLoc->getPosition());
        if (navigationGrid)
        {
            auto &node = (*navigationGrid)[gameUnit->getTilePosition()];
            auto nextNode = node.nextNode;
            auto secondNode = nextNode ? nextNode->nextNode : nullptr;
            auto blockingPredicate = [&](const Unit &enemy)
            {
                /* Consider it to be blocking if the unit is on one of the next two path nodes */
                auto tilePosition = enemy->getTilePosition();
                if ((tilePosition.x == node.x && tilePosition.y == node.y) ||
                    (nextNode && (tilePosition.x == nextNode->x && tilePosition.y == nextNode->y)) ||
                    (secondNode && (tilePosition.x == secondNode->x && tilePosition.y == secondNode->y)))
                {
                    return true;
                }

                /* Consider it blocking if we are in a narrow choke, the enemy is in our attack range, and the enemy is closer to the goal */
                return Map::isInNarrowChoke(gameUnit->getTilePosition())
                       && gameUnit->isInOurWeaponRange(enemy)
                       && (*navigationGrid)[enemy->getTilePosition()].cost <= node.cost;
            };
            auto target = getTarget(gameUnit, UnitMgr::allEnemy(), false, 100, blockingPredicate);
            if (target)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(gameUnit->ident) << "Attacking blocking unit " << *target;
#endif
                gameUnit->attackUnit(target);
                return;
            }
        }

        gameUnit->moveTo(baseLoc->getPosition());
    }

    void executeUnit(OwnedUnit &gameUnit)
    {
        bool detectionNearby = false;
        for (const auto &observer : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Observer))
        {
            /* Don't count observers we haven't seen in a while */
            if (!observer->lastPositionValid || observer->lastSeen < (frameNow - 120)) continue;

            if (gameUnit->getDistance(observer) < 480)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(gameUnit->ident) << "Nearby observer: " << *observer;
#endif
                detectionNearby = true;
                break;
            }
        }
        if (!detectionNearby)
        {
            for (const auto &cannon : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
            {
                if (!cannon->completed) continue;
                if (gameUnit->getDistance(cannon) < (8 * 32)) // actual range is 7 tiles, giving it an extra one for safety
                {
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(gameUnit->ident) << "Nearby observer: " << *cannon;
#endif
                    detectionNearby = true;
                    break;
                }
            }
        }

        auto canKillBeforeCompletedPredicate = [&](const Unit &target)
        {
            if (target->completed) return false;

            /* Attack a target if we think we can kill it before it completes */
            auto damagePerAttack = PlayerInfo::attackDamage(gameUnit->player, gameUnit->type, target->player, target->type);
            auto moveFrames = (int) ((double) gameUnit->getDistance(target) * 1.4 / gameUnit->type.topSpeed());
            auto nextAttack = std::max(gameUnit->cooldownUntil - frameNow, moveFrames);

            int attacks = (int) ((float) (target->lastHealth + target->lastShields) / (float) damagePerAttack);
            int framesToKill = nextAttack + attacks * gameUnit->type.groundWeapon().damageCooldown();

            return (frameNow + framesToKill) < target->estimatedCompletionFrame;
        };

        if (!detectionNearby)
        {
            auto cannonTarget = getTarget(gameUnit,
                                          UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon),
                                          false,
                                          300,
                                          canKillBeforeCompletedPredicate);
            if (cannonTarget)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(gameUnit->ident) << "Attacking cannon " << *cannonTarget;
#endif
                gameUnit->attackUnit(cannonTarget);
                return;
            }

            auto workerTarget = getTarget(gameUnit, UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Probe), false, 300);
            if (workerTarget)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(gameUnit->ident) << "Attacking worker " << *workerTarget;
#endif
                gameUnit->attackUnit(workerTarget);
                return;
            }
        }

        auto baseLoc = getBaseToHarass(gameUnit);
        if (baseLoc)
        {
            if (gameUnit->getDistance(baseLoc->mineralLineCenter) > 320 ||
                Map::isInNarrowChoke(gameUnit->getTilePosition()) ||
                BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos)) != baseLoc->getArea())
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(gameUnit->ident) << "Moving to harass base @ " << BWAPI::WalkPosition(baseLoc->getPosition());
#endif
                moveToBase(gameUnit, baseLoc);
                return;
            }
        }

        /* Prioritize completed nexuses, observatories we can kill before they complete, robo facilities, completed things, everything else */
        if (!detectionNearby)
        {
            auto completedPredicate = [](const Unit &target)
            {
                return target->completed;
            };
            auto otherTarget = getTarget(gameUnit, UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Nexus), false, 500, completedPredicate);
            if (!otherTarget) otherTarget = getTarget(gameUnit, UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Observatory), false, 500, canKillBeforeCompletedPredicate);
            if (!otherTarget) otherTarget = getTarget(gameUnit, UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Robotics_Facility), false, 500);
            if (!otherTarget) otherTarget = getTarget(gameUnit, UnitMgr::allEnemy(), false, INT_MAX, completedPredicate);
            if (!otherTarget) otherTarget = getTarget(gameUnit, UnitMgr::allEnemy(), false);
            if (otherTarget)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(gameUnit->ident) << "Attacking target " << *otherTarget;
#endif
                gameUnit->attackUnit(otherTarget);
                return;
            }
        }

        auto nextExpansions = Map::getUntakenExpansions(BWAPI::Broodwar->enemy());
        if (!nextExpansions.empty())
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(gameUnit->ident) << "Moving to next probable enemy expansion @ " << BWAPI::WalkPosition((*nextExpansions.begin())->getPosition());
#endif
            moveToBase(gameUnit, *nextExpansions.begin());
            return;
        }
    }
}

void DarkTemplarHarass::update()
{
    /* Always request DTs so all created DTs get assigned to this play */
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Dark_Templar, Map::getMyMain()->getPosition());

    /* Micro each unit individually */
    for (auto gameUnit : units)
    {
        executeUnit(gameUnit);
    }
}
