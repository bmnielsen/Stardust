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
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        auto chokePath = PathFinding::GetChokePointPath(start,
                                                        end,
                                                        BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
        for (auto bwemChoke : chokePath)
        {
            auto choke = Map::choke(bwemChoke);
            if (choke->width > 300) continue;
            if (grid.detection(choke->center) > 0) return false;
        }

        return true;
    }

    Base *getBaseToHarass(MyUnit &unit)
    {
        std::vector<std::pair<Base *, bool>> harassableBases;
        for (auto base : Map::getEnemyBases())
        {
            if (base->island) continue;
            if (!base->resourceDepot || !base->resourceDepot->completed) continue;

            bool hasCannon = false;
            for (const auto &cannon : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
            {
                if (!cannon->completed && cannon->estimatedCompletionFrame < (currentFrame + 120))
                {
                    continue;
                }
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(cannon->lastPosition)) == base->getArea())
                {
                    hasCannon = true;
                    break;
                }
            }
            if (hasCannon) continue;
            if (!hasPathWithoutDetection(unit->lastPosition, base->getPosition())) continue;

            int workerCount = 0;
            for (const auto &worker : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (!worker->lastPositionValid) continue;

                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(worker->lastPosition)) == base->getArea())
                {
                    workerCount++;
                }
            }

            harassableBases.push_back(std::make_pair(base, workerCount > 2));
        }

        int bestDist = INT_MAX;
        Base *bestBase = nullptr;
        bool bestBaseHasWorkers = false;
        for (auto base : harassableBases)
        {
            if (bestBaseHasWorkers && !base.second) continue;

            int dist = unit->getDistance(base.first->getPosition());
            if (dist < bestDist || (!bestBaseHasWorkers && base.second))
            {
                bestDist = dist;
                bestBase = base.first;
                bestBaseHasWorkers = base.second;
            }
        }

        return bestBase;
    }

    Unit getTarget(const MyUnit &myUnit,
                   const std::unordered_set<Unit> &enemyUnits,
                   bool allowRetreating = true,
                   int distThreshold = INT_MAX,
                   const std::function<bool(const Unit &)> &predicate = nullptr)
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());

        Unit bestTarget = nullptr;
        int bestTargetDist = INT_MAX;
        int bestTargetHealth = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            if (!enemy->lastPositionValid) continue;
            if (enemy->undetected) continue;
            if (grid.detection(enemy->lastPosition) > 0 && grid.groundThreat(enemy->lastPosition) > 0) continue;
            if (!myUnit->canAttack(enemy)) continue;

            int dist = myUnit->getDistance(enemy);
            if (dist > distThreshold) continue;

            if (predicate && !predicate(enemy)) continue;

            if (dist < bestTargetDist || (dist == bestTargetDist && (enemy->lastHealth + enemy->lastShields) < bestTargetHealth))
            {
                if (!allowRetreating)
                {
                    auto predictedEnemyPosition = enemy->predictPosition(1);
                    if (predictedEnemyPosition.isValid() && myUnit->getDistance(enemy, predictedEnemyPosition) > dist)
                    {
                        continue;
                    }
                }

                bestTarget = enemy;
                bestTargetDist = dist;
                bestTargetHealth = enemy->lastHealth + enemy->lastShields;
            }
        }

        if (bestTargetDist > distThreshold) return nullptr;
        return bestTarget;
    }

    void moveToBase(MyUnit &unit, Base *base)
    {
        auto grid = PathFinding::getNavigationGrid(base->getTilePosition());
        if (grid)
        {
            auto node = (*grid)[unit->getTilePosition()];
            auto nextNode = node.nextNode;
            auto secondNode = nextNode ? nextNode->nextNode : nullptr;
            auto blockingPredicate = [&](const Unit &enemy)
            {
                // Consider it to be blocking if the unit is on one of the next two path nodes
                auto tilePosition = enemy->getTilePosition();
                if ((tilePosition.x == node.x && tilePosition.y == node.y) ||
                    (nextNode && (tilePosition.x == nextNode->x && tilePosition.y == nextNode->y)) ||
                    (secondNode && (tilePosition.x == secondNode->x && tilePosition.y == secondNode->y)))
                {
                    return true;
                }

                // Consider it blocking if we are in a narrow choke, the enemy is in our attack range, and the enemy is closer to the goal
                return Map::isInNarrowChoke(unit->getTilePosition())
                       && unit->isInOurWeaponRange(enemy)
                       && (*grid)[enemy->getTilePosition()].cost <= node.cost;
            };
            auto target = getTarget(unit, Units::allEnemy(), false, 100, blockingPredicate);
            if (target)
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(unit->id) << "Attacking blocking unit " << *target;
#endif
                unit->attackUnit(target);
                return;
            }
        }

        unit->moveTo(base->getPosition());
    }

    void executeUnit(MyUnit &unit)
    {
        auto canKillBeforeCompletedPredicate = [&](const Unit &target)
        {
            if (target->completed) return false;

            // Attack a target if we think we can kill it before it completes
            auto damagePerAttack = Players::attackDamage(unit->player, unit->type, target->player, target->type);
            auto moveFrames = (int) ((double) unit->getDistance(target) * 1.4 / unit->type.topSpeed());
            auto nextAttack = std::max(unit->cooldownUntil - currentFrame, moveFrames);

            int attacks = (int) ((float) (target->lastHealth + target->lastShields) / (float) damagePerAttack);
            int framesToKill = nextAttack + attacks * unit->type.groundWeapon().damageCooldown();

            return (currentFrame + framesToKill) < target->estimatedCompletionFrame;
        };
        auto cannonTarget = getTarget(unit,
                                      Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon),
                                      false,
                                      300,
                                      canKillBeforeCompletedPredicate);
        if (cannonTarget)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Attacking cannon " << *cannonTarget;
