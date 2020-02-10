#include "UnitCluster.h"

#include "Players.h"
#include "UnitUtil.h"
#include "Map.h"

namespace
{
    auto &bwemMap = BWEM::Map::Instance();
}

/*
 * Almost everything in this file is currently just ported from the old Steamhammer-based Locutus.
 */

#ifndef _DEBUG
namespace
{
#endif

    int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit target)
    {
        BWAPI::UnitType targetType = target->getType();

        // Exceptions for dark templar.
        if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
        {
            if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
            {
                return 10;
            }
            if ((targetType == BWAPI::UnitTypes::Terran_Missile_Turret || targetType == BWAPI::UnitTypes::Terran_Comsat_Station) &&
                (BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0))
            {
                return 9;
            }
            if (targetType == BWAPI::UnitTypes::Zerg_Spore_Colony)
            {
                return 8;
            }
            if (targetType.isWorker())
            {
                return 8;
            }
        }

        // Short circuit: Enemy unit which is far enough outside its range is lower priority than a worker.
        int enemyRange = Players::weaponRange(target->getPlayer(), attacker->isFlying() ? targetType.airWeapon() : targetType.groundWeapon());
        if (enemyRange &&
            !targetType.isWorker() &&
            attacker->getDistance(target) > 32 + enemyRange)
        {
            return 8;
        }

        // Constructing a bunker makes a worker critical
        if (targetType.isWorker() && target->isConstructing() && target->getOrderTarget()
            && target->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Bunker)
            return 12;

        // Short circuit: Units before bunkers!
        if (targetType == BWAPI::UnitTypes::Terran_Bunker)
        {
            return 10;
        }
        // Medics and ordinary combat units. Include workers that are doing stuff.
        if (targetType == BWAPI::UnitTypes::Terran_Medic ||
            targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
            targetType == BWAPI::UnitTypes::Protoss_Reaver ||
            targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
        {
            return 12;
        }
        if (targetType.groundWeapon() != BWAPI::WeaponTypes::None && !targetType.isWorker())
        {
            return 11;
        }
        if (targetType.isWorker() && (target->isRepairing() || target->isConstructing() || Map::nearNarrowChokepoint(target->getPosition())))
        {
            return 11;
        }
        // next priority is bored workers and turrets
        if (targetType.isWorker() || targetType == BWAPI::UnitTypes::Terran_Missile_Turret)
        {
            return 9;
        }
        // Buildings come under attack during free time, so they can be split into more levels.
        // Nydus canal is critical.
        if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
        {
            return 10;
        }
        if (targetType == BWAPI::UnitTypes::Zerg_Spire)
        {
            return 6;
        }
        if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool ||
            targetType.isResourceDepot() ||
            targetType == BWAPI::UnitTypes::Protoss_Templar_Archives ||
            targetType.isSpellcaster())
        {
            return 5;
        }
        // Short circuit: Addons other than a completed comsat are worth almost nothing.
        // TODO should also check that it is attached
        if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->isCompleted()))
        {
            return 1;
        }
        // anything with a cost
        if (targetType.gasPrice() > 0 || targetType.mineralPrice() > 0)
        {
            return 3;
        }

        // then everything else
        return 1;
    }

#ifndef _DEBUG
}
#endif

