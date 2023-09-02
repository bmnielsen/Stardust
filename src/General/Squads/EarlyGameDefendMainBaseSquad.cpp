#include "EarlyGameDefendMainBaseSquad.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"

#include "DebugFlag_CombatSim.h"
#include "DebugFlag_UnitOrders.h"

namespace
{
    bool inMain(BWAPI::Position pos)
    {
        auto choke = Map::getMyMainChoke();
        if (choke && choke->center.getApproxDistance(pos) < 320) return true;

        return Map::getMyMainAreas().contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(pos)));
    }

    bool addEnemyUnits(std::set<Unit> &enemyUnits, Choke *choke, BWAPI::Position clusterCenter = BWAPI::Positions::Invalid, int clusterRadius = -1)
    {
        // Get enemy combat units in our base
        auto combatUnitSeenRecentlyPredicate = [](const Unit &unit)
        {
            if (!unit->type.isBuilding() && unit->lastSeen < (currentFrame - 48)) return false;
            if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (currentFrame - 120)) return false;
            if (!unit->isTransport() && !UnitUtil::CanAttackGround(unit->type)) return false;

            return true;
        };
        for (const auto &area : Map::getMyMainAreas())
        {
            Units::enemyInArea(enemyUnits, area, combatUnitSeenRecentlyPredicate);
        }

        // Determine if there is an enemy in our base, i.e. has passed the choke
        bool enemyInOurBase;
        if (choke && choke->isNarrowChoke)
        {
            enemyInOurBase = false;
            for (const auto &unit : enemyUnits)
            {
                if (!Map::isInNarrowChoke(unit->getTilePosition()))
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

        // If there is a choke, get enemy combat units close to it
        if (choke)
        {
            Units::enemyInRadius(enemyUnits, choke->center, 192, combatUnitSeenRecentlyPredicate);
        }

        // If we passed a cluster center and a valid radius, get enemy combat units close to it
        if (clusterCenter != BWAPI::Positions::Invalid && clusterRadius > 0)
        {
            Units::enemyInRadius(enemyUnits, clusterCenter, clusterRadius + BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 32);
        }

        // If there are no enemy combat units, include enemy buildings to defend against gas steals or other cheese
        if (enemyUnits.empty())
        {
            for (const auto &area : Map::getMyMainAreas())
            {
                Units::enemyInArea(enemyUnits, area, [](const Unit &unit)
                {
                    return unit->type.isBuilding() && !unit->isFlying;
                });
            }

            enemyInOurBase = !enemyUnits.empty();
        }

        return enemyInOurBase;
    }

    bool shouldStartAttack(UnitCluster &cluster, const CombatSimResult &simResult)
    {
        // Don't start an attack until we have 24 frames of recommending attack with the same number of friendly units

        // First ensure the sim has recommended attack with the same number of friendly units for the past 24 frames
        int count = 0;
        for (auto it = cluster.recentSimResults.rbegin(); it != cluster.recentSimResults.rend() && count < 24; it++)
        {
            if (!it->second)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": aborting as the sim has recommended regrouping within the past 24 frames";
#endif
                return false;
            }

            if (simResult.myUnitCount != it->first.myUnitCount)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": aborting as the number of friendly units has changed within the past 24 frames";
#endif
                return false;
            }

            count++;
        }

        if (count < 24)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                             << ": aborting as we have fewer than 24 frames of sim data";
#endif
            return false;
        }

        // TODO: check that the sim result has been stable

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": starting attack as the sim has recommended doing so for the past 24 frames";
#endif
        return true;
    }

    bool shouldAbortAttack(UnitCluster &cluster, const CombatSimResult &simResult)
    {
        // If this is the first run of the combat sim for this fight, always abort immediately
        if (cluster.recentSimResults.size() < 2)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting as this is the first sim result";
#endif
            return true;
        }

        CombatSimResult &previousSimResult = cluster.recentSimResults.rbegin()[1].first;

        // If the number of enemy units has increased, abort the attack: the enemy has reinforced or we have discovered previously-unseen enemy units
        if (simResult.enemyUnitCount > previousSimResult.enemyUnitCount)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting as there are now more enemy units";
