#include "UnitCluster.h"

#include "PathFinding.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"
#include "Players.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_TARGETING false  // Writes verbose log info to debug for each unit targeting
#define DRAW_TARGETING true  // Draws lines between our units and their targets
#endif

namespace
{
    int targetPriority(Unit target)
    {
        // This is similar to the target prioritization in Locutus that originally came from Steamhammer/UAlbertaBot

        const BWAPI::UnitType targetType = target->type;
        auto notCloseToTargetPosition = [&target]()
        {
            return target->distToTargetPosition > 1500;
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
        if (target->type.isBuilding() && !target->isFlying && notCloseToTargetPosition())
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
            if ((target->bwapiUnit->isConstructing() || target->bwapiUnit->isRepairing()) && notCloseToTargetPosition())
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
            if ((currentFrame - target->lastSeenAttacking) < 96)
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
            return 7;
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

    bool isInOurMainOrNatural(const Unit &unit)
    {
        if (!unit->lastPositionValid) return false;

        auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition));
        auto mainAndNaturalAreas = Map::getMyMainAreas();
        auto natural = Map::getMyNatural();
        if (natural)
        {
            mainAndNaturalAreas.insert(natural->getArea());
        }

        return mainAndNaturalAreas.contains(area);
    }

    struct Target
    {
        Unit unit;
        int priority;               // Base priority computed by targetPriority above
        int healthIncludingShields; // Estimated health reduced by incoming bullets and earlier attackers
        int attackerCount;          // How many attackers have this target in their closeTargets vector
        bool cliffedTank;
        bool inMyMainOrNatural;

        explicit Target(const Unit &unit, const MyUnit &vanguard)
                : unit(unit)
                , priority(targetPriority(unit))
                , healthIncludingShields(unit->health + unit->shields)
                , attackerCount(0)
                , cliffedTank(unit->isCliffedTank(vanguard))
                , inMyMainOrNatural(isInOurMainOrNatural(unit)) {}

