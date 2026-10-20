#include "EarlyGameDefendMainBaseSquad.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"

#include "DebugFlag_CombatSim.h"
#include "DebugFlag_UnitOrders.h"

namespace
{
    bool inMain(BWAPI::Position pt)
    {
        auto choke = Map::getMyMainChoke();
        if (choke && choke->midpoint.getApproxDistance(pt) < 320) return true;

        return Map::getMyMainAreas().contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(pt)));
    }

    bool addEnemyUnits(std::set<Unit> &enemyUnits, Choke *choke, BWAPI::Position clusterCenter = BWAPI::Positions::Invalid, int clusterRadius = -1)
    {
        /* Get enemy combat units in our base */
        auto combatUnitSeenRecentlyPredicate = [](const Unit &gameUnit)
        {
            if (!gameUnit->type.isBuilding() && gameUnit->lastSeen < (frameNow - 48)) return false;
            if (!UnitHelper::IsCombatUnit(gameUnit->type) && gameUnit->lastSeenAttacking < (frameNow - 120)) return false;
            if (!gameUnit->isTransport() && !UnitHelper::CanAttackGround(gameUnit->type)) return false;

            return true;
        };
        for (const auto &area : Map::getMyMainAreas())
        {
            UnitMgr::enemyInArea(enemyUnits, area, combatUnitSeenRecentlyPredicate);
        }

        /* Determine if there is an enemy in our base, i.e. has passed the choke */
        bool enemyInOurBase;
        if (choke && choke->isNarrowChoke)
        {
            enemyInOurBase = false;
            for (const auto &gameUnit : enemyUnits)
            {
                if (!Map::isInNarrowChoke(gameUnit->getTilePosition()))
                {
                    enemyInOurBase = true;
                    break;
                }
            }
        }
        else
        {
            enemyInOurBase = !enemyUnits.empty();
        }

        /* If there is a choke, get enemy combat units close to it */
        if (choke)
        {
            UnitMgr::enemyInRadius(enemyUnits, choke->midpoint, 192, combatUnitSeenRecentlyPredicate);
        }

        /* If we passed a cluster center and a valid radius, get enemy combat units close to it */
        if (clusterCenter != BWAPI::Positions::Invalid && clusterRadius > 0)
        {
            UnitMgr::enemyInRadius(enemyUnits, clusterCenter, clusterRadius + BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 32);
        }

        /* If there are no enemy combat units, include enemy buildings to defend against gas steals or other cheese */
        if (enemyUnits.empty())
        {
            for (const auto &area : Map::getMyMainAreas())
            {
                UnitMgr::enemyInArea(enemyUnits, area, [](const Unit &gameUnit)
                {
                    return gameUnit->type.isBuilding() && !gameUnit->isFlying;
                });
            }

            enemyInOurBase = !enemyUnits.empty();
        }

        return enemyInOurBase;
    }

    bool shouldStartAttack(UnitCluster &clump, const CombatSimResult &simResult)
    {
        /* Don't start an attack until we have 24 frames of recommending attack with the same number of friendly units */

        /* First ensure the sim has recommended attack with the same number of friendly units for the past 24 frames */
        int count = 0;
        for (auto iterRef = clump.recentSimResults.rbegin(); iterRef != clump.recentSimResults.rend() && count < 24; iterRef++)
        {
            if (!iterRef->second)
            {
#if DEBUG_COMBATSIM
                Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                                 << ": aborting as the sim has recommended regrouping within the past 24 frames";
#endif
                return false;
            }

            if (simResult.myUnitCount != iterRef->first.myUnitCount)
            {
#if DEBUG_COMBATSIM
                Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                                 << ": aborting as the number of friendly units has changed within the past 24 frames";
#endif
                return false;
            }

            count++;
        }

        if (count < 24)
        {
#if DEBUG_COMBATSIM
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                             << ": aborting as we have fewer than 24 frames of sim data";
#endif
            return false;
        }

        /* TODO: check that the sim result has been stable */

#if DEBUG_COMBATSIM
        Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                         << ": starting attack as the sim has recommended doing so for the past 24 frames";
#endif
        return true;
    }

    bool shouldAbortAttack(UnitCluster &clump, const CombatSimResult &simResult)
    {
        /* If this is the first run of the combat sim for this fight, always abort immediately */
        if (clump.recentSimResults.size() < 2)
        {
#if DEBUG_COMBATSIM
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": aborting as this is the first sim result";
#endif
            return true;
        }

        CombatSimResult &previousSimResult = clump.recentSimResults.rbegin()[1].first;

        /* If the number of enemy units has increased, abort the attack: the enemy has reinforced or we have discovered previously-unseen enemy units */
        if (simResult.enemyUnitCount > previousSimResult.enemyUnitCount)
        {
#if DEBUG_COMBATSIM
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": aborting as there are now more enemy units";
#endif
            return true;
        }

        /* Otherwise only abort the attack when the sim has been stable for a number of frames */
        if (clump.recentSimResults.size() < 12)
        {
#if DEBUG_COMBATSIM
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": continuing as we have fewer than 12 frames of sim data";
#endif
            return false;
        }

        int count = 0;
        for (auto iterRef = clump.recentSimResults.rbegin(); iterRef != clump.recentSimResults.rend() && count < 12; iterRef++)
        {
            if (iterRef->second)
            {
#if DEBUG_COMBATSIM
                Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                                 << ": continuing as a sim result within the past 12 frames recommended attacking";
#endif
                return false;
            }
            count++;
        }

#if DEBUG_COMBATSIM
        Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                         << ": aborting as the sim has recommended regrouping for the past 12 frames";
#endif
        return true;
    }

    /* Returns true if we have a unit currently trying to pass through this choke */
    bool blockedFriendlyUnit(Choke *choke, const std::map<OwnedUnit, std::shared_ptr<UnitCluster>> &squadUnits)
    {
        for (const auto &myUnit : UnitMgr::allMine())
        {
            if (!myUnit->movingTo().isValid()) continue;
            if (myUnit->getDistance(choke->midpoint) > 320) continue;

            /* Don't include units in this squad as this can cause unstable behaviour */
            if (squadUnits.find(myUnit) != squadUnits.end()) continue;

            auto chokePath = Pathfinder::GetChokePointPath(myUnit->prevPos,
                                                            myUnit->movingTo(),
                                                            myUnit->type,
                                                            Pathfinder::PathFindingOptions::UseNearestBWEMArea);
            for (const auto &bwemChoke : chokePath)
            {
                if (choke->choke == bwemChoke) return true;
            }
        }

        return false;
    }
}