#endif
            return true;
        }

        // Otherwise only abort the attack when the sim has been stable for a number of frames
        if (cluster.recentSimResults.size() < 12)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing as we have fewer than 12 frames of sim data";
#endif
            return false;
        }

        int count = 0;
        for (auto it = cluster.recentSimResults.rbegin(); it != cluster.recentSimResults.rend() && count < 12; it++)
        {
            if (it->second)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": continuing as a sim result within the past 12 frames recommended attacking";
#endif
                return false;
            }
            count++;
        }

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": aborting as the sim has recommended regrouping for the past 12 frames";
#endif
        return true;
    }

    // Returns true if we have a unit currently trying to pass through this choke
    bool blockedFriendlyUnit(Choke *choke, const std::map<MyUnit, std::shared_ptr<UnitCluster>> &squadUnits)
    {
        for (const auto &myUnit : Units::allMine())
        {
            if (!myUnit->movingTo().isValid()) continue;
            if (myUnit->getDistance(choke->center) > 320) continue;

            // Don't include units in this squad as this can cause unstable behaviour
            if (squadUnits.find(myUnit) != squadUnits.end()) continue;

            auto chokePath = PathFinding::GetChokePointPath(myUnit->lastPosition,
                                                            myUnit->movingTo(),
                                                            myUnit->type,
                                                            PathFinding::PathFindingOptions::UseNearestBWEMArea);
            for (const auto &bwemChoke : chokePath)
            {
                if (choke->choke == bwemChoke) return true;
            }
        }

        return false;
    }
}

EarlyGameDefendMainBaseSquad::EarlyGameDefendMainBaseSquad()
        : Squad("Defend main base")
        , choke(nullptr)
        , workerDefenseSquad(std::make_unique<WorkerDefenseSquad>(Map::getMyMain()))
{
    initializeChoke();
}

bool EarlyGameDefendMainBaseSquad::canTransitionToAttack() const
{
    // Don't transition if any clusters are not attacking
    for (const auto &cluster : clusters)
    {
        if (cluster->currentActivity != UnitCluster::Activity::Attacking) return false;
    }

    // Get the vanguard cluster
    auto vanguard = vanguardCluster();
    if (!vanguard) return false;

    // Assert that the sim has recommended attacking for at least the past 72 frames
    if (vanguard->recentSimResults.size() < 72) return false;
    int count = 0;
    for (auto it = vanguard->recentSimResults.rbegin(); it != vanguard->recentSimResults.rend() && count < 72; it++)
    {
        if (!it->second) return false;
        count++;
    }

    return true;
}

void EarlyGameDefendMainBaseSquad::initializeChoke()
{
    auto base = Map::getMyMain();
    choke = Map::getMyMainChoke();

    if (choke)
    {
        targetPosition = choke->center;

        if (choke->isNarrowChoke)
        {
            auto grid = PathFinding::getNavigationGrid(base->getPosition());
            auto end1Node = grid ? &(*grid)[choke->end1Center] : nullptr;
            auto end2Node = grid ? &(*grid)[choke->end2Center] : nullptr;
            if (end1Node && end1Node->nextNode && end2Node && end2Node->nextNode)
            {
                chokeDefendEnd = end1Node->cost < end2Node->cost ? choke->end1Center : choke->end2Center;
            }
            else
            {
                auto end1Dist = PathFinding::GetGroundDistance(base->getPosition(),
                                                               choke->end1Center,
                                                               BWAPI::UnitTypes::Protoss_Dragoon,
                                                               PathFinding::PathFindingOptions::UseNearestBWEMArea);
                auto end2Dist = PathFinding::GetGroundDistance(base->getPosition(),
                                                               choke->end2Center,
                                                               BWAPI::UnitTypes::Protoss_Dragoon,
                                                               PathFinding::PathFindingOptions::UseNearestBWEMArea);
                chokeDefendEnd = end1Dist < end2Dist ? choke->end1Center : choke->end2Center;
            }
        }
    }
    else
    {
        targetPosition = base->mineralLineCenter;
    }
}