        void dealDamage(const MyUnit &attacker)
        {
            if (unit->isBeingHealed()) return;

            int damage = Players::attackDamage(attacker->player, attacker->type, unit->player, unit->type);

            // For low-ground to high-ground attacks, simulate half damage
            if (!attacker->isFlying && UnitUtil::IsRangedUnit(attacker->type) &&
                BWAPI::Broodwar->getGroundHeight(attacker->getTilePosition()) < BWAPI::Broodwar->getGroundHeight(unit->getTilePosition()))
            {
                damage /= 2;
            }

            healthIncludingShields -= damage;
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

    bool isTargetReachableEnemyBase(BWAPI::Position targetPosition, const MyUnit &vanguard)
    {
        // First check if the target is an enemy base
        auto targetBase = Map::baseNear(targetPosition);
        if (!targetBase) return false;
        if (targetBase->owner != BWAPI::Broodwar->enemy()) return false;
        if (targetBase->lastScouted != -1 && !targetBase->resourceDepot) return false;

        if (!vanguard) return true;

        auto grid = PathFinding::getNavigationGrid(targetBase->getPosition());
        if (!grid) return true;

        auto &node = (*grid)[vanguard->lastPosition];
        return node.nextNode != nullptr;
    }
}

std::vector<std::pair<MyUnit, Unit>>
UnitCluster::selectTargets(std::set<Unit> &targetUnits, BWAPI::Position targetPosition, bool staticPosition)
{
    // Perform a scan to remove target units that are at much different distances from our target position than the vanguard unit
    // This indicates we have picked up enemy units that are in a different region separated from us by a cliff
    if (!vanguard->isFlying)
    {
        for (auto it = targetUnits.begin(); it != targetUnits.end();)
        {
            auto &unit = *it;
            if (!unit->isFlying)
            {
                unit->distToTargetPosition = PathFinding::GetGroundDistance(
                        unit->simPosition,
                        targetPosition,
                        unit->type,
                        PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);

                if (unit->distToTargetPosition != -1 && abs(unit->distToTargetPosition - vanguardDistToTarget) > 700
                    && !vanguard->isInEnemyWeaponRange(unit))
                {
                    it = targetUnits.erase(it);
                    continue;
                }
            }

            it++;
        }
    }

    std::vector<std::pair<MyUnit, Unit>> result;

    // For our targeting we want to know if we are attacking a reachable enemy base
    // Criteria:
    // - Not in static position mode
    // - Target is near a base
    // - The base is owned by the enemy
    // - The base has a resource depot or hasn't been scouted yet
    // - The base has a navigation grid path from our main choke (i.e. hasn't been walled-off)
    bool targetIsReachableEnemyBase = !staticPosition && isTargetReachableEnemyBase(targetPosition, vanguard);

    // Create the target objects
    // This also updates the enemy AOE radius
    std::vector<Target> targets;
    targets.reserve(targetUnits.size());
    enemyAoeRadius = 0;
    for (const auto &targetUnit : targetUnits)
    {
        if (targetUnit->exists())
        {
            targets.emplace_back(targetUnit, vanguard);

            enemyAoeRadius = std::max(enemyAoeRadius, targetUnit->groundWeapon().outerSplashRadius());
        }
    }

#if DEBUG_TARGETING
    std::ostringstream dbg;
    dbg << "Targeting for cluster " << BWAPI::TilePosition(center) << " - targetIsReachableEnemyBase=" << targetIsReachableEnemyBase;
#endif

    auto getCurrentTarget = [&targets](const MyUnit &unit) -> Target *
    {
        if (unit->bwapiUnit->getLastCommand().type != BWAPI::UnitCommandTypes::Attack_Unit) return nullptr;

        auto targetUnit = Units::get(unit->bwapiUnit->getLastCommand().getTarget());
        if (!targetUnit) return nullptr;

        for (auto &target : targets)
        {
            if (target.unit == targetUnit) return &target;
        }

        return nullptr;
    };

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
            auto target = getCurrentTarget(unit);

            // If the unit isn't on its cooldown yet, simulate the attack on the target for use in later targeting
            // If the unit is on its cooldown, there will be an upcomingAttack already registered on the target
            // This ensures consistency between our targeting and combat sim (avoiding double-counting of damage)
            if (target && (unit->cooldownUntil - currentFrame) <= (BWAPI::Broodwar->getLatencyFrames() + 2))
            {
                target->dealDamage(unit);
            }

#if DEBUG_TARGETING
            dbg << "\n Not ready, locking to current target";
            if (target) dbg << " " << target->unit;
#endif

#if DRAW_TARGETING
            if (target)
            {
                CherryVis::drawLine(unit->lastPosition.x,
                                    unit->lastPosition.y,
                                    target->unit->simPosition.x,
                                    target->unit->simPosition.y,
                                    CherryVis::DrawColor::White,
                                    unit->id);
            }
#endif

            result.emplace_back(unit, target ? target->unit : nullptr);
            continue;
        }

        bool isRanged = UnitUtil::IsRangedUnit(unit->type);
        bool isAntiAir = unit->type == BWAPI::UnitTypes::Protoss_Corsair;
        int distanceToTargetPosition = unit->getDistance(targetPosition);

        // Start by doing a pass to gather the types of targets we have and filter those we don't want to consider
        bool hasNonBuilding = false;
        std::vector<std::pair<Target *, int>> filteredTargets;
        for (auto &target : targets)
        {
            if (target.unit->type == BWAPI::UnitTypes::Zerg_Larva ||
                target.unit->type == BWAPI::UnitTypes::Zerg_Egg ||
                target.unit->undetected ||
                target.unit->health <= 0 ||
                !unit->canAttack(target.unit))
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " because of type / detection / health / can't attack";
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

            // Cannons can only attack what they can see and are in range of
            if (unit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
            {
                if (!target.unit->bwapiUnit->isVisible())
                {
#if DEBUG_TARGETING
                    dbg << "\n Skipping " << *target.unit << " as it is not visible";
#endif
                    continue;
                }

                auto predictedRange = unit->getDistance(target.unit, target.unit->predictPosition(BWAPI::Broodwar->getLatencyFrames()));
                if (predictedRange > (target.unit->isFlying ? unit->airRange() : unit->groundRange()))
                {
#if DEBUG_TARGETING
                    dbg << "\n Skipping " << *target.unit << " as it is not in range";
#endif
                    continue;
                }
            }

            const int range = unit->getDistance(target.unit, target.unit->simPosition);
            int distToRange = std::max(0, range - (target.unit->isFlying ? unit->airRange() : unit->groundRange()));

            // In static position mode, units only attack what they are in range of
            if (staticPosition && distToRange > 0)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " as we are in a static position and it is not in range";
#endif
                continue;
            }

            // Cliffed tanks can only be attacked by units in range with vision
            if (target.cliffedTank && (distToRange > 0 || !target.unit->bwapiUnit->isVisible()))
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " as it is a cliffed tank";
#endif
                continue;
            }

