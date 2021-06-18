#include "UnitCluster.h"

#include "Units.h"
#include "Map.h"
#include "Geo.h"
#include "Players.h"

#include "DebugFlag_CombatSim.h"

#include <iomanip>

/*
 * Orders a cluster to regroup.
 *
 * There are several regrouping strategies that can be chosen based on the situation:
 * - Contain static: The enemy is considered to be mainly static, so retreat to a safe distance and attack anything that comes into range.
 * - Hold choke: We can stay at a choke and hold off the enemy.
 * - Stand ground: We can stop a safe distance from the enemy until we are reinforced.
 * - Flee: Move back towards our main base until we are reinforced.
 */

namespace
{
    bool shouldContainStaticDefense(UnitCluster &cluster,
                                    std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                                    std::set<Unit> &enemyUnits,
                                    std::set<MyUnit> &detectors,
                                    const CombatSimResult &initialSimResult,
                                    BWAPI::Position targetPosition)
    {
        // Run a combat sim excluding enemy static defense
        std::vector<std::pair<MyUnit, Unit>> filteredUnitsAndTargets;
        std::set<Unit> filteredEnemyUnits;
        for (const auto &pair : unitsAndTargets)
        {
            if (pair.second && !pair.second->isStaticGroundDefense())
            {
                filteredUnitsAndTargets.emplace_back(pair);
            }
            else
            {
                filteredUnitsAndTargets.emplace_back(std::make_pair(pair.first, nullptr));
            }
        }
        for (auto &unit : enemyUnits)
        {
            if (!unit->isStaticGroundDefense()) filteredEnemyUnits.insert(unit);
        }

        auto simResult = cluster.runCombatSim(targetPosition, filteredUnitsAndTargets, filteredEnemyUnits, detectors, false, initialSimResult.narrowChoke);

        bool contain = simResult.myPercentLost() <= 0.001 ||
                       (simResult.valueGain() > 0 && simResult.percentGain() > -0.05) ||
                       simResult.percentGain() > 0.2;

#if DEBUG_COMBATSIM_LOG
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << (contain ? "; CONTAIN_STATIC" : "; DON'T_CONTAIN_STATIC");
#endif

        return contain;
    }

