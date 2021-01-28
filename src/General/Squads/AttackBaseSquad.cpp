#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"

#include "DebugFlag_CombatSim.h"

#include <iomanip>

namespace
{
    bool shouldAttack(UnitCluster &cluster, const CombatSimResult &simResult, double aggression = 1.0)
    {
        double distanceFactor = 1.0;

        auto attack = [&]()
        {
            // Always attack if we don't lose anything
            if (simResult.myPercentLost() <= 0.001) return true;

            // Special cases when the enemy has undetected units
            if (simResult.enemyHasUndetectedUnits)
            {
                // Always retreat if we are close to the target and can't hurt anything
                // This usually means there are no detected units we can attack
                if (cluster.vanguardDistToTarget < 500 && simResult.valueGain() <= 0)
                {
                    return false;
                }

                // Always attack if we are further from the target and aren't losing a large percentage of our army
                // This handles cases where our army would otherwise get pinned in its main by e.g. a single cloaked wraith
                if (cluster.vanguardDistToTarget >= 500 && simResult.myPercentLost() <= 0.15)
                {
                    return true;
                }
            }

            // Attack in cases where we think we will kill 50% more value than we lose
            if (aggression > 0.99 && simResult.valueGain() > (simResult.initialMine - simResult.finalMine) / 2 &&
                (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9))
            {
                return true;
            }

            // Compute the distance factor, an adjustment based on where our army is relative to our main and the target position
            distanceFactor = 1.2 - 0.4 * cluster.percentageToEnemyMain;

            // Give an extra penalty to narrow chokes close to the enemy base
            if (simResult.narrowChoke && cluster.percentageToEnemyMain > 0.7)
            {
                distanceFactor *= 0.8;
            }

            double percentGainCutoff = 0.2 / (aggression * distanceFactor);

            // Attack if we expect to end the fight with a sufficiently larger army and aren't losing an unacceptable percentage of it
            if (simResult.myPercentageOfTotal() > (1.0 - 0.3 * (distanceFactor - 0.2) * aggression)
                && simResult.percentGain() > (percentGainCutoff - 0.15))
            {
                return true;
            }

            // Attack if the percentage gain, adjusted for aggression and distance factor, is acceptable
            // A percentage gain here means the enemy loses a larger percentage of their army than we do
            if (simResult.percentGain() > percentGainCutoff) return true;

            return false;
        };

        bool result = attack();

#if DEBUG_COMBATSIM_LOG
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << std::setprecision(2) << "-" << aggression << "-" << distanceFactor
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << "; %t=" << simResult.myPercentageOfTotal()
                         << (result ? "; ATTACK" : "; RETREAT");
#endif

        return result;
    }

    bool shouldStartAttack(UnitCluster &cluster, CombatSimResult &simResult)
    {
        bool attack = shouldAttack(cluster, simResult);
        cluster.addSimResult(simResult, attack);
        return attack;
    }

    bool shouldContinueAttack(UnitCluster &cluster, CombatSimResult &simResult)
    {
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
        int consecutiveRetreatFrames = UnitCluster::consecutiveSimResults(cluster.recentSimResults, &attackFrames, &regroupFrames, 48);

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
        int consecutiveAttackFrames = UnitCluster::consecutiveSimResults(cluster.recentSimResults, &attackFrames, &regroupFrames, 72);

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

    // Scan the targets to see if any of our units have a valid target that has been seen recently
    bool hasValidTarget = false;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        if (!unitAndTarget.second) continue;

        // A stationary attacker is valid if we are close to their attack range
        if (unitAndTarget.second->lastPositionValid && UnitUtil::IsStationaryAttacker(unitAndTarget.second->type)
            && unitAndTarget.first->isInEnemyWeaponRange(unitAndTarget.second, 96))
        {
            hasValidTarget = true;
            break;
        }

        // Other targets are valid if they have been seen in the past 5 seconds
        if (unitAndTarget.second->lastSeen > (BWAPI::Broodwar->getFrameCount() - 120))
        {
            hasValidTarget = true;
            break;
        }
    }

    // Run combat sim
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, detectors);

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
        // Move instead if none of our units have a valid target
        if (!hasValidTarget)
        {
            cluster.setActivity(UnitCluster::Activity::Moving);
            cluster.move(targetPosition);
            return;
        }

        cluster.setActivity(UnitCluster::Activity::Attacking);
        cluster.attack(unitsAndTargets, targetPosition);
        return;
    }

    // Check if our cluster should try to link up with a closer cluster
    if (currentVanguardCluster && currentVanguardCluster->vanguard && currentVanguardCluster->center != cluster.center)
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

            int distTarget = unitAndTarget.first->isFlying
                             ? unitAndTarget.first->getDistance(unitAndTarget.second->lastPosition)
                             : PathFinding::GetGroundDistance(
                            unitAndTarget.first->lastPosition,
                            unitAndTarget.second->lastPosition,
                            unitAndTarget.first->type,
                            PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
            int distVanguardCluster = unitAndTarget.first->isFlying
                                      ? unitAndTarget.first->getDistance(currentVanguardCluster->vanguard->lastPosition)
                                      : PathFinding::GetGroundDistance(unitAndTarget.first->lastPosition,
                                                                       currentVanguardCluster->vanguard->lastPosition,
                                                                       unitAndTarget.first->type,
                                                                       PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
            linkUp = (distTarget == -1 || distVanguardCluster == -1 || distVanguardCluster < distTarget);

            break;
        }

        if (linkUp)
        {
            cluster.setActivity(UnitCluster::Activity::Moving);
            cluster.move(currentVanguardCluster->vanguard->lastPosition);
            return;
        }
    }

    // TODO: Run retreat sim?

    cluster.setActivity(UnitCluster::Activity::Regrouping);
    cluster.regroup(unitsAndTargets, enemyUnits, detectors, simResult, targetPosition, hasValidTarget);
}