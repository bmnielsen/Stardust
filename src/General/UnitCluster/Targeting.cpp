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
        /* This is similar to the target prioritization in Locutus that originally came from Steamhammer/UAlbertaBot */

        const BWAPI::UnitType targetType = target->type;
        auto notCloseToTargetPosition = [&target]()
        {
            return target->distToTargetPosition > 1500;
        };

        if (targetType == BWAPI::UnitTypes::Zerg_Infested_Terran ||
            targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
            targetType == BWAPI::UnitTypes::Protoss_Reaver ||
            (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && !target->burrowed) ||
            (targetType == BWAPI::UnitTypes::Protoss_Observer && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0))
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

        /* Proxies */
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
            if ((target->rawUnit->isConstructing() || target->rawUnit->isRepairing()) && notCloseToTargetPosition())
            {
                return 15;
            }

            /* Blocking a narrow choke makes you critical. */
            if (Map::isInNarrowChoke(target->getTilePosition()))
            {
                return 14;
            }

            /* Repairing */
            if (target->rawUnit->isRepairing() && target->rawUnit->getOrderTarget())
            {
                /* Something that can shoot */
                if (target->rawUnit->getOrderTarget()->getType().groundWeapon() != BWAPI::WeaponTypes::None)
                {
                    return 14;
                }

                /* A bunker: only target the workers if we can't outrange the bunker */
                if (target->rawUnit->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Bunker &&
                    PlayerInfo::weaponRange(target->player, BWAPI::UnitTypes::Terran_Marine.groundWeapon()) > 128)
                {
                    return 13;
                }
            }

            /* Workers that have attacked in the last four seconds */
            if ((frameNow - target->lastSeenAttacking) < 96)
            {
                return 11;
            }

            if (target->rawUnit->isConstructing())
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

        if (!target->completed || (targetType.requiresPsi() && !target->rawUnit->isPowered()))
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

    bool isInOurMainOrNatural(const Unit &gameUnit)
    {
        if (!gameUnit->lastPositionValid) return false;

        auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos));
        auto mainAndNaturalAreas = Map::getMyMainAreas();
        auto natBase = Map::getMyNatural();
        if (natBase)
        {
            mainAndNaturalAreas.insert(natBase->getArea());
        }

        return mainAndNaturalAreas.contains(area);
    }

    struct Target
    {
        Unit gameUnit;
        int priority;               // Base priority computed by targetPriority above
        int healthIncludingShields; // Estimated health reduced by incoming bullets and earlier attackers
        int attackerCount;          // How many attackers have this target in their closeTargets vector
        bool cliffedTank;
        bool inMyMainOrNatural;

        explicit Target(const Unit &gameUnit, const OwnedUnit &vanguard)
                : gameUnit(gameUnit)
                , priority(targetPriority(gameUnit))
                , healthIncludingShields(gameUnit->health + gameUnit->shields)
                , attackerCount(0)
                , cliffedTank(gameUnit->isCliffedTank(vanguard))
                , inMyMainOrNatural(isInOurMainOrNatural(gameUnit)) {}

        void dealDamage(const OwnedUnit &attacker)
        {
            if (gameUnit->isBeingHealed()) return;

            int damage = PlayerInfo::attackDamage(attacker->player, attacker->type, gameUnit->player, gameUnit->type);

            /* For low-ground to high-ground attacks, simulate half damage */
            if (!attacker->isFlying && UnitHelper::IsRangedUnit(attacker->type) &&
                BWAPI::Broodwar->getGroundHeight(attacker->getTilePosition()) < BWAPI::Broodwar->getGroundHeight(gameUnit->getTilePosition()))
            {
                damage /= 2;
            }

            healthIncludingShields -= damage;
        }
    };

    struct Attacker
    {
        OwnedUnit gameUnit;
        std::vector<Target *> targets;      // All of the targets available to this attacker
        int framesToAttack;                 // The number of frames before this attacker can attack something
        std::vector<Target *> closeTargets; // All targets that can be attacked at framesToAttack

        explicit Attacker(OwnedUnit gameUnit) : gameUnit(std::move(gameUnit)), framesToAttack(INT_MAX) {}
    };

    bool isTargetReachableEnemyBase(BWAPI::Position targetPosition, const OwnedUnit &vanguard)
    {
        /* First check if the target is an enemy base */
        auto targetBase = Map::baseNear(targetPosition);
        if (!targetBase) return false;
        if (targetBase->owner != BWAPI::Broodwar->enemy()) return false;
        if (targetBase->lastScouted != -1 && !targetBase->resourceDepot) return false;

        if (!vanguard) return true;

        auto grid = Pathfinder::getNavigationGrid(targetBase->getPosition());
        if (!grid) return true;

        auto &node = (*grid)[vanguard->prevPos];
        return node.nextNode != nullptr;
    }
}

