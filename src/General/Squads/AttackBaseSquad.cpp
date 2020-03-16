#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

namespace
{
    bool shouldStartAttack(UnitCluster &cluster, CombatSimResult simResult)
    {
        // Don't start an attack until we have 48 frames of recommending attack with the same number of friendly units

        // First ensure the sim has recommended attack with the same number of friendly units for the past 48 frames
        int count = 0;
        for (auto it = cluster.recentSimResults.rbegin(); it != cluster.recentSimResults.rend() && count < 48; it++)
        {
            if (!it->second)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": aborting as the sim has recommended regrouping within the past 48 frames";
#endif
                return false;
            }

            if (simResult.myUnitCount != it->first.myUnitCount)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": aborting as the number of friendly units has changed within the past 48 frames";
#endif
                return false;
            }

            count++;
        }

        if (count < 48)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                             << ": aborting as we have fewer than 48 frames of sim data";
#endif
            return false;
        }

        // TODO: check that the sim result has been stable

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": starting attack as the sim has recommended doing so for the past 48 frames";
#endif
        return true;
    }

    bool shouldAbortAttack(UnitCluster &cluster, CombatSimResult simResult)
    {
        // TODO: Would probably be a good idea to run a retreat sim to see what the consequences of retreating are

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
        if (cluster.recentSimResults.size() < 24)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing as we have fewer than 24 frames of sim data";
#endif
            return false;
        }

        int count = 0;
        for (auto it = cluster.recentSimResults.rbegin(); it != cluster.recentSimResults.rend() && count < 24; it++)
        {
            if (it->second)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": continuing as a sim result within the past 24 frames recommended attacking";
#endif
                return false;
            }
            count++;
        }

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": aborting as the sim has recommended regrouping for the past 24 frames";
#endif
        return true;
    }
}

void AttackBaseSquad::execute(UnitCluster &cluster)
{
    // Look for enemies near this cluster
    std::set<Unit> enemyUnits;
    Units::enemyInRadius(enemyUnits, cluster.center, 640 + cluster.vanguard->getDistance(cluster.center));

    // If there are no enemies near the cluster, just move towards the target
    if (enemyUnits.empty())
    {
        cluster.setActivity(UnitCluster::Activity::Moving);
        cluster.move(targetPosition);
        return;
    }

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Run combat sim
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits);

    // If the sim result is nothing, and none of our units have a target, move instead of attacking
    if (simResult.myPercentLost() <= 0.001 && simResult.enemyPercentLost() <= 0.001)
    {
        bool hasTarget = false;
        for (const auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.second)
            {
                hasTarget = true;
                break;
            }
        }

        if (!hasTarget)
        {
            cluster.setActivity(UnitCluster::Activity::Moving);
            cluster.move(targetPosition);
            return;
        }
    }

    // TODO: If our units can't do any damage (e.g. ground-only vs. air, melee vs. kiting ranged units), do something else

    // For now, decide to attack if one of these holds:
    // - Attacking does not cost us anything
    // - We gain value without losing too much of our army
    // - We gain significant army proportion
    // - We have a huge advantage, so there's no reason to withdraw
    // TODO: Make this more dynamic, integrate more of the logic from old Locutus
    // TODO: Consider whether it is even more beneficial to wait for nearby reinforcements
    bool attack =
            simResult.myPercentLost() <= 0.001 ||
            (simResult.valueGain() > 0 && (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9)) ||
            simResult.percentGain() > 0.2;

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
        cluster.attack(unitsAndTargets, targetPosition);
        return;
    }

    // TODO: Run retreat sim?

    cluster.setActivity(UnitCluster::Activity::Regrouping);
    cluster.regroup(unitsAndTargets, enemyUnits, targetPosition);
}