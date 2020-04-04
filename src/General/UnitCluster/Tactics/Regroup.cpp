#include "UnitCluster.h"

#include "Units.h"
#include "Map.h"
#include "Geo.h"

/*
 * Orders a cluster to regroup.
 *
 * There are several regrouping strategies that can be chosen based on the situation:
 * - Contain: The enemy is considered to be mainly static, so retreat to a safe distance and attack anything that comes into range.
 * - Pull back: Retreat to a location that is more easily defensible (choke, high ground) and set up appropriately.
 * - Flee: Move back towards our main base until we are reinforced.
 * - Explode: Send the cluster units in multiple directions to confuse the enemy.
 */

namespace
{
    bool isStaticDefense(BWAPI::UnitType type)
    {
        return type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
               type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
               type == BWAPI::UnitTypes::Terran_Bunker ||
               type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
    }

    bool shouldContainStaticDefense(UnitCluster &cluster, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, std::set<Unit> &enemyUnits)
    {
        // Run a combat sim excluding enemy static defense
        std::vector<std::pair<MyUnit, Unit>> filteredUnitsAndTargets;
        std::set<Unit> filteredEnemyUnits;
        for (const auto &pair : unitsAndTargets)
        {
            if (pair.second && !isStaticDefense(pair.second->type))
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
            if (!isStaticDefense(unit->type)) filteredEnemyUnits.insert(unit);
        }

        auto simResult = cluster.runCombatSim(filteredUnitsAndTargets, filteredEnemyUnits);
        auto adjustedSimResult = simResult.adjustForChoke(false);

        bool contain = adjustedSimResult.myPercentLost() <= 0.001 ||
                       (adjustedSimResult.valueGain() > 0 && adjustedSimResult.percentGain() > -0.05) ||
                       adjustedSimResult.percentGain() > 0.2;

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": %l=" << adjustedSimResult.myPercentLost()
                         << "; vg=" << adjustedSimResult.valueGain()
                         << "; %g=" << adjustedSimResult.percentGain()
                         << (contain ? "; CONTAIN_STATIC" : "; FLEE");
#endif

        return contain;
    }

    bool shouldContainChoke(UnitCluster &cluster, const CombatSimResult &simResult)
    {
        if (!simResult.narrowChoke) return false;

        auto adjustedSimResult = simResult.adjustForChoke(false);

        bool contain = adjustedSimResult.myPercentLost() <= 0.001 ||
                       (adjustedSimResult.valueGain() > 0 && adjustedSimResult.percentGain() > -0.3) ||
                       adjustedSimResult.percentGain() > -0.2;

#if DEBUG_COMBATSIM
        CherryVis::log() << BWAPI::WalkPosition(cluster.center)
                         << ": %l=" << adjustedSimResult.myPercentLost()
                         << "; vg=" << adjustedSimResult.valueGain()
                         << "; %g=" << adjustedSimResult.percentGain()
                         << (contain ? "; CONTAIN_CHOKE" : "; FLEE");
#endif

        cluster.addRegroupSimResult(adjustedSimResult, contain);

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
            if (isStaticDefense(unit->type))
            {
                staticDefense = true;
                break;
            }
        }

        if (staticDefense && shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits))
        {
            setSubActivity(SubActivity::ContainStaticDefense);
        }
        else if (shouldContainChoke(*this, simResult))
        {
            setSubActivity(SubActivity::ContainChoke);
        }
        else
        {
            setSubActivity(SubActivity::Flee);
        }
    }
    else if ((currentSubActivity == SubActivity::ContainStaticDefense && !shouldContainStaticDefense(*this, unitsAndTargets, enemyUnits)) ||
             (currentSubActivity == SubActivity::ContainChoke && !shouldContainChoke(*this, simResult)))
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
        holdChoke(simResult.narrowChoke, unitsAndTargets, targetPosition);
    }
}