std::vector<std::pair<OwnedUnit, Unit>>
UnitCluster::selectTargets(std::set<Unit> &targetUnits, BWAPI::Position targetPosition, bool staticPosition)
{
    /* Perform a scan to remove target units that are at much different distances from our target position than the vanguard unit */
    /* This indicates we have picked up enemy units that are in a different region separated from us by a cliff */
    if (!vanguard->isFlying)
    {
        for (auto iterRef = targetUnits.begin(); iterRef != targetUnits.end();)
        {
            auto &gameUnit = *iterRef;
            if (!gameUnit->isFlying)
            {
                gameUnit->distToTargetPosition = Pathfinder::GetGroundDistance(
                        gameUnit->simPosition,
                        targetPosition,
                        gameUnit->type,
                        Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);

                if (gameUnit->distToTargetPosition != -1 && abs(gameUnit->distToTargetPosition - vanguardDistToTarget) > 700
                    && !vanguard->isInEnemyWeaponRange(gameUnit))
                {
                    iterRef = targetUnits.erase(iterRef);
                    continue;
                }
            }

            iterRef++;
        }
    }

    std::vector<std::pair<OwnedUnit, Unit>> outcome;

    /* For our targeting we want to know if we are attacking a reachable enemy base */
    /* Criteria: */
    /* - Not in static position mode */
    /* - Target is near a base */
    /* - The base is owned by the enemy */
    /* - The base has a resource depot or hasn't been scouted yet */
    /* - The base has a navigation grid path from our main choke (i.e. hasn't been walled-off) */
    bool targetIsReachableEnemyBase = !staticPosition && isTargetReachableEnemyBase(targetPosition, vanguard);

    /* Create the target objects */
    /* This also updates the enemy AOE radius */
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
    dbg << "Targeting for cluster " << BWAPI::TilePosition(midpoint) << " - targetIsReachableEnemyBase=" << targetIsReachableEnemyBase;
#endif

    auto getCurrentTarget = [&targets](const OwnedUnit &gameUnit) -> Target *
    {
        if (gameUnit->rawUnit->getLastCommand().type != BWAPI::UnitCommandTypes::Attack_Unit) return nullptr;

        auto targetUnit = UnitMgr::get(gameUnit->rawUnit->getLastCommand().getTarget());
        if (!targetUnit) return nullptr;

        for (auto &target : targets)
        {
            if (target.gameUnit == targetUnit) return &target;
        }

        return nullptr;
    };

    /* Perform a pre-scan to get valid targets and the frame at which we can attack them for each unit */
    std::vector<Attacker> attackers;
    attackers.reserve(units.size());
    for (const auto &gameUnit : units)
    {
#if DEBUG_TARGETING
        dbg << "\n" << *gameUnit << " target filtering:";
#endif

        /* If the unit isn't ready, lock it to its current target and skip targeting completely */
        if (!gameUnit->isReady())
        {
            auto target = getCurrentTarget(gameUnit);

            /* If the unit isn't on its cooldown yet, simulate the attack on the target for use in later targeting */
            /* If the unit is on its cooldown, there will be an upcomingAttack already registered on the target */
            /* This ensures consistency between our targeting and combat sim (avoiding double-counting of damage) */
            if (target && (gameUnit->cooldownUntil - frameNow) <= (BWAPI::Broodwar->getLatencyFrames() + 2))
            {
                target->dealDamage(gameUnit);
            }

#if DEBUG_TARGETING
            dbg << "\n Not ready, locking to current target";
            if (target) dbg << " " << target->gameUnit;
#endif

#if DRAW_TARGETING
            if (target)
            {
                Telemetry::drawLine(gameUnit->prevPos.x,
                                    gameUnit->prevPos.y,
                                    target->gameUnit->simPosition.x,
                                    target->gameUnit->simPosition.y,
                                    Telemetry::DrawColor::White,
                                    gameUnit->ident);
            }
#endif

            outcome.emplace_back(gameUnit, target ? target->gameUnit : nullptr);
            continue;
        }

        bool isRanged = UnitHelper::IsRangedUnit(gameUnit->type);
        bool isAntiAir = gameUnit->type == BWAPI::UnitTypes::Protoss_Corsair;
        int distanceToTargetPosition = gameUnit->getDistance(targetPosition);

        /* Start by doing a pass to gather the types of targets we have and filter those we don't want to consider */
        bool hasNonBuilding = false;
        std::vector<std::pair<Target *, int>> filteredTargets;
        for (auto &target : targets)
        {
            if (target.gameUnit->type == BWAPI::UnitTypes::Zerg_Larva ||
                target.gameUnit->type == BWAPI::UnitTypes::Zerg_Egg ||
                target.gameUnit->undetected ||
                target.gameUnit->health <= 0 ||
                !gameUnit->canAttack(target.gameUnit))
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.gameUnit << " because of type / detection / health / can't attack";
#endif
                continue;
            }

            /* Ranged cannot hit targets under dark swarm */
            if ((isRanged || gameUnit->type.isWorker())
                && gameUnit->type != BWAPI::UnitTypes::Protoss_Reaver
                && target.gameUnit->rawUnit->isUnderDarkSwarm())
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.gameUnit << " as under dark swarm";
#endif
                continue;
            }

            /* Melee cannot hit targets under disruption web and don't want to attack targets under storm */
            if (!isRanged && (target.gameUnit->rawUnit->isUnderDisruptionWeb() || target.gameUnit->rawUnit->isUnderStorm()))
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.gameUnit << " as under disruption web";
#endif
                continue;
            }

            /* Cannons can only attack what they can see and are in range of */
            if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
            {
                if (!target.gameUnit->rawUnit->isVisible())
                {
#if DEBUG_TARGETING
                    dbg << "\n Skipping " << *target.gameUnit << " as it is not visible";
#endif
                    continue;
                }

                auto predictedRange = gameUnit->getDistance(target.gameUnit, target.gameUnit->predictPosition(BWAPI::Broodwar->getLatencyFrames()));
                if (predictedRange > (target.gameUnit->isFlying ? gameUnit->airRange() : gameUnit->groundRange()))
                {
#if DEBUG_TARGETING
                    dbg << "\n Skipping " << *target.gameUnit << " as it is not in range";
#endif
                    continue;
                }
            }

            const int range = gameUnit->getDistance(target.gameUnit, target.gameUnit->simPosition);
            int distToRange = std::max(0, range - (target.gameUnit->isFlying ? gameUnit->airRange() : gameUnit->groundRange()));

            /* In static position mode, units only attack what they are in range of */
            if (staticPosition && distToRange > 0)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.gameUnit << " as we are in a static position and it is not in range";