    bool shouldContainChoke(UnitCluster &cluster,
                            std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                            std::set<Unit> &enemyUnits,
                            std::set<MyUnit> &detectors,
                            const CombatSimResult &initialSimResult,
                            BWAPI::Position targetPosition)
    {
        if (!initialSimResult.narrowChoke) return false;

        // Never contain a choke that is covered by enemy static defense at both ends
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        if (grid.staticGroundThreat(initialSimResult.narrowChoke->end1Center) > 0 &&
            grid.staticGroundThreat(initialSimResult.narrowChoke->end2Center) > 0)
        {
            return false;
        }

        auto simResult = cluster.runCombatSim(targetPosition, unitsAndTargets, enemyUnits, detectors, false, initialSimResult.narrowChoke);

        double distanceFactor = 1.0;
        auto attack = [&]()
        {
            // Always attack if we don't lose anything
            if (simResult.myPercentLost() <= 0.001) return true;

            // Attack in cases where we think we will kill 50% more value than we lose
            if (simResult.valueGain() > (simResult.initialMine - simResult.finalMine) / 2 &&
                (simResult.percentGain() > -0.05 || simResult.myPercentageOfTotal() > 0.9))
            {
                return true;
            }

            // Compute the distance factor, an adjustment based on where our army is relative to our main and the target position
            distanceFactor = 1.2 - 0.4 * cluster.percentageToEnemyMain;

            // Give an extra bonus to narrow chokes close to our base
            if (simResult.narrowChoke && cluster.percentageToEnemyMain < 0.3)
            {
                distanceFactor *= 1.2;
            }

            // Attack if we expect to end the fight with a sufficiently larger army and aren't losing an unacceptable percentage of it
            if (simResult.myPercentageOfTotal() > (1.0 - 0.45 * distanceFactor) && simResult.percentGain() > -0.05 * distanceFactor)
            {
                return true;
            }

            // Attack if the percentage gain, adjusted for distance factor, is acceptable
            // A percentage gain here means the enemy loses a larger percentage of their army than we do
            if (simResult.percentGain() > (1.0 - distanceFactor - 0.1)) return true;

            return false;
        };

        bool contain = attack();

#if DEBUG_COMBATSIM_LOG
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << std::setprecision(2) << "-" << distanceFactor
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << (contain ? "; CONTAIN_CHOKE" : "; DON'T_CONTAIN_CHOKE");
#endif

        simResult.distanceFactor = distanceFactor;

        cluster.addRegroupSimResult(simResult, contain);

        // What we decide depends on the current regroup activity
        switch (cluster.currentSubActivity)
        {
            case UnitCluster::SubActivity::None:
                // This is the first regroup frame, so go with the sim
                return contain;
            case UnitCluster::SubActivity::ContainChoke:
            {
                // Continue the contain if the sim recommends it
                if (contain) return true;

                int containFrames;
                int fleeFrames;
                int consecutiveFleeFrames = UnitCluster::consecutiveSimResults(cluster.recentRegroupSimResults, &containFrames, &fleeFrames, 24);

                // Continue if the sim hasn't been stable for 6 frames
                if (consecutiveFleeFrames < 6)
                {
#if DEBUG_COMBATSIM_LOG
                    CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing contain as the sim has not yet been stable for 6 frames";
#endif
                    return true;
                }

                // Continue if the sim has recommended containing more than fleeing
                if (containFrames > fleeFrames)
                {
#if DEBUG_COMBATSIM_LOG
                    CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing contain; flee=" << fleeFrames
                                     << " vs. contain=" << containFrames;
#endif
                    return true;
                }

                // Abort
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": aborting contain; flee=" << fleeFrames
                                 << " vs. contain=" << containFrames;
#endif
                return false;
            }
            case UnitCluster::SubActivity::StandGround:
            case UnitCluster::SubActivity::Flee:
            {
                if (!contain) return false;

                int containFrames;
                int fleeFrames;
                int consecutiveContainFrames = UnitCluster::consecutiveSimResults(cluster.recentRegroupSimResults, &containFrames, &fleeFrames, 24);

                // Continue if the sim hasn't been stable for 6 frames
                if (consecutiveContainFrames < 6)
                {
#if DEBUG_COMBATSIM_LOG
                    CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": not containing as the sim has not yet been stable for 6 frames";
#endif
                    return false;
                }

                // Continue if the sim has recommended fleeing more than containing
                if (fleeFrames > containFrames)
                {
#if DEBUG_COMBATSIM_LOG
                    CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing flee; flee=" << fleeFrames
                                     << " vs. contain=" << containFrames;
#endif
                    return false;
                }

                // Continue if our number of units has increased in the past 60 frames.
                // This gives our reinforcements time to link up with the rest of the cluster before containing.
                int count = 0;
                for (auto it = cluster.recentRegroupSimResults.rbegin(); it != cluster.recentRegroupSimResults.rend() && count < 60; it++)
                {
                    if (simResult.myUnitCount > it->first.myUnitCount)
                    {
#if DEBUG_COMBATSIM_LOG
                        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                         << ": waiting to contain as more friendly units have joined the cluster in the past 60 frames";
#endif
                        return false;
                    }

                    count++;
                }

                // Start the contain
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": starting contain; flee=" << fleeFrames
                                 << " vs. contain=" << containFrames;
#endif

                return true;
            }
            case UnitCluster::SubActivity::ContainStaticDefense:
                // Both of these states do not consider containing a choke
                break;
        }

        return false;
    }

    bool shouldStandGround(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, const CombatSimResult &initialSimResult, bool hasValidTarget)
    {
        // If none of our units has a target, and none of our units are in danger, stand ground
        if (!hasValidTarget)
        {
            auto &grid = Players::grid(BWAPI::Broodwar->enemy());
            bool anyThreat = false;
            for (auto &unitAndTarget : unitsAndTargets)
            {
                if (unitAndTarget.first->isFlying)
                {
                    if (grid.airThreat(unitAndTarget.first->lastPosition) > 0)
                    {
                        anyThreat = true;
                        break;
                    }
                }
                else
                {
                    if (grid.groundThreat(unitAndTarget.first->lastPosition) > 0)
                    {
                        anyThreat = true;
                        break;
                    }
                }
            }

            if (!anyThreat) return true;
        }

        // For now just stand ground if the sim result indicates no damage to our units
        // We may want to make this less strict later
        return initialSimResult.myPercentLost() < 0.001;
    }
}

