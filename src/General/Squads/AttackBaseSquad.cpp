#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

#include <iomanip>

namespace
{
    bool shouldAttack(UnitCluster &cluster, const CombatSimResult &simResult, double aggression = 1.0)
    {
        // We attack in the following cases:
        // - Attacking costs us nothing
        // - We gain a high percentage value, even if we lose absolute value
        //   This handles the case where our army is much larger than the enemy's
        // - We gain some value without losing an unacceptably-large proportion of our army
        //   This criterion does not apply if our aggression value is low
        bool attack =
                (simResult.myPercentLost() <= 0.001) ||
                (simResult.percentGain() > (0.2 / aggression)) ||
                (aggression > 0.99 && simResult.valueGain() > 0 && (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9));

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << std::setprecision(2) << "-" << aggression
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << (attack ? "; ATTACK" : "; RETREAT");
#endif

        return attack;
    }

    bool shouldStartAttack(UnitCluster &cluster, CombatSimResult &simResult)
    {
        // Always attack if we are maxed
        if (BWAPI::Broodwar->self()->supplyUsed() > 380)
        {
            return true;
        }

        bool attack = shouldAttack(cluster, simResult);
        cluster.addSimResult(simResult, attack);
        return attack;
    }

    bool shouldContinueAttack(UnitCluster &cluster, CombatSimResult &simResult)
    {
        // Always continue if we are close to maxed
        if (BWAPI::Broodwar->self()->supplyUsed() > 300)
        {
            return true;
        }

        bool attack = shouldAttack(cluster, simResult, 1.2);

        cluster.addSimResult(simResult, attack);

        if (attack) return true;

        // TODO: Would probably be a good idea to run a retreat sim to see what the consequences of retreating are

        // If this is the first run of the combat sim for this fight, always abort immediately
        if (cluster.recentSimResults.size() < 2)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting as this is the first sim result";
#endif
            return false;
        }

        CombatSimResult previousSimResult = cluster.recentSimResults.rbegin()[1].first;

        // If the enemy army strength has increased significantly, abort the attack immediately
        if (simResult.initialEnemy > (int) ((double) previousSimResult.initialEnemy * 1.2))
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting as there are now more enemy units";
#endif
            return false;
        }

        // Otherwise only abort the attack when the sim has been stable for a number of frames
        if (cluster.recentSimResults.size() < 24)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing as we have fewer than 24 frames of sim data";
#endif
            return true;
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
                return true;
            }
            count++;
        }

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": aborting as the sim has recommended regrouping for the past 24 frames";
#endif
        return false;
    }

    bool shouldStopRegrouping(UnitCluster &cluster, CombatSimResult &simResult)
    {
        // Always attack if we are maxed
        if (BWAPI::Broodwar->self()->supplyUsed() > 380)
        {
            return true;
        }

        // Be more conservative if the battle is across a choke
        double aggression = 0.8;
        if (simResult.narrowChoke)
        {
            // Scale based on width: 128 pixels wide gives no reduction, 48 or lower gives 0.2 reduction
            aggression -= std::max(0.0, 0.2 * std::min(1.0, (128.0 - (double) simResult.narrowChoke->width) / 80.0));

            // Scale based on length: 0 pixels long gives no reduction, 128 or higher gives 0.35 reduction
            aggression -= std::max(0.0, 0.35 * std::min(1.0, ((double) simResult.narrowChoke->length) / 128.0));
        }
        bool attack = shouldAttack(cluster, simResult, aggression);

        cluster.addSimResult(simResult, attack);

        if (!attack) return false;

        // Stop the retreat when we have 48 frames of recommending attack with the same number of friendly units
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

    updateDetectionNeeds(enemyUnits);

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


    // Make the final decision based on what state we are currently in

    bool attack;
    switch (cluster.currentActivity)
    {
        case UnitCluster::Activity::Moving:
            attack = shouldStartAttack(cluster, simResult);
            break;
        case UnitCluster::Activity::Attacking:
            attack = shouldContinueAttack(cluster, simResult);
            break;
        case UnitCluster::Activity::Regrouping:
            attack = shouldStopRegrouping(cluster, simResult);
            break;
    }

    if (attack)
    {
        cluster.setActivity(UnitCluster::Activity::Attacking);
        cluster.attack(unitsAndTargets, targetPosition);
        return;
    }

    // TODO: Run retreat sim?

    cluster.setActivity(UnitCluster::Activity::Regrouping);
    cluster.regroup(unitsAndTargets, enemyUnits, simResult, targetPosition);
}