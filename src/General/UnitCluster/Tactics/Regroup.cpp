#include "UnitCluster.h"

#include "Units.h"
#include "Map.h"
#include "Geo.h"
#include "Players.h"

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

        auto simResult = cluster.runCombatSim(filteredUnitsAndTargets, filteredEnemyUnits, false, initialSimResult.narrowChoke);

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
                            const CombatSimResult &initialSimResult)
    {
        if (!initialSimResult.narrowChoke) return false;

        auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits, false, initialSimResult.narrowChoke);

        bool contain = simResult.myPercentLost() <= 0.001 ||
                       (simResult.valueGain() > 0 && simResult.percentGain() > -0.3) ||
                       simResult.percentGain() > -0.2;

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
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
}

void UnitCluster::regroup(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                          std::set<Unit> &enemyUnits,
                          const CombatSimResult &simResult,
                          BWAPI::Position targetPosition)
{
    // If this is the first regrouping frame, determine the type of regrouping to use
    if (currentSubActivity == SubActivity::None)
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

        if (staticDefense && shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits, simResult))
        {
            setSubActivity(SubActivity::ContainStaticDefense);
        }
        else if (shouldContainChoke(*this, unitsAndTargets, enemyUnits, simResult))
        {
            setSubActivity(SubActivity::ContainChoke);
        }
        else
        {
            setSubActivity(SubActivity::Flee);
        }
    }
    else if ((currentSubActivity == SubActivity::ContainStaticDefense && !shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits, simResult))
             || (currentSubActivity == SubActivity::ContainChoke && !shouldContainChoke(*this, unitsAndTargets, enemyUnits, simResult)))
    {
        setSubActivity(SubActivity::Flee);
    }

    if (currentSubActivity == SubActivity::Flee)
    {
        // TODO: Support fleeing elsewhere
        move(Map::getMyMain()->getPosition());
    }

    if (currentSubActivity == SubActivity::ContainStaticDefense)
    {
        containBase(unitsAndTargets, enemyUnits, targetPosition);
    }

    if (currentSubActivity == SubActivity::ContainChoke)
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
    }
}