#endif
                continue;
            }

            /* Cliffed tanks can only be attacked by units in range with vision */
            if (target.cliffedTank && (distToRange > 0 || !target.gameUnit->rawUnit->isVisible()))
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.gameUnit << " as it is a cliffed tank";
#endif
                continue;
            }

            /* The next checks apply if we are not close to our target position */
            if (distanceToTargetPosition > 500 && !isAntiAir && !target.inMyMainOrNatural)
            {
                /* Skip targets that are out of range and moving away from us */
                if (distanceToTargetPosition > 500 && distToRange > 0)
                {
                    if (!target.gameUnit->rawUnit->isVisible())
                    {
#if DEBUG_TARGETING
                        dbg << "\n Skipping " << *target.gameUnit << " as it is not visible";
#endif
                        continue;
                    }

                    auto predictedTargetPosition = target.gameUnit->predictPosition(1);
                    if (predictedTargetPosition.isValid() && gameUnit->getDistance(target.gameUnit, predictedTargetPosition) > range)
                    {
#if DEBUG_TARGETING
                        dbg << "\n Skipping " << *target.gameUnit << " as it is out of range and moving away from us";
#endif
                        continue;
                    }
                }

                /* Skip targets that are further away from the target position and are either: */
                /* - Out of range */
                /* - In our range, but we aren't in their range, and we are on cooldown */
                if (gameUnit->isFlying == target.gameUnit->isFlying && gameUnit->distToTargetPosition != -1 && target.gameUnit->distToTargetPosition != -1
                    && gameUnit->distToTargetPosition < target.gameUnit->distToTargetPosition
                    && (distToRange > 0
                        || (gameUnit->cooldownUntil > frameNow && !gameUnit->isInEnemyWeaponRange(target.gameUnit))))
                {
#if DEBUG_TARGETING
                    dbg << "\n Skipping " << *target.gameUnit << " as it is further away from the target position";
#endif
                    continue;
                }
            }

            /* This is a suitable target */
            filteredTargets.emplace_back(&target, distToRange);

            if (target.priority > 7) hasNonBuilding = true;
        }

        attackers.emplace_back(gameUnit);
        auto &attacker = *attackers.rbegin();
        attacker.targets.reserve(filteredTargets.size());
        attacker.closeTargets.reserve(filteredTargets.size());
        for (auto &targetAndDistToRange : filteredTargets)
        {
            auto &target = *(targetAndDistToRange.first);

            /* If we are targeting an enemy base, ignore outlying buildings (except static defense) unless we have a higher-priority target */
            /* Rationale: When we have a non-building target, we want to consider buildings since they might be blocking us from attacking them */
            if (target.priority < 7 && (!hasNonBuilding || target.gameUnit->isFlying) && targetIsReachableEnemyBase && distanceToTargetPosition > 200
                && !isAntiAir)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping " << *target.gameUnit << " as priority < 7, targetIsReachableEnemyBase, distanceToTargetPosition > 200, not anti-air";
#endif
                continue;
            }

