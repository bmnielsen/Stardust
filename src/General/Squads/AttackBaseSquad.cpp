#include "AttackBaseSquad.h"

#include "Config.h"
#include "FileTools.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

#include "DebugFlag_CombatSim.h"

#include <iomanip>
#include <sstream>

namespace
{
    /* The enemy storm line is online once a Templar Archives or High Templar has
     * been sighted (timings only clear on confirmed destruction). batch22
     * cross-check: PW built Archives in 4/5 games at ~14-16k frames but we
     * sighted none (no mid-game scouting) - HTs walking with the army are the
     * reliable secondary signal. */
    bool enemyStormOnline()
    {
        return !UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Templar_Archives).empty()
               || !UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_High_Templar).empty();
    }

    /* Direction "storm window" (2026-09-30, official 341-game data): FFE wins
     * 97% of games decided before the enemy storm line forms and 29% after.
     * While fresh scouting shows the enemy has no Templar Archives the squad is
     * more willing to force a fight (cash in the economic lead); once the
     * Archives is sighted it backs off. Stale intel (enemy main not seen
     * recently) gets no modulation - "Archives not seen" is not "not built".
     * storm_guard (2026-10-08 funnel plan) deepens the CLOSED-side penalty. */
    double stormWindowAggressionFactor()
    {
        if (!Config::stormWindowEnabled() && !Config::stormGuardEnabled()) return 1.0;

        if (enemyStormOnline())
        {
            static bool loggedClosed = false;
            if (!loggedClosed)
            {
                loggedClosed = true;
                FileTools::appendLine("shushanking_htpipe",
                                      "HTPIPE SWIN frame=" + std::to_string(frameNow) + " state=CLOSED(storm online)");
            }
            return Config::stormGuardEnabled() ? Config::stormGuardPenalty() : 0.8;
        }

        if (!Config::stormWindowEnabled()) return 1.0;

        if (frameNow < Config::stormWindowMinFrame()) return 1.0;

        /* Intel freshness: only trust "no Archives" while one of the enemy's
         * resource depots was sighted recently */
        auto depotType = BWAPI::Broodwar->enemy()->getRace().getResourceDepot();
        for (const auto &depot : UnitMgr::allEnemyOfType(depotType))
        {
            if (depot->lastSeen >= frameNow - 2000)
            {
                static bool loggedOpen = false;
                if (!loggedOpen)
                {
                    loggedOpen = true;
                    FileTools::appendLine("shushanking_htpipe",
                                          "HTPIPE SWIN frame=" + std::to_string(frameNow) + " state=OPEN(boost)");
                }
                return 1.3;
            }
        }

        static bool loggedStale = false;
        if (!loggedStale)
        {
            loggedStale = true;
            FileTools::appendLine("shushanking_htpipe",
                                  "HTPIPE SWIN frame=" + std::to_string(frameNow) + " state=STALE(no modulation)");
        }
        return 1.0;
    }

    bool pushedBackToNatural(UnitCluster &clump, int threshold, int *pDist = nullptr)
    {
        if (!clump.isVanguardCluster) return false;
        if (Map::mapSpecificOverride()->hasBackdoorNatural()) return false;

        auto natBase = Map::getMyNatural();
        if (!natBase || natBase->owner != BWAPI::Broodwar->self() || !natBase->resourceDepot || !natBase->resourceDepot->completed) return false;

        auto span = clump.midpoint.getApproxDistance(natBase->getPosition());
        if (pDist) *pDist = span;
        return span < threshold;
    }

    double reinforcementFactor(UnitCluster &clump, double closestReinforcements, double reinforcementPercentage)
    {
        /* Only adjust for reinforcements when our army is out on the field */
        /* TODO: Would also be nice to adjust for units in production when waiting to break out from our main */
        if (clump.percentageToEnemyMain < 0.2) return 1.0;

#if DEBUG_COMBATSIM_LOG
        Telemetry::log() << std::setprecision(2) << BWAPI::WalkPosition(clump.midpoint)
                         << ": cluster@" << clump.percentageToEnemyMain
                         << "; reinforcements@" << closestReinforcements
                         << "; reinforcement%=" << reinforcementPercentage
                         << "; dist=" << (1.0 - ((closestReinforcements / clump.percentageToEnemyMain) / 2))
                         << "; %=" << (1.0 - reinforcementPercentage)
                         << "; adjustedDist=" << (1.0 - std::min(1.0, reinforcementPercentage / 0.2) *
                                                        ((closestReinforcements / clump.percentageToEnemyMain) / 2));
#endif

        /* For distance, scale the effect from 0 (far away) to 0.5 (close) */
        double distanceFactor = (closestReinforcements / clump.percentageToEnemyMain) / 2;

        /* Then scale it lower if the number of reinforcements is low */
        distanceFactor *= std::min(1.0, reinforcementPercentage / 0.2);

        /* Combine with the reinforcement percentage */
        return (1.0 - distanceFactor) * (1.0 - reinforcementPercentage);
    }

    bool shouldAttack(UnitCluster &clump, CombatSimResult &simResult, double aggression = 1.0)
    {
        double distanceFactor = 1.0;

        auto attack = [&]()
        {
            /* Always attack if we don't lose anything */
            if (simResult.myPercentLost() <= 0.001) return true;

            /* Special cases when the enemy has undetected units */
            if (simResult.enemyHasUndetectedUnits)
            {
                /* Always retreat if we are close to the target and can't hurt anything */
                /* This usually means there are no detected units we can attack */
                if (clump.vanguardDistToTarget < 500 && simResult.valueGain() <= 0)
                {
                    return false;
                }

                /* Always attack if we are further from the target and aren't losing a large percentage of our army */
                /* This handles cases where our army would otherwise get pinned in its main by e.g. a single cloaked wraith */
                if (clump.vanguardDistToTarget >= 500 && simResult.myPercentLost() <= 0.15)
                {
                    return true;
                }
            }

            /* Attack in cases where we think we will kill 50% more value than we lose */
            if (aggression > 0.99 && simResult.valueGain() > (simResult.initialMine - simResult.finalMine) / 2 &&
                (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9))
            {
                return true;
            }

            /* Compute the distance factor, an adjustment based on where our army is relative to our main and the target position */
            distanceFactor = 1.2 - 0.4 * clump.percentageToEnemyMain;

            /* Give an extra penalty to narrow chokes close to the enemy base */
            if (simResult.narrowChoke && clump.percentageToEnemyMain > 0.7)
            {
                distanceFactor *= 0.8;
            }

            /* Attack if we expect to end the fight with a sufficiently larger army and aren't losing an unacceptable percentage of it */
            double percentOfTotal = simResult.myPercentageOfTotal() - (0.9 - 0.35 * distanceFactor * aggression);
            if (percentOfTotal > 0.0
                && simResult.percentGain() > (-0.05 * distanceFactor * aggression - percentOfTotal))
            {
                return true;
            }

            /* Attack if the percentage gain, adjusted for aggression and distance factor, is acceptable */
            /* A percentage gain here means the enemy loses a larger percentage of their army than we do */
            if (simResult.percentGain() > (0.2 / (aggression * distanceFactor))) return true;

            return false;
        };

        bool outcome = attack();

        simResult.distanceFactor = distanceFactor;
        simResult.aggression = aggression;

#if DEBUG_COMBATSIM_LOG
        Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                         << std::setprecision(2) << "-" << aggression << "-" << distanceFactor
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << "; %t=" << simResult.myPercentageOfTotal()
                         << (outcome ? "; ATTACK" : "; RETREAT");
#endif

        return outcome;
    }

    bool shouldStartAttack(UnitCluster &clump,
                           CombatSimResult &simResult,
                           double closestReinforcements,
                           double reinforcementPercentage)
    {
        double aggression = reinforcementFactor(clump, closestReinforcements, reinforcementPercentage);

        /* Increase aggression if we are trying to fight our way out of our natural */
        int naturalDist;
        if (pushedBackToNatural(clump, 800, &naturalDist))
        {
            /* Scales linearly from 1.0 at 1000 distance to 2.0 at <500 distance */
            aggression *= 1.0 + (1000.0 - std::max((double)naturalDist, 500.0)) / 500.0;
        }

        /* Direction "storm window" (2026-09-30): more willing to start fights
         * while the enemy storm line is not formed, less once it is */
        aggression *= stormWindowAggressionFactor();

        /* Engagement-discipline hysteresis gate (BB audit 2026-09-18): never open
         * a fight we enter with less than attack_ratio of the enemy's value.
         * Disabled by default; preserves the 6-frame stability window below. */
        if (Config::attackRatioGateEnabled()
            && simResult.initialEnemy > 0
            && (double)simResult.initialMine < Config::attackRatioAttack() * (double)simResult.initialEnemy)
        {
            clump.addSimResult(simResult, false);
            return false;
        }

        /* Direction C (2026-09-25 forensics Q1): FAP exchange-ratio gate. Battle
         * forensics showed SD loss games self-feed battles (3x count, higher own
         * density). Refuse opening a fight the sim predicts as a net percentage
         * loss below the configured gain. The hysteresis window below still
         * arbitrates borderline cases. */
        if (Config::exchangeGateEnabled()
            && simResult.percentGain() < Config::exchangeGateMinGain())
        {
            clump.addSimResult(simResult, false);
            return false;
        }

        bool attack = shouldAttack(clump, simResult, aggression);
        clump.addSimResult(simResult, attack);
        return attack;
    }

    bool shouldContinueAttack(UnitCluster &clump,
                              CombatSimResult &simResult,
                              double closestReinforcements,
                              double reinforcementPercentage)
    {
        double aggression = 1.2;

        /* Increase aggression if we are close to maxed and have no significant reinforcements incoming */
        if (BWAPI::Broodwar->self()->supplyUsed() > 300 && reinforcementPercentage < 0.15)
        {
            aggression += 0.005 * (double)(BWAPI::Broodwar->self()->supplyUsed() - 300);
        }

        /* Increase aggression if we are trying to fight our way out of our natural */
        int naturalDist;
        if (pushedBackToNatural(clump, 800, &naturalDist))
        {
            /* Scales linearly from 1.0 at 1000 distance to 2.0 at <500 distance */
            aggression *= 1.0 + (1000.0 - std::max((double)naturalDist, 500.0)) / 500.0;
        }

        /* Direction "storm window" (2026-09-30): keep pressing while the enemy
         * storm line is not formed, disengage more readily once it is */
        aggression *= stormWindowAggressionFactor();

        /* Engagement-discipline hysteresis gate, retreat side (BB audit 2026-09-18):
         * disengage a continuing fight we entered with <= retreat_ratio of the
         * enemy's value. Disabled by default. */
        if (Config::attackRatioGateEnabled()
            && simResult.initialEnemy > 0
            && (double)simResult.initialMine <= Config::attackRatioRetreat() * (double)simResult.initialEnemy)
        {
            clump.addSimResult(simResult, false);
            return false;
        }

        /* Direction C retreat side: disengage when the continuing fight's
         * predicted exchange has gone clearly negative (below minus the gain
         * threshold - a symmetric band around zero keeps the 6-frame stability
         * window meaningful rather than flickering on small swings). */
        if (Config::exchangeGateEnabled()
            && simResult.percentGain() < -Config::exchangeGateMinGain())
        {
            clump.addSimResult(simResult, false);
            return false;
        }

        bool attack = shouldAttack(clump, simResult, aggression);

        clump.addSimResult(simResult, attack);

        if (attack) return true;

        /* TODO: Would probably be a good idea to run a retreat sim to see what the consequences of retreating are */

        /* If this is the first run of the combat sim for this fight, always abort immediately */
        if (clump.recentSimResults.size() < 2)
        {
#if DEBUG_COMBATSIM
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": aborting as this is the first sim result";
#endif
            return false;
        }

        CombatSimResult &previousSimResult = clump.recentSimResults.rbegin()[1].first;

        /* If the enemy army strength has increased significantly, abort the attack immediately */
        if (simResult.initialEnemy > (int) ((double) previousSimResult.initialEnemy * 1.2))
        {
#if DEBUG_COMBATSIM
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": aborting as there are now more enemy units";
#endif
            return false;
        }

        /* If the fight is now across a choke, abort the attack immediately */
        /* We probably want to try to hold the choke instead */
        if (simResult.narrowChoke && !previousSimResult.narrowChoke)
        {
#if DEBUG_COMBATSIM
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": aborting as the fight is now across a narrow choke";
#endif
            return false;
        }

        int attackFrames;
        int regroupFrames;
        int consecutiveRetreatFrames = UnitCluster::consecutiveSimResults(clump.recentSimResults, &attackFrames, &regroupFrames, 48);

        /* Continue if the sim hasn't been stable for 6 frames */
        if (consecutiveRetreatFrames < 6)
        {
#if DEBUG_COMBATSIM_LOG
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": continuing attack as the sim has not yet been stable for 6 frames";
#endif
            return true;
        }

        /* Continue if the sim has recommended attacking more than regrouping */
        if (attackFrames > regroupFrames)
        {
#if DEBUG_COMBATSIM_LOG
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": continuing attack; regroup=" << regroupFrames
                             << " vs. attack=" << attackFrames;
#endif
            return true;
        }

        /* Abort */
#if DEBUG_COMBATSIM
        Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": aborting attack; regroup=" << regroupFrames
                         << " vs. attack=" << attackFrames;
#endif

        return false;
    }

    bool shouldStopRegrouping(UnitCluster &clump,
                              CombatSimResult &simResult,
                              double closestReinforcements,
                              double reinforcementPercentage)
    {
        /* Always attack if we are maxed and have no significant reinforcements incoming */
        if (BWAPI::Broodwar->self()->supplyUsed() > 380 && reinforcementPercentage < 0.075)
        {
            clump.addSimResult(simResult, true);
            return true;
        }

        /* Be more conservative if the battle is across a choke */
        double aggression = 0.8;
        if (simResult.narrowChoke)
        {
            /* Scale based on width: 128 pixels wide gives no reduction, 48 or lower gives 0.2 reduction */
            aggression -= std::max(0.0, 0.2 * std::min(1.0, (128.0 - (double) simResult.narrowChoke->width) / 80.0));

            /* Scale based on length: 0 pixels long gives no reduction, 128 or higher gives 0.35 reduction */
            aggression -= std::max(0.0, 0.35 * std::min(1.0, ((double) simResult.narrowChoke->length) / 128.0));
        }
        else if (clump.currentSubActivity == UnitCluster::SubActivity::ContainChoke)
        {
            /* Special case if we were previously containing the choke and now the battle is not across it any more */
            /* This indicates the enemy has moved across the choke, so consider this to be attacking instead */
            aggression = 1.2;
        }
        else if (clump.currentSubActivity == UnitCluster::SubActivity::StandGround)
        {
            /* This is considered a somewhat neutral case */
            aggression = 1.0;
        }

        /* Adjust the aggression for reinforcements */
        aggression *= reinforcementFactor(clump, closestReinforcements, reinforcementPercentage);

        /* Adjust the aggression if we have been pushed back into our natural */
        if (pushedBackToNatural(clump, 500))
        {
            aggression *= 2.0;
        }

        bool attack = shouldAttack(clump, simResult, aggression);

        clump.addSimResult(simResult, attack);

        if (!attack) return false;

        int attackFrames;
        int regroupFrames;
        int consecutiveAttackFrames = UnitCluster::consecutiveSimResults(clump.recentSimResults, &attackFrames, &regroupFrames, 72);

        /* Continue if the sim hasn't been stable for 12 frames */
        if (consecutiveAttackFrames < 12)
        {
#if DEBUG_COMBATSIM_LOG
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": continuing regroup as the sim has not yet been stable for 12 frames";
#endif
            return false;
        }

        /* Continue if the sim has recommended regrouping more than attacking */
        if (regroupFrames > attackFrames)
        {
#if DEBUG_COMBATSIM_LOG
            Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": continuing regroup; regroup=" << regroupFrames
                             << " vs. attack=" << attackFrames;
#endif
            return false;
        }

        /* Continue if our number of units has increased in the past 72 frames. */
        /* This gives our reinforcements time to link up with the rest of the cluster before engaging. */
        int count = 0;
        for (auto iterRef = clump.recentSimResults.rbegin(); iterRef != clump.recentSimResults.rend() && count < 72; iterRef++)
        {
            if (simResult.myUnitCount > iterRef->first.myUnitCount)
            {
#if DEBUG_COMBATSIM_LOG
                Telemetry::log() << BWAPI::WalkPosition(clump.midpoint)
                                 << ": continuing regroup as more friendly units have joined the cluster in the past 72 frames";
#endif
                return false;
            }

            count++;
        }

        /* Start the attack */
#if DEBUG_COMBATSIM
        Telemetry::log() << BWAPI::WalkPosition(clump.midpoint) << ": starting attack; regroup=" << regroupFrames
                         << " vs. attack=" << attackFrames;
#endif

        return true;
    }
}