#endif
            unit->attackUnit(cannonTarget);
            return;
        }

        auto workerTarget = getTarget(unit, Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Probe), false, 300);
        if (workerTarget)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Attacking worker " << *workerTarget;
#endif
            unit->attackUnit(workerTarget);
            return;
        }

        auto base = getBaseToHarass(unit);
        if (base)
        {
            if (unit->getDistance(base->mineralLineCenter) > 320 ||
                Map::isInNarrowChoke(unit->getTilePosition()) ||
                BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != base->getArea())
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(unit->id) << "Moving to harass base @ " << BWAPI::WalkPosition(base->getPosition());
#endif
                moveToBase(unit, base);
                return;
            }
        }

        // Prioritize completed nexuses, observatories we can kill before they complete, robo facilities, completed things, everything else
        auto completedPredicate = [](const Unit &target)
        {
            return target->completed;
        };
        auto otherTarget = getTarget(unit, Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Nexus), false, 500, completedPredicate);
        if (!otherTarget) otherTarget = getTarget(unit, Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Observatory), false, 500, canKillBeforeCompletedPredicate);
        if (!otherTarget) otherTarget = getTarget(unit, Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Robotics_Facility), false, 500);
        if (!otherTarget) otherTarget = getTarget(unit, Units::allEnemy(), false, INT_MAX, completedPredicate);
        if (!otherTarget) otherTarget = getTarget(unit, Units::allEnemy(), false);
        if (otherTarget)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Attacking target " << *otherTarget;
#endif
            unit->attackUnit(otherTarget);
            return;
        }

        auto nextExpansions = Map::getUntakenExpansions(BWAPI::Broodwar->enemy());
        if (!nextExpansions.empty())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Moving to next probable enemy expansion @ " << BWAPI::WalkPosition((*nextExpansions.begin())->getPosition());
#endif
            moveToBase(unit, *nextExpansions.begin());
            return;
        }
    }
}

void DarkTemplarHarass::update()
{
    // Always request DTs so all created DTs get assigned to this play
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Dark_Templar, Map::getMyMain()->getPosition());

    // Micro each unit individually
    for (auto unit : units)
    {
        executeUnit(unit);
    }
}
