#include "UnitCluster.h"

#include "Players.h"
#include "UnitUtil.h"
#include "Map.h"
#include "Units.h"

/*
 * Almost everything in this file is currently just ported from the old Steamhammer-based Locutus.
 */

namespace
{
    int getAttackPriority(const MyUnit &attacker, const Unit &target)
    {
        const BWAPI::UnitType rangedType = attacker->type;
        const BWAPI::UnitType targetType = target->type;

        if (rangedType == BWAPI::UnitTypes::Zerg_Scourge)
        {
            if (!targetType.isFlyer())
            {
                // Can't target it. Also, ignore lifted buildings.
                return 0;
            }
            if (targetType == BWAPI::UnitTypes::Zerg_Overlord ||
                targetType == BWAPI::UnitTypes::Zerg_Scourge ||
                targetType == BWAPI::UnitTypes::Protoss_Interceptor)
            {
                // Usually not worth scourge at all.
                return 0;
            }

            // Arbiters first.
            if (targetType == BWAPI::UnitTypes::Protoss_Arbiter)
            {
                return 10;
            }

            // Everything else is the same. Hit whatever's closest.
            return 9;
        }

        if (rangedType == BWAPI::UnitTypes::Zerg_Guardian && target->isFlying)
        {
            // Can't target it.
            return 0;
        }

        // A carrier should not target an enemy interceptor.
        if (rangedType == BWAPI::UnitTypes::Protoss_Carrier && targetType == BWAPI::UnitTypes::Protoss_Interceptor)
        {
            return 0;
        }

        // An addon other than a completed comsat is boring.
        // TODO should also check that it is attached
        if (targetType.isAddon() && !(targetType == BWAPI::UnitTypes::Terran_Comsat_Station && target->completed))
        {
            return 1;
        }

        // if the target is building something near our base something is fishy
        auto ourBasePosition = BWAPI::Position(Map::getMyMain()->getPosition());
        if (target->getDistance(ourBasePosition) < 1000)
        {
            if (target->type.isWorker() && (target->bwapiUnit->isConstructing() || target->bwapiUnit->isRepairing()))
            {
                return 12;
            }
            if (target->type.isBuilding())
            {
                // This includes proxy buildings, which deserve high priority.
                // But when bases are close together, it can include innocent buildings.
                // We also don't want to disrupt priorities in case of proxy buildings
                // supported by units; we may want to target the units first.
                if (target->canAttackGround() || target->canAttackAir())
                {
                    return 10;
                }
                return 8;
            }
        }

        if (rangedType.isFlyer())
        {
            // Exceptions if we're a flyer (other than scourge, which is handled above).
            if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
            {
                return 12;
            }
        }
        else
        {
            // Exceptions if we're a ground unit.
            if ((targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && !target->burrowed) ||
                targetType == BWAPI::UnitTypes::Zerg_Infested_Terran)
            {
                return 12;
            }
        }

        // Observers are very high priority if we have dark templar
        if (targetType == BWAPI::UnitTypes::Protoss_Observer &&
            Units::countCompleted(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0)
        {
            return 12;
        }

        // Wraiths, scouts, and goliaths strongly prefer air targets because they do more damage to air units.
        if (attacker->type == BWAPI::UnitTypes::Terran_Wraith ||
            attacker->type == BWAPI::UnitTypes::Protoss_Scout)
        {
            if (target->type.isFlyer())    // air units, not floating buildings
            {
                return 11;
            }
        }
        else if (attacker->type == BWAPI::UnitTypes::Terran_Goliath)
        {
            if (target->type.isFlyer())    // air units, not floating buildings
            {
                return 10;
            }
        }

        if (targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
            targetType == BWAPI::UnitTypes::Protoss_Reaver)
        {
            return 12;
        }

        if (targetType == BWAPI::UnitTypes::Protoss_Arbiter)
        {
            return 11;
        }

        if (targetType == BWAPI::UnitTypes::Terran_Bunker)
        {
            return 9;
        }

        // Threats can attack us. Exceptions: Workers are not threats.
        if (target->canAttack(attacker) && !targetType.isWorker())
        {
            // Enemy unit which is far enough outside its range is lower priority than a worker.
            int enemyRange = Players::weaponRange(target->player, target->getWeapon(attacker));
            if (attacker->getDistance(target) > 48 + enemyRange)
            {
                return 8;
            }
            return 10;
        }
        // Droppers are as bad as threats. They may be loaded and are often isolated and safer to attack.
        if (targetType == BWAPI::UnitTypes::Terran_Dropship ||
            targetType == BWAPI::UnitTypes::Protoss_Shuttle)
        {
            return 10;
        }
        // Also as bad are other dangerous things.
        if (targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
            targetType == BWAPI::UnitTypes::Zerg_Scourge ||
            targetType == BWAPI::UnitTypes::Protoss_Observer)
        {
            return 10;
        }
        // Next are workers.
        if (targetType.isWorker())
        {
            if (attacker->type == BWAPI::UnitTypes::Terran_Vulture)
            {
                return 11;
            }
            // Blocking a narrow choke makes you critical.
            if (Map::isInNarrowChoke(target->getTilePosition()))
            {
                return 11;
            }
            // Repairing
            if (target->bwapiUnit->isRepairing() && target->bwapiUnit->getOrderTarget())
            {
                // Something that can shoot
                if (target->bwapiUnit->getOrderTarget()->getType().groundWeapon() != BWAPI::WeaponTypes::None)
                {
                    return 11;
                }

                // A bunker: only target the workers if we can't outrange the bunker
                if (target->bwapiUnit->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Bunker &&
                    Players::weaponRange(target->player, BWAPI::UnitTypes::Terran_Marine.groundWeapon()) > 128)
                {
                    return 10;
                }
            }
            // SCVs so close to the unit that they are likely to be attacking it are important
            if (attacker->getDistance(target) < 32)
            {
                return 10;
            }
            // SCVs constructing are also important.
            if (target->bwapiUnit->isConstructing())
            {
                return 9;
            }

            return 8;
        }
        // Sieged tanks are slightly more important than unsieged tanks
        if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
        {
            return 9;
        }
        // Important combat units that we may not have targeted above (esp. if we're a flyer).
        if (targetType == BWAPI::UnitTypes::Protoss_Carrier ||
            targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
        {
            return 8;
        }
        // Nydus canal is the most important building to kill.
        if (targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
        {
            return 10;
        }
        // Spellcasters are as important as key buildings.
        // Also remember to target other non-threat combat units.
        if (targetType.isSpellcaster() ||
            targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
            targetType.airWeapon() != BWAPI::WeaponTypes::None)
        {
            return 7;
        }
        // Templar tech and spawning pool are more important.
        if (targetType == BWAPI::UnitTypes::Protoss_Templar_Archives)
        {
            return 7;
        }
        if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
        {
            return 7;
        }
        // Don't forget the nexus/cc/hatchery.
        if (targetType.isResourceDepot())
        {
            return 6;
        }
        if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
        {
            return 5;
        }
        if (targetType == BWAPI::UnitTypes::Terran_Factory || targetType == BWAPI::UnitTypes::Terran_Armory)
        {
            return 5;
        }
        // Downgrade unfinished/unpowered buildings, with exceptions.
        if (targetType.isBuilding() &&
            (!target->completed || !target->bwapiUnit->isPowered()) &&
            !(targetType.isResourceDepot() ||
              targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
              targetType.airWeapon() != BWAPI::WeaponTypes::None ||
              targetType == BWAPI::UnitTypes::Terran_Bunker))
        {
            return 2;
        }
        if (targetType.gasPrice() > 0)
        {
            return 4;
        }
        if (targetType.mineralPrice() > 0)
        {
            return 3;
        }
        // Finally everything else.
        return 1;
    }
}

Unit UnitCluster::ChooseRangedTarget(const MyUnit &attacker, std::set<Unit> &targets, BWAPI::Position targetPosition)
{
    int bestScore = -999999;
    Unit bestTarget = nullptr;

    // Determine if the target is a base that is not owned by the enemy
    auto targetBase = Map::baseNear(targetPosition);
    bool targetIsEnemyBase = targetBase && targetBase->owner == BWAPI::Broodwar->enemy();

    int distanceToTarget = attacker->getDistance(targetPosition);

    for (const auto &targetUnit : targets)
    {
        auto target = targetUnit->bwapiUnit;

        if (target->getType() == BWAPI::UnitTypes::Zerg_Larva ||
            target->getType() == BWAPI::UnitTypes::Zerg_Egg ||
            !attacker->canAttack(targetUnit))
        {
            continue;
        }


        // If we are targeting an enemy base, ignore outlying buildings (except static defense)
        if (targetIsEnemyBase && distanceToTarget > 200 && target->getType().isBuilding() && !targetUnit->canAttackGround())
        {
            continue;
        }

        // Skip targets under dark swarm that we can't hit.
        if (target->isUnderDarkSwarm() && attacker->type != BWAPI::UnitTypes::Protoss_Reaver)
        {
            continue;
        }

        // Skip targets that are too far away to worry about.
        const int range = attacker->getDistance(targetUnit);
        if (range >= 13 * 32)
        {
            continue;
        }

        // Let's say that 1 priority step is worth 160 pixels (5 tiles).
        // We care about unit-target range and target-order position distance.
        const int priority = getAttackPriority(attacker, targetUnit);        // 0..12
        int score = 5 * 32 * priority - range;

        const int closerToGoal =                                        // positive if target is closer than us to the goal
                distanceToTarget - target->getDistance(targetPosition);

        // Adjust for special features.
        // A bonus for attacking enemies that are "in front".
        // It helps reduce distractions from moving toward the goal, the order position.
        if (closerToGoal > 0)
        {
            score += 2 * 32;
        }

        const bool isThreat = targetUnit->canAttack(attacker);   // may include workers as threats
        const bool canShootBack = isThreat && attacker->isInEnemyWeaponRange(targetUnit);

        if (isThreat)
        {
            if (canShootBack)
            {
                score += 6 * 32;
            }
            else if (attacker->isInOurWeaponRange(targetUnit))
            {
                score += 4 * 32;
            }
            else
            {
                score += 3 * 32;
            }
        }

            // This could adjust for relative speed and direction, so that we don't chase what we can't catch.
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
                score += 24;
            }
        }
        else if (target->isBraking())
        {
            score += 16;
        }
        else if (target->getType().topSpeed() >= attacker->type.topSpeed())
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

        // Prefer to hit air units that have acid spores on them from devourers.
        if (target->getAcidSporeCount() > 0)
        {
            // Especially if we're a mutalisk with a bounce attack.
            if (attacker->type == BWAPI::UnitTypes::Zerg_Mutalisk)
            {
                score += 16 * target->getAcidSporeCount();
            }
            else
            {
                score += 8 * target->getAcidSporeCount();
            }
        }

        // Take the damage type into account.
        BWAPI::DamageType damage = attacker->getWeapon(targetUnit).damageType();
        if (damage == BWAPI::DamageTypes::Explosive)
        {
            if (target->getType().size() == BWAPI::UnitSizeTypes::Large)
            {
                score += 32;
            }
        }
        else if (damage == BWAPI::DamageTypes::Concussive)
        {
            if (target->getType().size() == BWAPI::UnitSizeTypes::Small)
            {
                score += 32;
            }
            else if (target->getType().size() == BWAPI::UnitSizeTypes::Large)
            {
                score -= 32;
            }
        }

        // For wall buildings, prefer the ones with lower health
        /*
        if (InformationManager::Instance().isEnemyWallBuilding(target) &&
            target->getType() == BWAPI::UnitTypes::Terran_Supply_Depot)
        {
            score += 128;
        }
         */

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = targetUnit;
        }
    }

    return bestTarget;
}
