#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

#include "DebugFlag_CombatSim.h"

#include <iomanip>

namespace
{
    bool pushedBackToNatural(UnitCluster &cluster, int threshold, int *pDist = nullptr)
    {
        if (!cluster.isVanguardCluster) return false;

        auto natural = Map::getMyNatural();
        if (!natural || natural->owner != BWAPI::Broodwar->self() || !natural->resourceDepot || !natural->resourceDepot->completed) return false;

        auto dist = cluster.center.getApproxDistance(natural->getPosition());
        if (pDist) *pDist = dist;
        return dist < threshold;
    }

    double reinforcementFactor(UnitCluster &cluster, double closestReinforcements, double reinforcementPercentage)
    {
        // Only adjust for reinforcements when our army is out on the field
        // TODO: Would also be nice to adjust for units in production when waiting to break out from our main
        if (cluster.percentageToEnemyMain < 0.2) return 1.0;

#if DEBUG_COMBATSIM_LOG
        CherryVis::log() << std::setprecision(2) << BWAPI::WalkPosition(cluster.center)
                         << ": cluster@" << cluster.percentageToEnemyMain
                         << "; reinforcements@" << closestReinforcements
                         << "; reinforcement%=" << reinforcementPercentage
                         << "; dist=" << (1.0 - ((closestReinforcements / cluster.percentageToEnemyMain) / 2))
                         << "; %=" << (1.0 - reinforcementPercentage)
                         << "; adjustedDist=" << (1.0 - std::min(1.0, reinforcementPercentage / 0.2) *
                                                        ((closestReinforcements / cluster.percentageToEnemyMain) / 2));
#endif

        // For distance, scale the effect from 0 (far away) to 0.5 (close)
        double distanceFactor = (closestReinforcements / cluster.percentageToEnemyMain) / 2;

        // Then scale it lower if the number of reinforcements is low
        distanceFactor *= std::min(1.0, reinforcementPercentage / 0.2);

        // Combine with the reinforcement percentage
        return (1.0 - distanceFactor) * (1.0 - reinforcementPercentage);
    }

    bool shouldAttack(UnitCluster &cluster, CombatSimResult &simResult, double aggression = 1.0)
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

            // Attack if we expect to end the fight with a sufficiently larger army and aren't losing an unacceptable percentage of it
            double percentOfTotal = simResult.myPercentageOfTotal() - (0.9 - 0.35 * distanceFactor * aggression);
            if (percentOfTotal > 0.0
                && simResult.percentGain() > (-0.05 * distanceFactor * aggression - percentOfTotal))
            {
                return true;
            }

            // Attack if the percentage gain, adjusted for aggression and distance factor, is acceptable
            // A percentage gain here means the enemy loses a larger percentage of their army than we do
            if (simResult.percentGain() > (0.2 / (aggression * distanceFactor))) return true;