void EarlyGameDefendMainBaseSquad::execute()
{
    Squad::execute();

    if (clusters.empty())
    {
        std::set<Unit> enemyUnits;
        addEnemyUnits(enemyUnits, choke);

        auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);
        std::vector<std::pair<MyUnit, Unit>> emptyUnitsAndTargets;
        workerDefenseSquad->execute(workersAndTargets, emptyUnitsAndTargets);
    }
}

void EarlyGameDefendMainBaseSquad::execute(UnitCluster &cluster)
{
    if (Map::getMyMainChoke() != choke) initializeChoke();

    // Against terran, add units that are close to our cluster units
    // This handles cases where the enemy draws them out of the choke
    int furthestFromCenter = -1;
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
    {
        for (auto &unit : cluster.units)
        {
            furthestFromCenter = std::max(furthestFromCenter, unit->lastPosition.getApproxDistance(cluster.center));
        }
    }

    std::set<Unit> enemyUnits;
    bool enemyInOurBase = addEnemyUnits(enemyUnits, choke, cluster.center, furthestFromCenter);

    updateDetectionNeeds(enemyUnits);

    // Get worker targets
    // The returned list includes any worker that is being threatened by an enemy unit
    auto workersAndTargets = workerDefenseSquad->selectTargets(enemyUnits);

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Consider enemy units in a bit larger radius to the choke for the combat sim
    if (choke)
    {
        Units::enemyInRadius(enemyUnits, choke->center, 256, [](const Unit &unit)
        {
            if (!unit->type.isBuilding() && unit->lastSeen < (currentFrame - 48)) return false;
            if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (currentFrame - 120)) return false;
            if (!unit->isTransport() && !UnitUtil::CanAttackGround(unit->type)) return false;

            return true;
        });
    }

    // Run combat sim
    auto simResult = cluster.runCombatSim(targetPosition, unitsAndTargets, enemyUnits, detectors, false);

    // Make the attack / retreat decision based on the sim result
    // TODO: Needs tuning
    bool attack = simResult.myPercentLost() <= 0.001 ||
                  simResult.percentGain() > -0.1;

#if DEBUG_COMBATSIM_LOG
    CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                     << ": %l=" << simResult.myPercentLost()
                     << "; vg=" << simResult.valueGain()
                     << "; %g=" << simResult.percentGain()
                     << (attack ? "; ATTACK" : "; RETREAT");