EarlyGameDefendMainBaseSquad::EarlyGameDefendMainBaseSquad()
        : UnitSquad("Defend main base")
        , choke(nullptr)
        , workerDefenseSquad(std::make_unique<WorkerDefenseSquad>(Map::getMyMain()))
{
    initializeChoke();
}

bool EarlyGameDefendMainBaseSquad::canTransitionToAttack() const
{
    /* Don't transition if any clusters are not attacking */
    for (const auto &clump : clusters)
    {
        if (clump->currentActivity != UnitCluster::Activity::Attacking) return false;
    }

    /* Get the vanguard cluster */
    auto vanguard = vanguardCluster();
    if (!vanguard) return false;

    /* Assert that the sim has recommended attacking for at least the past 72 frames */
    if (vanguard->recentSimResults.size() < 72) return false;
    int count = 0;
    for (auto iterRef = vanguard->recentSimResults.rbegin(); iterRef != vanguard->recentSimResults.rend() && count < 72; iterRef++)
    {
        if (!iterRef->second) return false;
        count++;
    }

    return true;
}

void EarlyGameDefendMainBaseSquad::initializeChoke()
{
    auto baseLoc = Map::getMyMain();
    choke = Map::getMyMainChoke();

    if (choke)
    {
        targetPosition = choke->midpoint;

        if (choke->isNarrowChoke)
        {
            auto grid = Pathfinder::getNavigationGrid(baseLoc->getPosition());
            auto end1Node = grid ? &(*grid)[choke->end1Center] : nullptr;
            auto end2Node = grid ? &(*grid)[choke->end2Center] : nullptr;
            if (end1Node && end1Node->nextNode && end2Node && end2Node->nextNode)
            {
                chokeDefendEnd = end1Node->cost < end2Node->cost ? choke->end1Center : choke->end2Center;
            }
            else
            {
                auto end1Dist = Pathfinder::GetGroundDistance(baseLoc->getPosition(),
                                                               choke->end1Center,
                                                               BWAPI::UnitTypes::Protoss_Dragoon,
                                                               Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                auto end2Dist = Pathfinder::GetGroundDistance(baseLoc->getPosition(),
                                                               choke->end2Center,
                                                               BWAPI::UnitTypes::Protoss_Dragoon,
                                                               Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                chokeDefendEnd = end1Dist < end2Dist ? choke->end1Center : choke->end2Center;
            }
        }
    }
    else
    {
        targetPosition = baseLoc->mineralLineCenter;
    }
}

void EarlyGameDefendMainBaseSquad::execute()
{
    UnitSquad::execute();

    if (clusters.empty())
    {
        std::set<Unit> enemyUnits;
        addEnemyUnits(enemyUnits, choke);

        auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);
        std::vector<std::pair<OwnedUnit, Unit>> emptyUnitsAndTargets;
        workerDefenseSquad->execute(workersAndTargets, emptyUnitsAndTargets);
    }
}

void EarlyGameDefendMainBaseSquad::execute(UnitCluster &clump)
{
    if (Map::getMyMainChoke() != choke) initializeChoke();

    /* Against terran, add units that are close to our cluster units */
    /* This handles cases where the enemy draws them out of the choke */
    int furthestFromCenter = -1;
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
    {
        for (auto &gameUnit : clump.units)
        {
            furthestFromCenter = std::max(furthestFromCenter, gameUnit->prevPos.getApproxDistance(clump.midpoint));
        }
    }

    std::set<Unit> enemyUnits;
    bool enemyInOurBase = addEnemyUnits(enemyUnits, choke, clump.midpoint, furthestFromCenter);

    updateDetectionNeeds(enemyUnits);

    /* Get worker targets */
    /* The returned list includes any worker that is being threatened by an enemy unit */
    auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);

    /* Select targets */
    auto unitsAndTargets = clump.selectTargets(enemyUnits, targetPosition);

    /* Consider enemy units in a bit larger radius to the choke for the combat sim */
    if (choke)
    {
        UnitMgr::enemyInRadius(enemyUnits, choke->midpoint, 256, [](const Unit &gameUnit)
        {
            if (!gameUnit->type.isBuilding() && gameUnit->lastSeen < (frameNow - 48)) return false;
            if (!UnitHelper::IsCombatUnit(gameUnit->type) && gameUnit->lastSeenAttacking < (frameNow - 120)) return false;
            if (!gameUnit->isTransport() && !UnitHelper::CanAttackGround(gameUnit->type)) return false;

            return true;
        });
    }

    /* Run combat sim */
    auto simResult = clump.runCombatSim(targetPosition, unitsAndTargets, enemyUnits, detectors, false);

    /* Make the attack / retreat decision based on the sim result */
    /* TODO: Needs tuning */
    bool attack = simResult.myPercentLost() <= 0.001 ||
                  simResult.percentGain() > -0.1;

#if DEBUG_COMBATSIM_LOG
    Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                     << ": %l=" << simResult.myPercentLost()
                     << "; vg=" << simResult.valueGain()
                     << "; %g=" << simResult.percentGain()
                     << (attack ? "; ATTACK" : "; RETREAT");
#endif

    clump.addSimResult(simResult, attack);

    /* Make the final decision based on what state we are currently in */

    /* Currently regrouping, but want to attack: do so once the sim has stabilized */
    if (attack && clump.currentActivity == UnitCluster::Activity::Regrouping)
    {
        attack = shouldStartAttack(clump, simResult);
    }

    /* Currently attacking, but want to regroup: make sure regrouping is safe */
    if (!attack && clump.currentActivity == UnitCluster::Activity::Attacking)
    {
        attack = !shouldAbortAttack(clump, simResult);
    }

    /* If the enemy has cloaked units and we have a choke, always stay at the choke */
    /* Rationale: we want to plug the choke to keep the cloked units from getting in until we get detection */
    if (!enemiesNeedingDetection.empty() && choke) attack = true;

    /* Hold the choke against Zerg if we have enough zealots to block it */
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && !attack && !enemyInOurBase && choke && choke->isNarrowChoke)
    {
        int zealotCount = 0;
        for (const auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.first->type == BWAPI::UnitTypes::Protoss_Zealot &&
                unitAndTarget.first->getDistance(choke->midpoint) < 250)
            {
                zealotCount++;
            }
        }

        if (zealotCount > (choke->width / 30)) attack = true;
    }

    if (attack)
    {
        clump.setActivity(UnitCluster::Activity::Attacking);

        /* Reset our defensive position to the choke when all enemy units are out of our base */
        if ((!enemiesNeedingDetection.empty() || !enemyInOurBase) && choke) targetPosition = choke->midpoint;

        /* Check if the enemy has static defense (e.g. cannon rush) */
        bool hasStaticDefense = false;
        for (const auto &gameUnit : enemyUnits)
        {
            if (UnitHelper::IsStationaryAttacker(gameUnit->type))
            {
                hasStaticDefense = true;
                break;
            }
        }

        /* Choose the type of micro depending on whether the enemy has static defense, we are holding a narrow choke, or neither */
        if (hasStaticDefense)
        {
            clump.containStatic(enemyUnits, targetPosition);
        }
        else if (enemiesNeedingDetection.empty() && !enemyInOurBase && choke && choke->isNarrowChoke)
        {
            if (enemyUnits.empty() && UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Dark_Templar).empty() && blockedFriendlyUnit(choke, unitToCluster))
            {
                clump.attack(unitsAndTargets, Map::getMyMain()->mineralLineCenter);
            }
            else
            {
                clump.holdChoke(choke, chokeDefendEnd, unitsAndTargets);
            }
        }
        else
        {
            clump.attack(unitsAndTargets, targetPosition);
        }

        if (clump.isVanguardCluster)
        {
            workerDefenseSquad->execute(workersAndTargets, unitsAndTargets);
        }

        return;
    }

    clump.setActivity(UnitCluster::Activity::Regrouping);

    /* Ensure the target position is set to the mineral line center */
    /* This allows our workers to help with the defense */
    targetPosition = Map::getMyMain()->mineralLineCenter;

    /* Micro our units according to the following flowchart: */
    /* - If there is a threatened worker, attack the unit threatening it. */
    /* - If we are at the mineral line center, attack it. */
    /* - Move to the mineral line center. */

    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &gameUnit = unitAndTarget.first;

        /* If the unit is stuck, unstick it */
        if (gameUnit->unstick()) continue;

        /* If the unit is not ready (i.e. is already in the middle of an attack), don't touch it */
        if (!gameUnit->isReady()) continue;

        /* Set our target to the enemy attacking the closest threatened worker */
        int closestThreatenedWorkerDist = INT_MAX;
        for (const auto &workerAndTarget : workersAndTargets)
        {
            int span = gameUnit->getDistance(workerAndTarget.first);
            if (span < closestThreatenedWorkerDist)
            {
                unitAndTarget.second = workerAndTarget.second;
                closestThreatenedWorkerDist = span;
            }
        }
        if (closestThreatenedWorkerDist < INT_MAX)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(unitAndTarget.first->ident) << "Targeting worker threat: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->simPosition);