            return false;
        };

        bool result = attack();

        simResult.distanceFactor = distanceFactor;
        simResult.aggression = aggression;

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

    bool shouldStartAttack(UnitCluster &cluster,
                           CombatSimResult &simResult,
                           double closestReinforcements,
                           double reinforcementPercentage)
    {
        double aggression = reinforcementFactor(cluster, closestReinforcements, reinforcementPercentage);

        // Increase aggression if we are trying to fight our way out of our natural
        int naturalDist;
        if (pushedBackToNatural(cluster, 800, &naturalDist))
        {
            // Scales linearly from 1.0 at 800 distance to 2.0 at 320 distance
            aggression *= 1.0 + (800.0 - std::max((double)naturalDist, 320.0)) / 480.0;
        }

        bool attack = shouldAttack(cluster, simResult, aggression);
        cluster.addSimResult(simResult, attack);
        return attack;
    }

    bool shouldContinueAttack(UnitCluster &cluster,
                              CombatSimResult &simResult,
                              double closestReinforcements,
                              double reinforcementPercentage)
    {
        double aggression = 1.2;

        // Increase aggression if we are close to maxed and have no significant reinforcements incoming
        if (BWAPI::Broodwar->self()->supplyUsed() > 300 && reinforcementPercentage < 0.15)
        {
            aggression += 0.005 * (double)(BWAPI::Broodwar->self()->supplyUsed() - 300);
        }

        // Increase aggression if we are trying to fight our way out of our natural
        int naturalDist;
        if (pushedBackToNatural(cluster, 800, &naturalDist))
        {
            // Scales linearly from 1.0 at 800 distance to 2.0 at 320 distance
            aggression *= 1.0 + (800.0 - std::max((double)naturalDist, 320.0)) / 480.0;
        }

        bool attack = shouldAttack(cluster, simResult, aggression);

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
#if DEBUG_COMBATSIM_LOG
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing attack as the sim has not yet been stable for 6 frames";
#endif
            return true;
        }

        // Continue if the sim has recommended attacking more than regrouping
        if (attackFrames > regroupFrames)
        {
#if DEBUG_COMBATSIM_LOG
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

    bool shouldStopRegrouping(UnitCluster &cluster,
                              CombatSimResult &simResult,
                              double closestReinforcements,
                              double reinforcementPercentage)
    {
        // Always attack if we are maxed and have no significant reinforcements incoming
        if (BWAPI::Broodwar->self()->supplyUsed() > 380 && reinforcementPercentage < 0.075)
        {
            cluster.addSimResult(simResult, true);
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

        // Adjust the aggression for reinforcements
        aggression *= reinforcementFactor(cluster, closestReinforcements, reinforcementPercentage);

        // Adjust the aggression if we have been pushed back into our natural
        if (pushedBackToNatural(cluster, 320))
        {
            aggression *= 2.0;
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
#if DEBUG_COMBATSIM_LOG
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing regroup as the sim has not yet been stable for 12 frames";
#endif
            return false;
        }

        // Continue if the sim has recommended regrouping more than attacking
        if (regroupFrames > attackFrames)
        {
#if DEBUG_COMBATSIM_LOG
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing regroup; regroup=" << regroupFrames
                             << " vs. attack=" << attackFrames;
#endif
            return false;
        }

        // Continue if our number of units has increased in the past 72 frames.
        // This gives our reinforcements time to link up with the rest of the cluster before engaging.
        int count = 0;
        for (auto it = cluster.recentSimResults.rbegin(); it != cluster.recentSimResults.rend() && count < 72; it++)
        {
            if (simResult.myUnitCount > it->first.myUnitCount)
            {
#if DEBUG_COMBATSIM_LOG
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": continuing regroup as more friendly units have joined the cluster in the past 72 frames";
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
    auto simResult = cluster.runCombatSim(targetPosition, unitsAndTargets, enemyUnits, detectors);

    // TODO: If our units can't do any damage (e.g. ground-only vs. air, melee vs. kiting ranged units), do something else

    // Find the closest reinforcements to our army
    // Here reinforcements are defined as a cluster further away from the target position that is moving
    double closestReinforcements = 0.0;
    int totalReinforcements = 0;
    for (auto &other : clusters)
    {
        if (other->center == cluster.center) continue;
        if (other->percentageToEnemyMain > cluster.percentageToEnemyMain) continue;
        if (other->currentActivity != UnitCluster::Activity::Moving) continue;

        totalReinforcements += other->units.size();

        if (other->percentageToEnemyMain > closestReinforcements)
        {
            closestReinforcements = other->percentageToEnemyMain;
        }
    }
    double reinforcementPercentage = (double) totalReinforcements / (double) (cluster.units.size() + totalReinforcements);

    simResult.closestReinforcements = closestReinforcements;
    simResult.reinforcementPercentage = reinforcementPercentage;

    // Make the final decision based on what state we are currently in

    bool attack;
    switch (cluster.currentActivity)
    {
        case UnitCluster::Activity::Moving:
            attack = shouldStartAttack(cluster, simResult, closestReinforcements, reinforcementPercentage);
            break;
        case UnitCluster::Activity::Attacking:
            attack = shouldContinueAttack(cluster, simResult, closestReinforcements, reinforcementPercentage);
            break;
        case UnitCluster::Activity::Regrouping:
            attack = shouldStopRegrouping(cluster, simResult, closestReinforcements, reinforcementPercentage);
            break;
    }

    if (attack || ignoreCombatSim)
    {
        cluster.setActivity(UnitCluster::Activity::Attacking);

        // Move instead if none of our units have a valid target
        if (!hasValidTarget)
        {
            cluster.move(targetPosition);
            return;
        }

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