#endif

    cluster.addSimResult(simResult, attack);

    // Make the final decision based on what state we are currently in

    // Currently regrouping, but want to attack: do so once the sim has stabilized
    if (attack && cluster.currentActivity == UnitCluster::Activity::Regrouping)
    {
        attack = shouldStartAttack(cluster, simResult);
    }

    // Currently attacking, but want to regroup: make sure regrouping is safe
    if (!attack && cluster.currentActivity == UnitCluster::Activity::Attacking)
    {
        attack = !shouldAbortAttack(cluster, simResult);
    }

    // If the enemy has cloaked units and we have a choke, always stay at the choke
    // Rationale: we want to plug the choke to keep the cloked units from getting in until we get detection
    if (!enemiesNeedingDetection.empty() && choke) attack = true;

    // Hold the choke against Zerg if we have enough zealots to block it
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && !attack && !enemyInOurBase && choke && choke->isNarrowChoke)
    {
        int zealotCount = 0;
        for (const auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.first->type == BWAPI::UnitTypes::Protoss_Zealot &&
                unitAndTarget.first->getDistance(choke->center) < 250)
            {
                zealotCount++;
            }
        }

        if (zealotCount > (choke->width / 30)) attack = true;
    }

    if (attack)
    {
        cluster.setActivity(UnitCluster::Activity::Attacking);

        // Reset our defensive position to the choke when all enemy units are out of our base
        if ((!enemiesNeedingDetection.empty() || !enemyInOurBase) && choke) targetPosition = choke->center;

        // Check if the enemy has static defense (e.g. cannon rush)
        bool hasStaticDefense = false;
        for (const auto &unit : enemyUnits)
        {
            if (UnitUtil::IsStationaryAttacker(unit->type))
            {
                hasStaticDefense = true;
                break;
            }
        }

        // Choose the type of micro depending on whether the enemy has static defense, we are holding a narrow choke, or neither
        if (hasStaticDefense)
        {
            cluster.containStatic(enemyUnits, targetPosition);
        }
        else if (enemiesNeedingDetection.empty() && !enemyInOurBase && choke && choke->isNarrowChoke)
        {
            if (enemyUnits.empty() && Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Dark_Templar).empty() && blockedFriendlyUnit(choke, unitToCluster))
            {
                cluster.attack(unitsAndTargets, Map::getMyMain()->mineralLineCenter);
            }
            else
            {
                cluster.holdChoke(choke, chokeDefendEnd, unitsAndTargets);
            }
        }
        else
        {
            cluster.attack(unitsAndTargets, targetPosition);
        }

        if (cluster.isVanguardCluster)
        {
            workerDefenseSquad->execute(workersAndTargets, unitsAndTargets);
        }

        return;
    }

    cluster.setActivity(UnitCluster::Activity::Regrouping);

    // Ensure the target position is set to the mineral line center
    // This allows our workers to help with the defense
    targetPosition = Map::getMyMain()->mineralLineCenter;

    // Micro our units according to the following flowchart:
    // - If there is a threatened worker, attack the unit threatening it.
    // - If we are at the mineral line center, attack it.
    // - Move to the mineral line center.

    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &unit = unitAndTarget.first;

        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

        // Set our target to the enemy attacking the closest threatened worker
        int closestThreatenedWorkerDist = INT_MAX;
        for (const auto &workerAndTarget : workersAndTargets)
        {
            int dist = unit->getDistance(workerAndTarget.first);
            if (dist < closestThreatenedWorkerDist)
            {
                unitAndTarget.second = workerAndTarget.second;
                closestThreatenedWorkerDist = dist;
            }
        }
        if (closestThreatenedWorkerDist < INT_MAX)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "Targeting worker threat: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->simPosition);
#endif
            unit->attackUnit(unitAndTarget.second, unitsAndTargets, false);
            continue;
        }

        // Attack the target if we are at the mineral line center and in the enemy's attack range
        if (unitAndTarget.second &&
            unit->getDistance(targetPosition) < 32 &&
            unit->isInEnemyWeaponRange(unitAndTarget.second, 32))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "Target: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->simPosition);
#endif
            unit->attackUnit(unitAndTarget.second, unitsAndTargets, false);
            continue;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unitAndTarget.first->id) << "Retreating: move to " << BWAPI::WalkPosition(targetPosition);
#endif
        unit->moveTo(targetPosition);
    }

    if (cluster.isVanguardCluster)
    {
        workerDefenseSquad->execute(workersAndTargets, unitsAndTargets);
    }
}

bool EarlyGameDefendMainBaseSquad::canAddUnitToCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster, int dist) const
{
    bool unitInMain = inMain(unit->lastPosition);
    bool clusterInMain = inMain(cluster->vanguard ? cluster->vanguard->lastPosition : cluster->center);
    if (unitInMain && clusterInMain) return true;
    if (unitInMain || clusterInMain) return false;
    return Squad::canAddUnitToCluster(unit, cluster, dist);
}

bool EarlyGameDefendMainBaseSquad::shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const
{
    bool firstInMain = inMain(first->vanguard ? first->vanguard->lastPosition : first->center);
    bool secondInMain = inMain(second->vanguard ? second->vanguard->lastPosition : second->center);
    if (firstInMain && secondInMain) return true;
    if (firstInMain || secondInMain) return false;
    return Squad::shouldCombineClusters(first, second);
}

bool EarlyGameDefendMainBaseSquad::shouldRemoveFromCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster) const
{
    if (inMain(unit->lastPosition)) return false;
    return Squad::shouldRemoveFromCluster(unit, cluster);
}
