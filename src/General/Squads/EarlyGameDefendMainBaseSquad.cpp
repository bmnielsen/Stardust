#include "EarlyGameDefendMainBaseSquad.h"

#include "Units.h"
#include "Map.h"
#include "Players.h"
#include "UnitUtil.h"

namespace
{
    bool inMain(BWAPI::Position pos)
    {
        auto choke = Map::getMyMainChoke();
        if (choke && choke->center.getApproxDistance(pos) < 320) return true;

        auto mainAreas = Map::getMyMainAreas();
        return mainAreas.find(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(pos))) != mainAreas.end();
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

        CombatSimResult previousSimResult = cluster.recentSimResults.rbegin()[1].first;

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
    else
    {
        targetPosition = base->mineralLineCenter;
    }
}

void EarlyGameDefendMainBaseSquad::execute(UnitCluster &cluster)
{
    if (Map::getMyMainChoke() != choke) initializeChoke();

    std::set<Unit> enemyUnits;

    // Get enemy combat units in our base
    auto combatUnitSeenRecentlyPredicate = [](const Unit &unit)
    {
        if (!unit->type.isBuilding() && unit->lastSeen < (BWAPI::Broodwar->getFrameCount() - 48)) return false;
        if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (BWAPI::Broodwar->getFrameCount() - 120)) return false;
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

    updateDetectionNeeds(enemyUnits);

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Consider enemy units in a bit larger radius to the choke for the combat sim
    if (choke)
    {
        Units::enemyInRadius(enemyUnits, choke->center, 256, combatUnitSeenRecentlyPredicate);
    }

    // Run combat sim
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, detectors, false);

    // Make the attack / retreat decision based on the sim result
    // TODO: Needs tuning
    bool attack = simResult.myPercentLost() <= 0.001 ||
                  simResult.percentGain() > -0.1;

#if DEBUG_COMBATSIM
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

    if (attack || (!enemiesNeedingDetection.empty() && choke) || (!enemyInOurBase && choke && choke->isNarrowChoke))
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
            cluster.containBase(unitsAndTargets, enemyUnits, targetPosition);
        }
        else if (enemiesNeedingDetection.empty() && !enemyInOurBase && choke && choke->isNarrowChoke)
        {
            if (enemyUnits.empty() && blockedFriendlyUnit(choke, unitToCluster))
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

        return;
    }

    cluster.setActivity(UnitCluster::Activity::Regrouping);

    // Ensure the target position is set to the mineral line center
    // This allows our workers to help with the defense
    targetPosition = Map::getMyMain()->mineralLineCenter;

    // Move our units in the following way:
    // - If the unit is in the mineral line and close to its target, attack it
    // - If the unit's target is far out of its attack range, move towards it
    //   Rationale: we want to encourage our enemies to get distracted and chase us
    // - Otherwise move towards the mineral line center
    // TODO: Do something else against ranged units

    // First scan to see if any unit is attacking, since if any single unit attacks, all units should attack
    bool anyAttacking = false;
    std::vector<std::pair<MyUnit, Unit>> filteredUnitsAndTargets;
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &unit = unitAndTarget.first;
        auto &target = unitAndTarget.second;

        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

        filteredUnitsAndTargets.push_back(unitAndTarget);

        if (!target) continue;

        // Attack if we are in the mineral line and in range of the enemy (or the enemy is in range of us)
        auto enemyPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames());
        if (Map::isInOwnMineralLine(unit->tilePositionX, unit->tilePositionY) &&
            (unit->isInOurWeaponRange(target, enemyPosition) || unit->isInEnemyWeaponRange(target, enemyPosition)))
        {
            anyAttacking = true;
        }
    }

    // Now execute the orders
    for (auto &unitAndTarget : filteredUnitsAndTargets)
    {
        auto &unit = unitAndTarget.first;
        auto &target = unitAndTarget.second;

        // If the unit has no target, just move to the target position
        if (!target)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "No target: move to " << BWAPI::WalkPosition(targetPosition);
#endif
            unit->moveTo(targetPosition);
            continue;
        }

        // Attack if the squad is attacking
        if (target && anyAttacking)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "Target: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->lastPosition);
#endif
            unit->attackUnit(target, unitsAndTargets, false);
            continue;
        }

        auto enemyPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames());

        // Move towards the enemy if we are well out of their attack range
        if (enemyPosition.isValid() && unit->getDistance(target, enemyPosition) > (target->range(unit) + 64))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "Retreating: stay close to enemy @ " << BWAPI::WalkPosition(enemyPosition);
#endif
            unit->moveTo(enemyPosition);
            continue;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unitAndTarget.first->id) << "Retreating: move to " << BWAPI::WalkPosition(targetPosition);
#endif
        unit->moveTo(targetPosition);
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
