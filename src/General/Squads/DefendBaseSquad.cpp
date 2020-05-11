#include "DefendBaseSquad.h"

#include "Units.h"
#include "Players.h"
#include "UnitUtil.h"

namespace
{
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
}

DefendBaseSquad::DefendBaseSquad(Base *base)
        : Squad((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
        , base(base)
        , choke(nullptr)
{
    if (base == Map::getMyMain())
    {
        choke = Map::getMyMainChoke();
    }
    else if (base == Map::getMyNatural())
    {
        // Find a single unblocked choke out of the natural that doesn't go to our main
        auto mainArea = Map::getMyMain()->getArea();
        for (auto bwemChoke : base->getArea()->ChokePoints())
        {
            if (bwemChoke->Blocked()) continue;
            if (bwemChoke->GetAreas().first == mainArea || bwemChoke->GetAreas().second == mainArea) continue;
            if (choke)
            {
                choke = nullptr;
                break;
            }
            choke = Map::choke(bwemChoke);
        }
    }

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

void DefendBaseSquad::execute(UnitCluster &cluster)
{
    std::set<Unit> enemyUnits;

    // Get enemy combat units in our base
    auto combatUnitSeenRecentlyPredicate = [](const Unit &unit)
    {
        return UnitUtil::IsCombatUnit(unit->type) && UnitUtil::CanAttackGround(unit->type)
               && unit->lastSeen > (BWAPI::Broodwar->getFrameCount() - 48);
    };
    Units::enemyInArea(enemyUnits, Map::getMyMain()->getArea(), combatUnitSeenRecentlyPredicate);

    bool enemyInOurBase = !enemyUnits.empty();

    // If there is a choke, get enemy combat units very close to the default target position
    if (choke)
    {
        Units::enemyInRadius(enemyUnits, choke->center, 64, combatUnitSeenRecentlyPredicate);
    }

    // If there are no enemy combat units, include enemy buildings to defend against gas steals or other cheese
    if (enemyUnits.empty())
    {
        Units::enemyInArea(enemyUnits, Map::getMyMain()->getArea(), [](const Unit &unit)
        {
            return unit->type.isBuilding() && !unit->isFlying;
        });

        enemyInOurBase = !enemyUnits.empty();
    }

    updateDetectionNeeds(enemyUnits);

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Consider enemy units in a bit larger radius to the choke for the combat sim
    if (choke)
    {
        Units::enemyInRadius(enemyUnits, choke->center, 192, combatUnitSeenRecentlyPredicate);
    }

    // Run combat sim
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, false);

    // TODO: Needs tuning
    bool attack =
            simResult.myPercentLost() <= 0.001 ||
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

    if (attack)
    {
        cluster.setActivity(UnitCluster::Activity::Attacking);

        // Reset our defensive position to the choke when all enemy units are out of our base
        if (!enemyInOurBase && choke) targetPosition = choke->center;

        // If defending at a narrow choke, use hold choke micro, otherwise just attack
        if (!enemyInOurBase && choke && choke->isNarrowChoke)
        {
            cluster.holdChoke(choke, chokeDefendEnd, unitsAndTargets);
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
    targetPosition = base->mineralLineCenter;

    // Move our units in the following way:
    // - If the unit is in the mineral line and close to its target, attack it
    // - If the unit's target is far out of its attack range, move towards it
    //   Rationale: we want to encourage our enemies to get distracted and chase us
    // - Otherwise move towards the mineral line center
    // TODO: Do something else against ranged units
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &unit = unitAndTarget.first;
        auto &target = unitAndTarget.second;

        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

        // If the unit has no target, just move to the target position
        if (!target)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "No target: move to " << BWAPI::WalkPosition(targetPosition);
#endif
            unit->moveTo(targetPosition);
            continue;
        }

        auto enemyPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames());

        // Attack the enemy if we are in the mineral line and in range of the enemy (or the enemy is in range of us)
        if (Map::isInOwnMineralLine(unit->tilePositionX, unit->tilePositionY) &&
            (unit->isInOurWeaponRange(target, enemyPosition) || unit->isInEnemyWeaponRange(target, enemyPosition)))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "Target: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->lastPosition);
#endif
            unit->attackUnit(target, unitsAndTargets, false);
            continue;
        }

        // Move towards the enemy if we are well out of their attack range
        int enemyRange = Players::weaponRange(target->player, target->getWeapon(unit));
        if (enemyPosition.isValid() && unit->getDistance(target, enemyPosition) > (enemyRange + 64))
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
