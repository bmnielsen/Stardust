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
 * - Contain base: The enemy is considered to be mainly static, so retreat to a safe distance and attack anything that comes into range.
 * - Hold choke: We can stay at our choke and hold off the enemy.
 * - Flee: Move back towards our main base until we are reinforced.
 */

namespace
{
    bool shouldContainStaticDefense(UnitCluster &cluster,
                                    std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                                    std::set<Unit> &enemyUnits,
                                    std::set<MyUnit> &detectors,
                                    const CombatSimResult &initialSimResult)
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

        auto simResult = cluster.runCombatSim(filteredUnitsAndTargets, filteredEnemyUnits, detectors, false, initialSimResult.narrowChoke);

        bool contain = simResult.myPercentLost() <= 0.001 ||
                       (simResult.valueGain() > 0 && simResult.percentGain() > -0.05) ||
                       simResult.percentGain() > 0.2;

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << (contain ? "; CONTAIN_STATIC" : "; FLEE");
#endif

        return contain;
    }

    bool shouldContainChoke(UnitCluster &cluster,
                            std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                            std::set<Unit> &enemyUnits,
                            std::set<MyUnit> &detectors,
                            const CombatSimResult &initialSimResult)
    {
        if (!initialSimResult.narrowChoke) return false;

        // Never contain a choke that is covered by enemy static defense at both ends
        auto grid = Players::grid(BWAPI::Broodwar->enemy());
        if (grid.staticGroundThreat(initialSimResult.narrowChoke->end1Center) > 0 &&
            grid.staticGroundThreat(initialSimResult.narrowChoke->end2Center) > 0)
        {
            return false;
        }

        auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, detectors, false, initialSimResult.narrowChoke);

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
            if (simResult.percentGain() > (1.0 - distanceFactor)) return true;

            return false;
        };

        bool contain = attack();

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << std::setprecision(2) << "-" << distanceFactor
                         << ": %l=" << simResult.myPercentLost()
                         << "; vg=" << simResult.valueGain()
                         << "; %g=" << simResult.percentGain()
                         << (contain ? "; CONTAIN_CHOKE" : "; FLEE");
#endif

        cluster.addRegroupSimResult(simResult, contain);

        // Always do a contain if the sim tells us to
        if (contain) return true;

        // If this is the first run of the contain sim for this fight, withdraw
        if (cluster.recentRegroupSimResults.size() < 2) return false;

        // Otherwise withdraw only when the sim has been stable for a number of frames
        if (cluster.recentRegroupSimResults.size() < 24)
        {
#if DEBUG_COMBATSIM
            CherryVis::log() << BWAPI::WalkPosition(cluster.center) << ": continuing contain as we have fewer than 24 frames of sim data";
#endif
            return true;
        }

        int count = 0;
        for (auto it = cluster.recentRegroupSimResults.rbegin(); it != cluster.recentRegroupSimResults.rend() && count < 24; it++)
        {
            if (it->second)
            {
#if DEBUG_COMBATSIM
                CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                                 << ": continuing contain as a sim result within the past 24 frames recommended containing";
#endif
                return true;
            }
            count++;
        }

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": aborting as the sim has recommended fleeing for the past 24 frames";
#endif
        return false;
    }

    bool shouldStandGround(const CombatSimResult &initialSimResult)
    {
        // For now just stand ground if the sim result indicates no damage to our units
        // We may want to make this less strict later
        return initialSimResult.myPercentLost() < 0.001;
    }
}

void UnitCluster::regroup(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                          std::set<Unit> &enemyUnits,
                          std::set<MyUnit> &detectors,
                          const CombatSimResult &simResult,
                          BWAPI::Position targetPosition)
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

            if (staticDefense && shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits, detectors, simResult))
            {
                setSubActivity(SubActivity::ContainStaticDefense);
            }
            else if (shouldContainChoke(*this, unitsAndTargets, enemyUnits, detectors, simResult))
            {
                setSubActivity(SubActivity::ContainChoke);
            }
            else
            {
                setSubActivity(SubActivity::Flee);
            }

            break;
        }
        case SubActivity::ContainStaticDefense:
        {
            if (!shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits, detectors, simResult))
            {
                setSubActivity(SubActivity::Flee);
            }
            break;
        }
        case SubActivity::ContainChoke:
        {
            if (!shouldContainChoke(*this, unitsAndTargets, enemyUnits, detectors, simResult))
            {
                setSubActivity(SubActivity::Flee);
            }
            break;
        }
        case SubActivity::StandGround:
        {
            // Flee if it is no longer safe to stand ground
            if (!shouldStandGround(simResult))
            {
                setSubActivity(SubActivity::Flee);
            }

            break;
        }
        case SubActivity::Flee:
        {
            // We might flee through a choke that we can contain from the other side
            if (shouldContainChoke(*this, unitsAndTargets, enemyUnits, detectors, simResult))
            {
                setSubActivity(SubActivity::ContainChoke);
            }

            // While fleeing we will often link up with reinforcements, or the enemy will not pursue, so it makes sense to stand ground instead
            if (shouldStandGround(simResult))
            {
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
            containBase(enemyUnits, targetPosition);

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
            // If the center of the cluster is walkable, move towards it
            // Otherwise move towards the vanguard with the assumption that the center will become walkable soon
            // (otherwise this results in forward motion as units move ahead and become the new vanguard)
            if (Map::isWalkable(BWAPI::TilePosition(center)))
            {
                move(center);
            }
            else if (vanguard)
            {
                move(vanguard->lastPosition);
            }
            else
            {
                // Flee if we for some reason don't have a vanguard unit
                move(Map::getMyMain()->getPosition());
            }

            break;
        }
        case SubActivity::Flee:
        {
            // TODO: Support fleeing elsewhere
            move(Map::getMyMain()->getPosition());

            break;
        }
    }
}