            // The next checks apply if we are not close to our target position
            if (distanceToTargetPosition > 500 && !isAntiAir && !target.inMyMainOrNatural)
            {
                // Skip targets that are out of range and moving away from us
                if (distanceToTargetPosition > 500 && distToRange > 0)
                {
                    if (!target.unit->bwapiUnit->isVisible())
                    {
#if DEBUG_TARGETING
                        dbg << "\n Skipping " << *target.unit << " as it is not visible";
#endif
                        continue;
                    }

                    auto predictedTargetPosition = target.unit->predictPosition(1);
                    if (predictedTargetPosition.isValid() && unit->getDistance(target.unit, predictedTargetPosition) > range)
                    {
#if DEBUG_TARGETING
                        dbg << "\n Skipping " << *target.unit << " as it is out of range and moving away from us";
#endif
                        continue;
                    }
                }

                // Skip targets that are further away from the target position and are either:
                // - Out of range
                // - In our range, but we aren't in their range, and we are on cooldown
                if (unit->isFlying == target.unit->isFlying && unit->distToTargetPosition != -1 && target.unit->distToTargetPosition != -1
                    && unit->distToTargetPosition < target.unit->distToTargetPosition
                    && (distToRange > 0
                        || (unit->cooldownUntil > currentFrame && !unit->isInEnemyWeaponRange(target.unit))))
                {
#if DEBUG_TARGETING
                    dbg << "\n Skipping " << *target.unit << " as it is further away from the target position";
#endif
                    continue;
                }
            }

            // This is a suitable target
            filteredTargets.emplace_back(&target, distToRange);

            if (target.priority > 7) hasNonBuilding = true;
        }

        attackers.emplace_back(unit);
        auto &attacker = *attackers.rbegin();
        attacker.targets.reserve(filteredTargets.size());
        attacker.closeTargets.reserve(filteredTargets.size());
        for (auto &targetAndDistToRange : filteredTargets)
        {
            auto &target = *(targetAndDistToRange.first);

            // If we are targeting an enemy base, ignore outlying buildings (except static defense) unless we have a higher-priority target
            // Rationale: When we have a non-building target, we want to consider buildings since they might be blocking us from attacking them
            if (target.priority < 7 && (!hasNonBuilding || target.unit->isFlying) && targetIsReachableEnemyBase && distanceToTargetPosition > 200
                && !isAntiAir)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.unit << " as priority < 7, targetIsReachableEnemyBase, distanceToTargetPosition > 200, not anti-air";
#endif
                continue;
            }

#if DEBUG_TARGETING
            dbg << "\n Added target " << *target.unit;
#endif
            attacker.targets.emplace_back(&target);