std::shared_ptr<Unit> UnitCluster::ChooseMeleeTarget(BWAPI::Unit attacker, std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition)
{
    int bestScore = -999999;
    std::shared_ptr<Unit> bestTarget = nullptr;

    // Determine if the target is a base that is owned by the enemy
    auto targetBase = Map::baseNear(targetPosition);
    bool targetIsEnemyBase = targetBase && targetBase->owner == BWAPI::Broodwar->enemy();

    int distanceToTarget = attacker->getDistance(targetPosition);

    for (const auto &targetUnit : targets)
    {
        auto target = targetUnit->unit;

        if (target->getType() == BWAPI::UnitTypes::Zerg_Larva ||
            target->getType() == BWAPI::UnitTypes::Zerg_Egg ||
            target->isUnderDisruptionWeb() ||
            !UnitUtil::CanAttack(attacker, target))
        {
            continue;
        }

        // If we are targeting an enemy base, ignore outlying buildings (except static defense)
        if (targetIsEnemyBase && distanceToTarget > 200 && target->getType().isBuilding() && !UnitUtil::CanAttackGround(target))
        {
            continue;
        }

        // Skip targets that are too far away to worry about.
        const int range = attacker->getDistance(target);
//        if (range >= 13 * 32)
//        {
//            continue;
//        }

        // Let's say that 1 priority step is worth 64 pixels (2 tiles).
        // We care about unit-target range and target-order position distance.
        const int priority = getAttackPriority(attacker, target);        // 0..12
        int score = 2 * 32 * priority - range;

        const int closerToGoal =                                        // positive if target is closer than us to the goal
                distanceToTarget - target->getDistance(targetPosition);
        bool inWeaponRange = attacker->isInWeaponRange(target);

        /*
         * Disabled as we do not currently have these skills
         *
        // Kamikaze and rush attacks ignore all tier 2+ combat units unless they are closer to the order position and in weapon range
        if ((StrategyManager::Instance().isRushing() || order.getType() == SquadOrderTypes::KamikazeAttack) &&
            UnitUtil::IsCombatUnit(target) &&
            !UnitUtil::IsTierOneCombatUnit(target->getType()) &&
            !target->getType().isWorker() &&
            (range > UnitUtil::GetAttackRange(attacker, target) || closerToGoal < 0))
        {
            continue;
        }

        // Consider whether to attack enemies that are outside of our weapon range when on the attack
        if (!inWeaponRange && order.getType() != SquadOrderTypes::Defend)
        {
            // Never chase units that can kite us easily
            if (target->getType() == BWAPI::UnitTypes::Protoss_Dragoon ||
                target->getType() == BWAPI::UnitTypes::Terran_Vulture) continue;

            // Check if the target is moving away from us
            BWAPI::Position targetPositionInFiveFrames = InformationManager::Instance().predictUnitPosition(target, 5);
            if (target->isMoving() &&
                range <= MathUtil::EdgeToEdgeDistance(attacker->getType(), myPositionInFiveFrames, target->getType(), targetPositionInFiveFrames))
            {
                // Never chase workers
                if (target->getType().isWorker()) continue;

                // When rushing, don't chase anything when outside the order position area
                if (StrategyManager::Instance().isRushing() && !inOrderPositionArea) continue;
            }

            // Skip targets behind a wall
            if (InformationManager::Instance().isBehindEnemyWall(attacker, target)) continue;
        }

        // When rushing, prioritize workers that are building something
        if (StrategyManager::Instance().isRushing() && target->getType().isWorker() && target->isConstructing())
        {
            score += 4 * 32;
        }
         */

        // Adjust for special features.

        // Prefer targets under dark swarm, on the expectation that then we'll be under it too.
        if (target->isUnderDarkSwarm())
        {
            if (attacker->getType().isWorker())
            {
                // Workers can't hit under dark swarm. Skip this target.
                continue;
            }
            score += 4 * 32;
        }

        // A bonus for attacking enemies that are "in front".
        // It helps reduce distractions from moving toward the goal, the order position.
        if (closerToGoal > 0)
        {
            score += 2 * 32;
        }

        // This could adjust for relative speed and direction, so that we don't chase what we can't catch.
        if (inWeaponRange)
        {
            score += 4 * 32;
        }
        else if (!target->isMoving())
        {
            if (target->isSieged() ||
                target->getOrder() == BWAPI::Orders::Sieging ||
                target->getOrder() == BWAPI::Orders::Unsieging)
            {
                score += 48;
            }
            else
            {
                score += 32;
            }
        }
        else if (target->isBraking())
        {
            score += 16;
        }
        else if (target->getType().topSpeed() >= attacker->getType().topSpeed())
        {
            score -= 2 * 32;
        }

        if (target->isUnderStorm())
        {
            score -= 4 * 32;
        }

        // Prefer targets that are already hurt.
        if (target->getType().getRace() == BWAPI::Races::Protoss && target->getShields() <= 5)
        {
            score += 32;
            if (target->getHitPoints() < (target->getType().maxHitPoints() / 3))
            {
                score += 24;
            }
        }
        else if (target->getHitPoints() < target->getType().maxHitPoints())
        {
            score += 24;
            if (target->getHitPoints() < (target->getType().maxHitPoints() / 3))
            {
                score += 24;
            }
        }

        // Avoid defensive matrix
        if (target->isDefenseMatrixed())
        {
            score -= 4 * 32;
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}