#if DEBUG_TARGETING
            dbg << "\n Added target " << *target.gameUnit;
#endif
            attacker.targets.emplace_back(&target);

            int framesToAttack = gameUnit->cooldownUntil - frameNow;
            if (gameUnit->type != BWAPI::UnitTypes::Protoss_Photon_Cannon)
            {
                framesToAttack = std::max(
                        framesToAttack,
                        (int) ((double) targetAndDistToRange.second / gameUnit->type.topSpeed()) + BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
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

        /* Increment the close target count for all of our close targets */
        for (auto &closeTarget : attacker.closeTargets)
        {
            closeTarget->attackerCount++;
        }
    }

    /* Sort the attackers */
    std::sort(attackers.begin(), attackers.end(), [](const Attacker &a, const Attacker &b)
    {
        /* First priority: lowest frames to attack */
        if (a.framesToAttack < b.framesToAttack) return true;
        if (a.framesToAttack > b.framesToAttack) return false;

        /* Second priority: lowest number of close targets */
        if (a.closeTargets.size() < b.closeTargets.size()) return true;
        if (a.closeTargets.size() > b.closeTargets.size()) return false;

        /* Third priority: lowest number of targets */
        if (a.targets.size() < b.targets.size()) return true;
        if (a.targets.size() > b.targets.size()) return false;

        /* Default: unit ID (guaranteed to be inequal) */
        return a.gameUnit->ident < b.gameUnit->ident;
    });

    /* Now assign each unit a target, skipping any that are simulated to already be dead */
    for (auto &attacker : attackers)
    {
        auto &gameUnit = attacker.gameUnit;

#if DEBUG_TARGETING
        dbg << "\n" << *gameUnit << " target scoring:";
#endif

        Target *bestTarget = nullptr;
        int bestScore = -999999;
        int bestAttackerCount = 0;
        int bestDist = INT_MAX;
        bool bestVisible = false;

        bool isRanged = UnitHelper::IsRangedUnit(gameUnit->type);
        int cooldownMoveFrames = std::max(0,
                                          gameUnit->cooldownUntil - frameNow - BWAPI::Broodwar->getRemainingLatencyFrames() - 2);

        int distanceToTargetPosition = gameUnit->getDistance(targetPosition);
        for (auto &potentialTarget : attacker.targets)
        {
            if (potentialTarget->healthIncludingShields <= 0)
            {
#if DEBUG_TARGETING
                dbg << "\n Skipping target scoring " << *potentialTarget->gameUnit << " as it is predicted to be dead";
#endif
                continue;
            }

            /* If we have a visible target, ignore non-visible */
            if (bestVisible && !potentialTarget->gameUnit->rawUnit->isVisible()) continue;

            /* Initialize the score as a formula of the target priority and how far outside our attack range it is */
            /* Each priority step is equivalent to 2 tiles */
            /* If the unit is on cooldown, we assume it can move towards the target before attacking */
            const int targetDist = gameUnit->getDistance(potentialTarget->gameUnit, potentialTarget->gameUnit->simPosition);
            const int range = potentialTarget->gameUnit->isFlying ? gameUnit->airRange() : gameUnit->groundRange();
            int score = 2 * 32 * potentialTarget->priority
                        - std::max(0, targetDist - (int) ((double) cooldownMoveFrames * gameUnit->type.topSpeed()) - range);

            /* Now adjust the score according to some rules */

            /* Give a bonus to units that are already in range */
            /* Melee units get an extra bonus, as they have a more difficult time getting around blocking things */
            if (targetDist <= range)
            {
                score += (isRanged ? 64 : 160);
            }

            /* Give a bonus to injured targets */
            /* This is what provides some focus fire behaviour, as we simulate previous attackers' hits */
            double healthPercentage = (double) potentialTarget->healthIncludingShields /
                                      (double) (potentialTarget->gameUnit->type.maxHitPoints() + potentialTarget->gameUnit->type.maxShields());
            score += (int) (160.0 * (1.0 - healthPercentage));

            /* Penalize ranged units fighting uphill */
            if (isRanged && BWAPI::Broodwar->getGroundHeight(gameUnit->tilePositionX, gameUnit->tilePositionY) <
                            BWAPI::Broodwar->getGroundHeight(potentialTarget->gameUnit->tilePositionX, potentialTarget->gameUnit->tilePositionY))
            {
                score -= 2 * 32;
            }

            /* Avoid defensive matrix */
            if (potentialTarget->gameUnit->rawUnit->isDefenseMatrixed())
            {
                score -= 4 * 32;
            }

            /* Give a bonus for enemies that are closer to our target position (usually the enemy base) */
            if (potentialTarget->gameUnit->getDistance(targetPosition) < distanceToTargetPosition)
            {
                score += 2 * 32;
            }

            /* Give bonus to units under dark swarm */
            /* Ranged units skip these targets earlier */
            if (potentialTarget->gameUnit->rawUnit->isUnderDarkSwarm())
            {
                score += 4 * 32;
            }

            /* Adjust based on the threat level of the enemy unit to us */
            if (potentialTarget->gameUnit->canAttack(gameUnit))
            {
                if (gameUnit->isInEnemyWeaponRange(potentialTarget->gameUnit))
                {
                    score += 6 * 32;
                }
                else if (gameUnit->isInOurWeaponRange(potentialTarget->gameUnit))
                {
                    score += 4 * 32;
                }
                else
                {
                    score += 3 * 32;
                }
            }

            /* Give a bonus to non-moving or braking targets, and a penalty to units that are faster than us */
            if (!potentialTarget->gameUnit->rawUnit->isMoving())
            {
                if (potentialTarget->gameUnit->rawUnit->isSieged() ||
                    potentialTarget->gameUnit->rawUnit->getOrder() == BWAPI::Orders::Sieging ||
                    potentialTarget->gameUnit->rawUnit->getOrder() == BWAPI::Orders::Unsieging)
                {
                    score += 48;
                }
                else
                {
                    score += 24;
                }
            }
            else if (potentialTarget->gameUnit->rawUnit->isBraking())
            {
                score += 16;
            }
            else if (potentialTarget->gameUnit->type.topSpeed() >= gameUnit->type.topSpeed())
            {
                score -= 4 * 32;
            }

            /* Take the damage type into account */
            BWAPI::DamageType damage = gameUnit->getWeapon(potentialTarget->gameUnit).damageType();
            if (damage == BWAPI::DamageTypes::Explosive)
            {
                if (potentialTarget->gameUnit->type.size() == BWAPI::UnitSizeTypes::Large)
                {
                    score += 32;
                }
            }
            else if (damage == BWAPI::DamageTypes::Concussive)
            {
                if (potentialTarget->gameUnit->type.size() == BWAPI::UnitSizeTypes::Small)
                {
                    score += 32;
                }
                else if (potentialTarget->gameUnit->type.size() == BWAPI::UnitSizeTypes::Large)
                {
                    score -= 32;
                }
            }

            /* Give a big bonus to SCVs repairing a bunker that we can attack without coming into range of the bunker */
            if (potentialTarget->gameUnit->rawUnit->isRepairing() &&
                potentialTarget->gameUnit->rawUnit->getOrderTarget() &&
                potentialTarget->gameUnit->rawUnit->getOrderTarget()->getType() == BWAPI::UnitTypes::Terran_Bunker &&
                gameUnit->getDistance(potentialTarget->gameUnit, potentialTarget->gameUnit->simPosition) <= range)
            {
                auto bunker = UnitMgr::get(potentialTarget->gameUnit->rawUnit->getOrderTarget());
                if (bunker && !gameUnit->isInEnemyWeaponRange(bunker))
                {
                    score += 256;
                }
            }

#if DEBUG_TARGETING
            dbg << "\n Target " << *potentialTarget->gameUnit << " scored: score=" << score
                << ", attackerCount=" << potentialTarget->attackerCount
                << ", dist=" << targetDist
                << ", visible=" << potentialTarget->gameUnit->rawUnit->isVisible();
#endif

            /* See if this is the best target */
            /* Criteria: */
            /* - Score is higher */
            /* - Attackers is higher */
            /* - Distance is lower */
            auto visible = potentialTarget->gameUnit->rawUnit->isVisible();
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

        /* For carriers, avoid frequently switching targets */
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Carrier)
        {
            auto currentTarget = getCurrentTarget(gameUnit);
            if (currentTarget && gameUnit->getDistance(currentTarget->gameUnit) < 11 * 32 &&
                gameUnit->lastCommandFrame > (frameNow - 96))
            {
                bestTarget = currentTarget;

#if DEBUG_TARGETING
                dbg << "\n Locking carrier to current target as it is still in range";
#endif
            }
        }

        if (bestTarget)
        {
            /* Only simulate dealt damage if the unit is already in our weapon range */
            /* Otherwise especially melee units will be simulated very badly */
            if (gameUnit->isInOurWeaponRange(bestTarget->gameUnit))
            {
                bestTarget->dealDamage(gameUnit);
            }
            outcome.emplace_back(attacker.gameUnit, bestTarget->gameUnit);

#if DEBUG_TARGETING
            dbg << "\n Selected target: " << *bestTarget->gameUnit;
#endif

#if DRAW_TARGETING
            Telemetry::drawLine(attacker.gameUnit->prevPos.x,
                                attacker.gameUnit->prevPos.y,
                                bestTarget->gameUnit->simPosition.x,
                                bestTarget->gameUnit->simPosition.y,
                                Telemetry::DrawColor::White,
                                attacker.gameUnit->ident);
#endif
        }
        else
        {
            outcome.emplace_back(attacker.gameUnit, nullptr);

#if DEBUG_TARGETING
            dbg << "\n No target";
#endif
        }
    }

#if DEBUG_TARGETING
    AppLog::Debug() << dbg.str();
#endif

    return outcome;
}