            int framesToAttack = unit->cooldownUntil - currentFrame;
            if (unit->type != BWAPI::UnitTypes::Protoss_Photon_Cannon)
            {
                framesToAttack = std::max(
                        framesToAttack,
                        (int) ((double) targetAndDistToRange.second / unit->type.topSpeed()) + BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
            }

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
        if (a.targets.size() > b.targets.size()) return false;

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
        bool bestVisible = false;

        bool isRanged = UnitUtil::IsRangedUnit(unit->type);
        int cooldownMoveFrames = std::max(0,
                                          unit->cooldownUntil - currentFrame - BWAPI::Broodwar->getRemainingLatencyFrames() - 2);

        int distanceToTargetPosition = unit->getDistance(targetPosition);
        for (auto &potentialTarget : attacker.targets)
        {
            if (potentialTarget->healthIncludingShields <= 0)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping target scoring " << *potentialTarget->unit << " as it is predicted to be dead";
#endif
                continue;
            }

            // If we have a visible target, ignore non-visible
            if (bestVisible && !potentialTarget->unit->bwapiUnit->isVisible()) continue;

            // Initialize the score as a formula of the target priority and how far outside our attack range it is
            // Each priority step is equivalent to 2 tiles
            // If the unit is on cooldown, we assume it can move towards the target before attacking
            const int targetDist = unit->getDistance(potentialTarget->unit, potentialTarget->unit->simPosition);
            const int range = potentialTarget->unit->isFlying ? unit->airRange() : unit->groundRange();
            int score = 2 * 32 * potentialTarget->priority
                        - std::max(0, targetDist - (int) ((double) cooldownMoveFrames * unit->type.topSpeed()) - range);

            // Now adjust the score according to some rules

            // Give a bonus to units that are already in range
            // Melee units get an extra bonus, as they have a more difficult time getting around blocking things
            if (targetDist <= range)
            {
                score += (isRanged ? 64 : 160);
            }

            // Give a bonus to injured targets
            // This is what provides some focus fire behaviour, as we simulate previous attackers' hits
            double healthPercentage = (double) potentialTarget->healthIncludingShields /
                                      (double) (potentialTarget->unit->type.maxHitPoints() + potentialTarget->unit->type.maxShields());
            score += (int) (160.0 * (1.0 - healthPercentage));

            // Penalize ranged units fighting uphill
            if (isRanged && BWAPI::Broodwar->getGroundHeight(unit->tilePositionX, unit->tilePositionY) <
                            BWAPI::Broodwar->getGroundHeight(potentialTarget->unit->tilePositionX, potentialTarget->unit->tilePositionY))
            {
                score -= 2 * 32;
            }

            // Avoid defensive matrix
            if (potentialTarget->unit->bwapiUnit->isDefenseMatrixed())
            {
                score -= 4 * 32;
            }

            // Give a bonus for enemies that are closer to our target position (usually the enemy base)
            if (potentialTarget->unit->getDistance(targetPosition) < distanceToTargetPosition)
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

            // Give a big bonus to SCVs repairing a bunker that we can attack without coming into range of the bunker
            if (potentialTarget->unit->bwapiUnit->isRepairing() &&
                potentialTarget->unit->bwapiUnit->getOrderTarget() &&
                potentialTarget->unit->bwapiUnit->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Bunker &&
                unit->getDistance(potentialTarget->unit, potentialTarget->unit->simPosition) <= range)
            {
                auto bunker = Units::get(potentialTarget->unit->bwapiUnit->getOrderTarget());
                if (bunker && !unit->isInEnemyWeaponRange(bunker))
                {
                    score += 256;
                }
            }

#if DEBUG_TARGETING
            dbg << "\n Target " << *potentialTarget->unit << " scored: score=" << score
                << ", attackerCount=" << potentialTarget->attackerCount
                << ", dist=" << targetDist
                << ", visible=" << potentialTarget->unit->bwapiUnit->isVisible();
#endif

            // See if this is the best target
            // Criteria:
            // - Score is higher
            // - Attackers is higher
            // - Distance is lower
            auto visible = potentialTarget->unit->bwapiUnit->isVisible();
            if ((!bestVisible && visible) ||
                score > bestScore ||
                (score == bestScore && potentialTarget->attackerCount > bestAttackerCount) ||
                (score == bestScore && potentialTarget->attackerCount == bestAttackerCount && targetDist < bestDist))
            {
                bestScore = score;
                bestAttackerCount = potentialTarget->attackerCount;
                bestDist = targetDist;
                bestTarget = potentialTarget;
                bestVisible = visible;

#if DEBUG_TARGETING
                dbg << " (best)";
#endif
            }
        }

        // For carriers, avoid frequently switching targets
        if (unit->type == BWAPI::UnitTypes::Protoss_Carrier)
        {
            auto currentTarget = getCurrentTarget(unit);
            if (currentTarget && unit->getDistance(currentTarget->unit) < 11 * 32 &&
                unit->lastCommandFrame > (currentFrame - 96))
            {
                bestTarget = currentTarget;

#if DEBUG_TARGETING
                dbg << "\n Locking carrier to current target as it is still in range";
#endif
            }
        }

        if (bestTarget)
        {
            // Only simulate dealt damage if the unit is already in our weapon range
            // Otherwise especially melee units will be simulated very badly
            if (unit->isInOurWeaponRange(bestTarget->unit))
            {
                bestTarget->dealDamage(unit);
            }
            result.emplace_back(attacker.unit, bestTarget->unit);

#if DEBUG_TARGETING
            dbg << "\n Selected target: " << *bestTarget->unit;
#endif

#if DRAW_TARGETING
            CherryVis::drawLine(attacker.unit->lastPosition.x,
                                attacker.unit->lastPosition.y,
                                bestTarget->unit->simPosition.x,
                                bestTarget->unit->simPosition.y,
                                CherryVis::DrawColor::White,
                                attacker.unit->id);
#endif
        }
        else
        {
            result.emplace_back(attacker.unit, nullptr);

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
