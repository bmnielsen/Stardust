#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"

#include "DebugFlag_CombatSim.h"

#include <iomanip>

namespace
{
    bool shouldAttack(UnitCluster &cluster, const CombatSimResult &simResult, double aggression = 1.0)
    {
        // Compute the distance factor, an adjustment based on where our army is relative to our main and the target position
        // In open terrain, be more aggressive closer to our base and less aggressive closer to the target base
        // Across a choke, be less aggressive when either close to our base or close to the target base
        double distanceFactor = 1.0;
        if (!simResult.narrowChoke)
        {
            distanceFactor = 1.2 - 0.4 * cluster.percentageToEnemyMain;
        }
        else if (cluster.percentageToEnemyMain < 0.3 || cluster.percentageToEnemyMain > 0.7)
        {
            distanceFactor = 0.8;
        }

        auto attack = [&]()
        {
            // Always attack if we don't lose anything
            if (simResult.myPercentLost() <= 0.001) return true;

            // Attack if the enemy has undetected units that do not damage us much
            // This handles cases where e.g. our army is being attacked by a single cloaked wraith -
            // we want to ignore it
            if (simResult.enemyHasUndetectedUnits && simResult.myPercentLost() <= 0.15) return true;

            // Attack if the percentage gain, adjusted for aggression and distance factor, is acceptable
            // A percentage gain here means the enemy loses a larger percentage of their army than we do
            if (simResult.percentGain() > (0.2 / (aggression * distanceFactor))) return true;

            // Finally attack in cases where we think we will gain acceptable value, despite losing a higher percentage of our army
            if (aggression > 0.99 && simResult.valueGain() > (simResult.initialMine - simResult.finalMine) / 2 &&
                (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9))
            {
                return true;
            }

            return false;
        };

        bool result = attack();

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << std::setprecision(2) << "-" << aggression << "-" << distanceFactor
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << (result ? "; ATTACK" : "; RETREAT");
#endif

        return result;
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

    // This returns the number of consecutive frames the sim has agreed on its current value.
    // It also returns the total number of attack and regroup frames within the window.
    int consecutiveSimResults(std::deque<std::pair<CombatSimResult, bool>> &recentSimResults,
                              int *attack,
                              int *regroup,
                              int limit)
    {
        *attack = 0;
        *regroup = 0;

        if (recentSimResults.empty()) return 0;

        bool firstResult = recentSimResults.rbegin()->second;
        bool isConsecutive = true;
        int consecutive = 0;
        int count = 0;
        for (auto it = recentSimResults.rbegin(); it != recentSimResults.rend() && count < limit; it++)
        {
            if (isConsecutive)
            {
                if (it->second == firstResult)
                {
                    consecutive++;
                }
                else
                {
                    isConsecutive = false;
                }
            }

            if (it->second)
            {
                (*attack)++;
            }
            else
            {
                (*regroup)++;
            }

            count++;
        }

        return consecutive;
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

        // If the fight is now across a choke, abort the attack immediately
        // We probably want to try to hold the choke instead
        if (simResult.narrowChoke && !previousSimResult.narrowChoke)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting as the fight is now across a narrow choke";
#endif
            return false;
        }

        int attackFrames;
        int regroupFrames;
        int consecutiveRetreatFrames = consecutiveSimResults(cluster.recentSimResults, &attackFrames, &regroupFrames, 48);

        // Continue if the sim hasn't been stable for 6 frames
        if (consecutiveRetreatFrames < 6)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing attack as the sim has not yet been stable for 6 frames";
#endif
            return true;
        }

        // Continue if the sim has recommended attacking more than regrouping
        if (attackFrames > regroupFrames)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing attack; regroup=" << regroupFrames
                             << " vs. attack=" << attackFrames;
#endif
            return true;
        }

        // Abort
#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting attack; regroup=" << regroupFrames
                         << " vs. attack=" << attackFrames;
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

        int attackFrames;
        int regroupFrames;
        int consecutiveAttackFrames = consecutiveSimResults(cluster.recentSimResults, &attackFrames, &regroupFrames, 72);

        // Continue if the sim hasn't been stable for 12 frames
        if (consecutiveAttackFrames < 12)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing regroup as the sim has not yet been stable for 12 frames";
#endif
            return false;
        }

        // Continue if the sim has recommended regrouping more than attacking
        if (regroupFrames > attackFrames)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing regroup; regroup=" << regroupFrames
                             << " vs. attack=" << attackFrames;
#endif
            return false;
        }

        // Continue if our number of units has increased in the past 48 frames.
        // This gives our reinforcements time to link up with the rest of the cluster before engaging.
        int count = 0;
        for (auto it = cluster.recentSimResults.rbegin(); it != cluster.recentSimResults.rend() && count < 48; it++)
        {
            if (simResult.myUnitCount > it->first.myUnitCount)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": continuing regroup as more friendly units have joined the cluster in the past 48 frames";
#endif
                return false;
            }

            count++;
        }

        // Start the attack
#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": starting attack; regroup=" << regroupFrames
                         << " vs. attack=" << attackFrames;
#endif

        return true;
    }
}

void AttackBaseSquad::execute(UnitCluster &cluster)
{
    // Look for enemies near this cluster
    std::set<Unit> enemyUnits;
    int radius = 640;
    if (cluster.vanguard) radius += cluster.vanguard->getDistance(cluster.center);
    Units::enemyInRadius(enemyUnits, cluster.center, radius);

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
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, detectors);

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

    if (attack || ignoreCombatSim)
    {
        cluster.setActivity(UnitCluster::Activity::Attacking);
        cluster.attack(unitsAndTargets, targetPosition);
        return;
    }

    // Check if our cluster should try to link up with a closer cluster
    if (currentVanguardCluster && currentVanguardCluster->center != cluster.center)
    {
        // Link up if our vanguard unit is closer to the vanguard cluster than its current target
        bool linkUp = false;
        for (auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.first != cluster.vanguard) continue;

            if (!unitAndTarget.second || !unitAndTarget.second->lastPositionValid)
            {
                linkUp = true;
                break;
            }

            int distTarget = PathFinding::GetGroundDistance(
                    unitAndTarget.first->lastPosition,
                    unitAndTarget.second->lastPosition,
                    unitAndTarget.first->type);
            int distVanguardCluster = PathFinding::GetGroundDistance(unitAndTarget.first->lastPosition,
                                                                     currentVanguardCluster->center,
                                                                     unitAndTarget.first->type);
            if (distTarget != -1 && distVanguardCluster != -1 && distVanguardCluster < distTarget)
            {
                linkUp = true;
                break;
            }
        }

        if (linkUp)
        {
            cluster.setActivity(UnitCluster::Activity::Moving);
            cluster.move(currentVanguardCluster->center);
            return;
        }
    }

    // TODO: Run retreat sim?

    cluster.setActivity(UnitCluster::Activity::Regrouping);
    cluster.regroup(unitsAndTargets, enemyUnits, detectors, simResult, targetPosition);
}