void UnitCluster::regroup(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                          std::set<Unit> &enemyUnits,
                          std::set<MyUnit> &detectors,
                          const CombatSimResult &simResult,
                          BWAPI::Position targetPosition,
                          bool hasValidTarget)
{
    // First choose which regrouping mode we want to use
    switch (currentSubActivity)
    {
        case SubActivity::None:
        {
            // Does the enemy have static defense?
            bool staticDefense = false;
            for (const auto &unit : enemyUnits)
            {
                if (unit->isStaticGroundDefense())
                {
                    staticDefense = true;
                    break;
                }
            }

            if (staticDefense && shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits, detectors, simResult, targetPosition))
            {
                setSubActivity(SubActivity::ContainStaticDefense);
            }
            else if (shouldContainChoke(*this, unitsAndTargets, enemyUnits, detectors, simResult, targetPosition))
            {
                setSubActivity(SubActivity::ContainChoke);
            }
            else if (shouldStandGround(unitsAndTargets, simResult, hasValidTarget))
            {
                setSubActivity(SubActivity::StandGround);
            }
            else
            {
                setSubActivity(SubActivity::Flee);
            }

            break;
        }
        case SubActivity::ContainStaticDefense:
        {
            if (!shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits, detectors, simResult, targetPosition))
            {
                setSubActivity(SubActivity::Flee);
            }
            break;
        }
        case SubActivity::ContainChoke:
        {
            if (!shouldContainChoke(*this, unitsAndTargets, enemyUnits, detectors, simResult, targetPosition))
            {
                setSubActivity(SubActivity::Flee);
            }
            break;
        }
        case SubActivity::StandGround:
        {
            // We might be standing ground near a choke that we are now able to contain
            if (shouldContainChoke(*this, unitsAndTargets, enemyUnits, detectors, simResult, targetPosition))
            {
                setSubActivity(SubActivity::ContainChoke);
            }
            else if (!shouldStandGround(unitsAndTargets, simResult, hasValidTarget))
            {
                // Flee if it is no longer safe to stand ground
                setSubActivity(SubActivity::Flee);
            }

            break;
        }
        case SubActivity::Flee:
        {
            // We might flee through a choke that we can contain from the other side
            if (shouldContainChoke(*this, unitsAndTargets, enemyUnits, detectors, simResult, targetPosition))
            {
                setSubActivity(SubActivity::ContainChoke);
            }
            else if (shouldStandGround(unitsAndTargets, simResult, hasValidTarget))
            {
                // While fleeing we will often link up with reinforcements, or the enemy will not pursue, so it makes sense to stand ground instead
                setSubActivity(SubActivity::StandGround);
            }

            break;
        }
    }

    // Now execute the regrouping activity
    switch (currentSubActivity)
    {
        case SubActivity::None:
        {
            Log::Get() << "WARNING: Cluster @ " << BWAPI::TilePosition(center) << " has no valid regroup subactivity to execute";

            break;
        }
        case SubActivity::ContainStaticDefense:
        {
            containStatic(enemyUnits, targetPosition);

            break;
        }
        case SubActivity::ContainChoke:
        {
            auto end1Dist = PathFinding::GetGroundDistance(targetPosition,
                                                           simResult.narrowChoke->end1Center,
                                                           BWAPI::UnitTypes::Protoss_Dragoon,
                                                           PathFinding::PathFindingOptions::UseNearestBWEMArea);
            auto end2Dist = PathFinding::GetGroundDistance(targetPosition,
                                                           simResult.narrowChoke->end2Center,
                                                           BWAPI::UnitTypes::Protoss_Dragoon,
                                                           PathFinding::PathFindingOptions::UseNearestBWEMArea);
            auto chokeDefendEnd = end1Dist < end2Dist ? simResult.narrowChoke->end2Center : simResult.narrowChoke->end1Center;
            holdChoke(simResult.narrowChoke, chokeDefendEnd, unitsAndTargets);

            break;
        }
        case SubActivity::StandGround:
        {
            standGround(enemyUnits, targetPosition);

            break;
        }
        case SubActivity::Flee:
        {
            flee(enemyUnits);

            break;
        }
    }
}