void AttackBaseSquad::execute(UnitCluster &clump)
{
    /* Look for enemies near this cluster */
    std::set<Unit> enemyUnits;
    int radius = 640;
    if (clump.vanguard) radius += clump.vanguard->getDistance(clump.midpoint);
    UnitMgr::enemyInRadius(enemyUnits, clump.midpoint, radius);

    /* If there are no enemies near the cluster, just move towards the target */
    if (enemyUnits.empty())
    {
        clump.setActivity(UnitCluster::Activity::Moving);
        clump.move(targetPosition);
        return;
    }

    updateDetectionNeeds(enemyUnits);

    /* Select targets */
    auto unitsAndTargets = clump.selectTargets(enemyUnits, targetPosition);

    /* Scan the targets to see if any of our units have a valid target that has been seen recently */
    bool hasValidTarget = false;
    for (const auto &unitAndTarget : unitsAndTargets)
    {
        if (!unitAndTarget.second) continue;

        /* A stationary attacker is valid if we are close to their attack range */
        if (unitAndTarget.second->lastPositionValid && UnitHelper::IsStationaryAttacker(unitAndTarget.second->type)
            && unitAndTarget.first->isInEnemyWeaponRange(unitAndTarget.second, 96))
        {
            hasValidTarget = true;
            break;
        }

        /* Other targets are valid if they have been seen in the past 5 seconds */
        if (unitAndTarget.second->lastSeen > (frameNow - 120))
        {
            hasValidTarget = true;
            break;
        }
    }

    /* Run combat sim */
    auto simResult = clump.runCombatSim(targetPosition, unitsAndTargets, enemyUnits, detectors);

    /* TODO: If our units can't do any damage (e.g. ground-only vs. air, melee vs. kiting ranged units), do something else */

    /* Find the closest reinforcements to our army */
    /* Here reinforcements are defined as a cluster further away from the target position that is moving */
    double closestReinforcements = 0.0;
    size_t totalReinforcements = 0;
    for (auto &other : clusters)
    {
        if (other->midpoint == clump.midpoint) continue;
        if (other->percentageToEnemyMain > clump.percentageToEnemyMain) continue;
        if (other->currentActivity != UnitCluster::Activity::Moving) continue;

        totalReinforcements += other->units.size();

        if (other->percentageToEnemyMain > closestReinforcements)
        {
            closestReinforcements = other->percentageToEnemyMain;
        }
    }
    double reinforcementPercentage = (double) totalReinforcements / (double) (clump.units.size() + totalReinforcements);

    simResult.closestReinforcements = closestReinforcements;
    simResult.reinforcementPercentage = reinforcementPercentage;

    /* Make the final decision based on what state we are currently in */

    bool attack;
    switch (clump.currentActivity)
    {
        case UnitCluster::Activity::Moving:
            attack = shouldStartAttack(clump, simResult, closestReinforcements, reinforcementPercentage);
            break;
        case UnitCluster::Activity::Attacking:
            attack = shouldContinueAttack(clump, simResult, closestReinforcements, reinforcementPercentage);
            break;
        case UnitCluster::Activity::Regrouping:
            attack = shouldStopRegrouping(clump, simResult, closestReinforcements, reinforcementPercentage);
            break;
    }

    if (attack || ignoreCombatSim)
    {
        clump.setActivity(UnitCluster::Activity::Attacking);

        /* Move instead if none of our units have a valid target */
        if (!hasValidTarget)
        {
            clump.move(targetPosition);
            return;
        }

        clump.attack(unitsAndTargets, targetPosition);
        return;
    }

    /* Check if our cluster should try to link up with a closer cluster */
    if (currentVanguardCluster && currentVanguardCluster->vanguard && currentVanguardCluster->midpoint != clump.midpoint)
    {
        /* Link up if our vanguard unit is closer to the vanguard cluster than its current target */
        bool linkUp = false;
        for (auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.first != clump.vanguard) continue;

            if (!unitAndTarget.second || !unitAndTarget.second->lastPositionValid)
            {
                linkUp = true;
                break;
            }

            int distTarget = unitAndTarget.first->isFlying
                             ? unitAndTarget.first->getDistance(unitAndTarget.second->prevPos)
                             : Pathfinder::GetGroundDistance(
                            unitAndTarget.first->prevPos,
                            unitAndTarget.second->prevPos,
                            unitAndTarget.first->type,
                            Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
            int distVanguardCluster = unitAndTarget.first->isFlying
                                      ? unitAndTarget.first->getDistance(currentVanguardCluster->vanguard->prevPos)
                                      : Pathfinder::GetGroundDistance(unitAndTarget.first->prevPos,
                                                                       currentVanguardCluster->vanguard->prevPos,
                                                                       unitAndTarget.first->type,
                                                                       Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
            linkUp = (distTarget == -1 || distVanguardCluster == -1 || distVanguardCluster < distTarget);

            break;
        }

        if (linkUp)
        {
            clump.setActivity(UnitCluster::Activity::Moving);
            clump.move(currentVanguardCluster->vanguard->prevPos);
            return;
        }
    }

    /* TODO: Run retreat sim? */

    clump.setActivity(UnitCluster::Activity::Regrouping);
    clump.regroup(unitsAndTargets, enemyUnits, detectors, simResult, targetPosition, hasValidTarget);
}