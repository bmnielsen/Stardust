#include "UnitCluster.h"

#include "PathFinding.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"
#include "Players.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_TARGETING false  // Writes verbose log info to debug for each unit targeting
#endif

namespace
{
    int targetPriority(Unit target)
    {
        // This is similar to the target prioritization in Locutus that originally came from Steamhammer/UAlbertaBot

        const BWAPI::UnitType targetType = target->type;
        auto closeToOurBase = [&target]()
        {
            auto ourBasePosition = BWAPI::Position(Map::getMyMain()->getPosition());
            return target->getDistance(ourBasePosition) < 1000;
        };

        if (targetType == BWAPI::UnitTypes::Zerg_Infested_Terran ||
            targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
            targetType == BWAPI::UnitTypes::Protoss_Reaver ||
            (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && !target->burrowed) ||
            (targetType == BWAPI::UnitTypes::Protoss_Observer && Units::countCompleted(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0))
        {
            return 15;
        }

        if (targetType == BWAPI::UnitTypes::Protoss_Arbiter ||
            targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
        {
            return 14;
        }

        if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
            targetType == BWAPI::UnitTypes::Terran_Dropship ||
            targetType == BWAPI::UnitTypes::Protoss_Shuttle ||
            targetType == BWAPI::UnitTypes::Terran_Science_Vessel ||
            targetType == BWAPI::UnitTypes::Zerg_Scourge ||
            targetType == BWAPI::UnitTypes::Protoss_Observer ||
            targetType == BWAPI::UnitTypes::Zerg_Nydus_Canal)
        {
            return 13;
        }

        // Proxies
        if (target->type.isBuilding() && closeToOurBase())
        {
            if (target->canAttackGround() || target->canAttackAir())
            {
                return 12;
            }
            return 10;
        }

        if (targetType == BWAPI::UnitTypes::Terran_Bunker)
        {
            return 11;
        }

        if (targetType.isWorker())
        {
            if ((target->bwapiUnit->isConstructing() || target->bwapiUnit->isRepairing()) && closeToOurBase())
            {
                return 15;
            }

            // Blocking a narrow choke makes you critical.
            if (Map::isInNarrowChoke(target->getTilePosition()))
            {
                return 14;
            }

            // Repairing
            if (target->bwapiUnit->isRepairing() && target->bwapiUnit->getOrderTarget())
            {
                // Something that can shoot
                if (target->bwapiUnit->getOrderTarget()->getType().groundWeapon() != BWAPI::WeaponTypes::None)
                {
                    return 14;
                }

                // A bunker: only target the workers if we can't outrange the bunker
                if (target->bwapiUnit->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Bunker &&
                    Players::weaponRange(target->player, BWAPI::UnitTypes::Terran_Marine.groundWeapon()) > 128)
                {
                    return 13;
                }
            }

            // Workers that have attacked in the last four seconds
            if ((BWAPI::Broodwar->getFrameCount() - target->lastSeenAttacking) < 96)
            {
                return 11;
            }

            if (target->bwapiUnit->isConstructing())
            {
                return 10;
            }

            return 9;
        }

        if (target->canAttackGround() || target->canAttackAir())
        {
            return 11;
        }

        if (targetType.isSpellcaster())
        {
            return 10;
        }

        if (targetType.isResourceDepot())
        {
            return 6;
        }

        if (targetType == BWAPI::UnitTypes::Protoss_Pylon ||
            targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool ||
            targetType == BWAPI::UnitTypes::Terran_Factory ||
            targetType == BWAPI::UnitTypes::Terran_Armory)
        {
            return 5;
        }

        if (targetType.isAddon())
        {
            return 1;
        }

        if (!target->completed || (targetType.requiresPsi() && !target->bwapiUnit->isPowered()))
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

        return 1;
    }

    struct Target
    {
        Unit unit;
        int priority;               // Base priority computed by targetPriority above
        int healthIncludingShields; // Estimated health reduced by incoming bullets and earlier attackers
        int attackerCount;          // How many attackers have this target in their closeTargets vector

        explicit Target(const Unit &unit)
                : unit(unit)
                , priority(targetPriority(unit))
                , healthIncludingShields(unit->health + unit->shields)
                , attackerCount(0) {}

        void dealDamage(const MyUnit &attacker)
        {
            if (unit->isBeingHealed()) return;

            healthIncludingShields -= Players::attackDamage(attacker->player, attacker->type, unit->player, unit->type);
        }
    };

    struct Attacker
    {
        MyUnit unit;
        std::vector<Target *> targets;      // All of the targets available to this attacker
        int framesToAttack;                 // The number of frames before this attacker can attack something
        std::vector<Target *> closeTargets; // All targets that can be attacked at framesToAttack

        explicit Attacker(MyUnit unit) : unit(std::move(unit)), framesToAttack(INT_MAX) {}
    };

    bool isTargetReachableEnemyBase(BWAPI::Position targetPosition)
    {
        // First check if the target is an enemy base
        auto targetBase = Map::baseNear(targetPosition);
        if (!targetBase) return false;
        if (targetBase->owner != BWAPI::Broodwar->enemy()) return false;
        if (targetBase->lastScouted != -1 && (!targetBase->resourceDepot || !targetBase->resourceDepot->exists())) return false;

        // Next check if we can path to it
        if (!Map::getMyMainChoke()) return true;

        auto grid = PathFinding::getNavigationGrid(targetBase->getTilePosition());
        if (!grid) return true;

        auto node = (*grid)[Map::getMyMainChoke()->center];
        return node.nextNode != nullptr;
    }
}

std::vector<std::pair<MyUnit, Unit>>
UnitCluster::selectTargets(std::set<Unit> &targetUnits, BWAPI::Position targetPosition, bool staticPosition)
{
    std::vector<std::pair<MyUnit, Unit>> result;

    // For our targeting we want to know if we are attacking a reachable enemy base
    // Criteria:
    // - Not in static position mode
    // - Target is near a base
    // - The base is owned by the enemy
    // - The base has a resource depot or hasn't been scouted yet
    // - The base has a navigation grid path from our main choke (i.e. hasn't been walled-off)
    bool targetIsReachableEnemyBase = !staticPosition && isTargetReachableEnemyBase(targetPosition);

    // Create the target objects
    std::vector<Target> targets;
    targets.reserve(targetUnits.size());
    for (const auto &targetUnit : targetUnits)
    {
        targets.emplace_back(targetUnit);
    }

#if DEBUG_TARGETING
    std::ostringstream dbg;
    dbg << "Targeting for cluster " << BWAPI::TilePosition(center) << " - targetIsReachableEnemyBase=" << targetIsReachableEnemyBase;
#endif

    // Perform a pre-scan to get valid targets and the frame at which we can attack them for each unit
    std::vector<Attacker> attackers;
    attackers.reserve(units.size());
    for (const auto &unit : units)
    {
#if DEBUG_TARGETING
        dbg << "\n" << *unit << " target filtering:";
#endif

        // If the unit isn't ready, lock it to its current target and skip targeting completely
        if (!unit->isReady())
        {
            Unit targetUnit = nullptr;

            if (unit->bwapiUnit->getLastCommand().type == BWAPI::UnitCommandTypes::Attack_Unit)
            {
                targetUnit = Units::get(unit->bwapiUnit->getLastCommand().getTarget());

                // If the unit isn't on its cooldown yet, simulate the attack on the target for use in later targeting
                // If the unit is on its cooldown, there will be an upcomingAttack already registered on the target
                // This ensures consistency between our targeting and combat sim (avoiding double-counting of damage)
                if (targetUnit && (unit->cooldownUntil - BWAPI::Broodwar->getFrameCount()) <= (BWAPI::Broodwar->getLatencyFrames() + 2))
                {
                    for (auto &target : targets)
                    {
                        if (target.unit == targetUnit)
                        {
                            target.dealDamage(unit);
                            break;
                        }
                    }
                }
            }

#if DEBUG_TARGETING
            dbg << "\n Not ready, locking to current target";
            if (targetUnit) dbg << " " << *targetUnit;
#endif

            result.emplace_back(std::make_pair(unit, targetUnit));
            continue;
        }

        attackers.emplace_back(unit);
        auto &attacker = *attackers.rbegin();
        attacker.targets.reserve(targets.size());
        attacker.closeTargets.reserve(targets.size());

        bool isRanged = UnitUtil::IsRangedUnit(unit->type);
        int distanceToTarget = unit->getDistance(targetPosition);
        for (auto &target : targets)
        {
            if (target.unit->type == BWAPI::UnitTypes::Zerg_Larva ||
                target.unit->type == BWAPI::UnitTypes::Zerg_Egg ||
                unit->undetected ||
                unit->health <= 0 ||
                !unit->canAttack(target.unit))
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " because of type / detection / health / can't attack";
#endif
                continue;
            }

            // If we are targeting an enemy base, ignore outlying buildings (except static defense)
            if (target.priority < 7 && targetIsReachableEnemyBase && distanceToTarget > 200)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " as priority < 7, targetIsReachableEnemyBase, distanceToTarget > 200";
#endif
                continue;
            }

            // Ranged cannot hit targets under dark swarm
            if ((isRanged || unit->type.isWorker())
                && unit->type != BWAPI::UnitTypes::Protoss_Reaver
                && target.unit->bwapiUnit->isUnderDarkSwarm())
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " as under dark swarm";
#endif
                continue;
            }