#endif
            gameUnit->attackUnit(unitAndTarget.second, unitsAndTargets, false);
            continue;
        }

        /* Attack the target if we are at the mineral line center and in the enemy's attack range */
        if (unitAndTarget.second &&
            gameUnit->getDistance(targetPosition) < 32 &&
            gameUnit->isInEnemyWeaponRange(unitAndTarget.second, 32))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(unitAndTarget.first->ident) << "Target: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->simPosition);
#endif
            gameUnit->attackUnit(unitAndTarget.second, unitsAndTargets, false);
            continue;
        }

#if DEBUG_UNIT_ORDERS
        Telemetry::log(unitAndTarget.first->ident) << "Retreating: move to " << BWAPI::WalkPosition(targetPosition);
#endif
        gameUnit->moveTo(targetPosition);
    }

    if (clump.isVanguardCluster)
    {
        workerDefenseSquad->execute(workersAndTargets, unitsAndTargets);
    }
}

bool EarlyGameDefendMainBaseSquad::canAddUnitToCluster(const OwnedUnit &gameUnit, const std::shared_ptr<UnitCluster> &clump, int span) const
{
    bool unitInMain = inMain(gameUnit->prevPos);
    bool clusterInMain = inMain(clump->vanguard ? clump->vanguard->prevPos : clump->midpoint);
    if (unitInMain && clusterInMain) return true;
    if (unitInMain || clusterInMain) return false;
    return UnitSquad::canAddUnitToCluster(gameUnit, clump, span);
}

bool EarlyGameDefendMainBaseSquad::shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const
{
    bool firstInMain = inMain(first->vanguard ? first->vanguard->prevPos : first->midpoint);
    bool secondInMain = inMain(second->vanguard ? second->vanguard->prevPos : second->midpoint);
    if (firstInMain && secondInMain) return true;
    if (firstInMain || secondInMain) return false;
    return UnitSquad::shouldCombineClusters(first, second);
}

bool EarlyGameDefendMainBaseSquad::shouldRemoveFromCluster(const OwnedUnit &gameUnit, const std::shared_ptr<UnitCluster> &clump) const
{
    if (inMain(gameUnit->prevPos)) return false;
    return UnitSquad::shouldRemoveFromCluster(gameUnit, clump);
}