            // Melee cannot hit targets under disruption web and don't want to attack targets under storm
            if (!isRanged && (target.unit->bwapiUnit->isUnderDisruptionWeb() || target.unit->bwapiUnit->isUnderStorm()))
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " as under disruption web";
#endif
                continue;
            }

            const int range = unit->getDistance(target.unit);
            int distToRange = std::max(0, range - (target.unit->isFlying ? unit->airRange() : unit->groundRange()));

            // In static position mode, units only attack what they are in range of
            if (staticPosition && distToRange > 0)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " as we are in a static position and it is not in range";
#endif
                continue;
            }

            // Skip targets that are out of range and moving away from us, unless we are close to our target position
            if (distanceToTarget > 500 && distToRange > 0)
            {
                auto predictedTargetPosition = target.unit->predictPosition(1);
                if (predictedTargetPosition.isValid() && unit->getDistance(target.unit, predictedTargetPosition) > range)
                {
#if DEBUG_TARGETING
                    dbg << "\n Skipping " << *target.unit << " as it is out of range and moving away from us";
#endif
                    continue;
                }
            }

            // This is a suitable target
#if DEBUG_TARGETING
            dbg << "\n Added target " << *target.unit;
#endif
            attacker.targets.emplace_back(&target);

            int framesToAttack = std::max(unit->cooldownUntil - BWAPI::Broodwar->getFrameCount(),
                                          (int) ((double) distToRange / unit->type.topSpeed()) + BWAPI::Broodwar->getRemainingLatencyFrames() + 2);

            if (framesToAttack < attacker.framesToAttack)
            {
                attacker.framesToAttack = framesToAttack;
                attacker.closeTargets.clear();
                attacker.closeTargets.push_back(&target);
            }
            else if (framesToAttack == attacker.framesToAttack)
            {
                attacker.closeTargets.push_back(&target);
            }
        }

        // Increment the close target count for all of our close targets
        for (auto &closeTarget : attacker.closeTargets)
        {
            closeTarget->attackerCount++;
        }
    }

    // Sort the attackers
    std::sort(attackers.begin(), attackers.end(), [](const Attacker &a, const Attacker &b)
    {
        // First priority: lowest frames to attack
        if (a.framesToAttack < b.framesToAttack) return true;
        if (a.framesToAttack > b.framesToAttack) return false;

        // Second priority: lowest number of close targets
        if (a.closeTargets.size() < b.closeTargets.size()) return true;
        if (a.closeTargets.size() > b.closeTargets.size()) return false;

        // Third priority: lowest number of targets
        if (a.targets.size() < b.targets.size()) return true;
        if (a.targets.size() > b.targets.size()) return true;

        // Default: unit ID (guaranteed to be inequal)
        return a.unit->id < b.unit->id;
    });

    // Now assign each unit a target, skipping any that are simulated to already be dead
    for (auto &attacker : attackers)
    {
        auto &unit = attacker.unit;

#if DEBUG_TARGETING
        dbg << "\n" << *unit << " target scoring:";
#endif

        Target *bestTarget = nullptr;
        int bestScore = -999999;
        int bestAttackerCount = 0;
        int bestDist = INT_MAX;

        int cooldownMoveFrames = std::max(0,
                                          unit->cooldownUntil - BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getRemainingLatencyFrames() - 2);

        int distanceToTarget = unit->getDistance(targetPosition);
        for (auto &potentialTarget : attacker.targets)
        {
            if (potentialTarget->healthIncludingShields <= 0)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping target scoring " << *potentialTarget->unit << " as it is predicted to be dead";
#endif
                continue;
            }

            // Initialize the score as a formula of the target priority and how far outside our attack range it is
            // Each priority step is equivalent to 2 tiles
            // If the unit is on cooldown, we assume it can move towards the target before attacking
            const int targetDist = unit->getDistance(potentialTarget->unit) -
                                   (int) ((double) cooldownMoveFrames * unit->type.topSpeed());
            const int range = potentialTarget->unit->isFlying ? unit->airRange() : unit->groundRange();
            int score = 2 * 32 * potentialTarget->priority - std::max(0, targetDist - range);

            // Now adjust the score according to some rules

            // Give a bonus to injured targets
            // This is what provides some focus fire behaviour, as we simulate previous attacker's hits
            double healthPercentage = (double) potentialTarget->healthIncludingShields /
                                      (double) (potentialTarget->unit->type.maxHitPoints() + potentialTarget->unit->type.maxShields());
            score += (int) (160.0 * (1.0 - healthPercentage));

            // Avoid defensive matrix
            if (potentialTarget->unit->bwapiUnit->isDefenseMatrixed())
            {
                score -= 4 * 32;
            }

            // Give a bonus for enemies that are closer to our target position (usually the enemy base)
            if (potentialTarget->unit->getDistance(targetPosition) < distanceToTarget)
            {
                score += 2 * 32;
            }

            // Give bonus to units under dark swarm
            // Ranged units skip these targets earlier
            if (potentialTarget->unit->bwapiUnit->isUnderDarkSwarm())
            {
                score += 4 * 32;
            }

            // Adjust based on the threat level of the enemy unit to us
            if (potentialTarget->unit->canAttack(unit))
            {
                if (unit->isInEnemyWeaponRange(potentialTarget->unit))
                {
                    score += 6 * 32;
                }
                else if (unit->isInOurWeaponRange(potentialTarget->unit))
                {
                    score += 4 * 32;
                }
                else
                {
                    score += 3 * 32;
                }
            }

            // Give a bonus to non-moving or braking targets, and a penalty to units that are faster than us
            if (!potentialTarget->unit->bwapiUnit->isMoving())
            {
                if (potentialTarget->unit->bwapiUnit->isSieged() ||
                    potentialTarget->unit->bwapiUnit->getOrder() == BWAPI::Orders::Sieging ||
                    potentialTarget->unit->bwapiUnit->getOrder() == BWAPI::Orders::Unsieging)
                {
                    score += 48;
                }
                else
                {
                    score += 24;
                }
            }
            else if (potentialTarget->unit->bwapiUnit->isBraking())
            {
                score += 16;
            }
            else if (potentialTarget->unit->type.topSpeed() >= unit->type.topSpeed())
            {
                score -= 4 * 32;
            }

            // Take the damage type into account
            BWAPI::DamageType damage = unit->getWeapon(potentialTarget->unit).damageType();
            if (damage == BWAPI::DamageTypes::Explosive)
            {
                if (potentialTarget->unit->type.size() == BWAPI::UnitSizeTypes::Large)
                {
                    score += 32;
                }
            }
            else if (damage == BWAPI::DamageTypes::Concussive)
            {
                if (potentialTarget->unit->type.size() == BWAPI::UnitSizeTypes::Small)
                {
                    score += 32;
                }
                else if (potentialTarget->unit->type.size() == BWAPI::UnitSizeTypes::Large)
                {
                    score -= 32;
                }
            }

#if DEBUG_TARGETING
            dbg << "\n Target " << *potentialTarget->unit << " scored: score=" << score
                << ", attackerCount=" << potentialTarget->attackerCount
                << ", dist=" << targetDist;
#endif

            // See if this is the best target
            // Criteria:
            // - Score is higher
            // - Attackers is higher
            // - Distance is lower
            if (score > bestScore ||
                (score == bestScore && potentialTarget->attackerCount > bestAttackerCount) ||
                (score == bestScore && potentialTarget->attackerCount == bestAttackerCount && targetDist < bestDist))
            {
                bestScore = score;
                bestAttackerCount = potentialTarget->attackerCount;
                bestDist = targetDist;
                bestTarget = potentialTarget;

#if DEBUG_TARGETING
                dbg << " (best)";
#endif
            }
        }

        if (bestTarget)
        {
            bestTarget->dealDamage(unit);
            result.emplace_back(std::make_pair(attacker.unit, bestTarget->unit));

#if DEBUG_TARGETING
            dbg << "\n Selected target: " << *bestTarget->unit;
#endif
        }
        else
        {
            result.emplace_back(std::make_pair(attacker.unit, nullptr));

#if DEBUG_TARGETING
            dbg << "\n No target";
#endif
        }
    }

#if DEBUG_TARGETING
    Log::Debug() << dbg.str();
#endif

    return result;
}
