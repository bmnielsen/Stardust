#include "DefendMainBase.h"

#include "Map.h"
#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Workers.h"

#define DEBUG_PLAY_STATE false

/*
 * Main base defense:
 * - Initially keep some zealots for safety until we have scouting information
 * - If under pressure, produce infinite zealots -> triggers Producer to remove workers from gas
 * - Workers do worker defense if enemy melee units enter the mineral line
 */

namespace
{
    const int REGROUP_EMERGENCY_TIMEOUT = 30 * 24;
}

DefendMainBase::DefendMainBase()
        : DefendBase(Map::getMyMain())
        , lastRegroupFrame(0)
        , enemyUnitsInBase(false)
        , emergencyProduction(BWAPI::UnitTypes::None)
{
}

void DefendMainBase::update()
{
    // Get enemy combat units in our base
    std::set<Unit> enemyCombatUnits;
    std::set<Unit> enemyWorkers;
    bool enemyFlyingUnit = false;
    bool gasSteal = false;
    for (const Unit &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != base->getArea()) continue;

        if (unit->type.isWorker())
        {
            enemyWorkers.insert(unit);
            if (unit->lastSeenAttacking > (BWAPI::Broodwar->getFrameCount() - 48)) enemyCombatUnits.insert(unit);
        }
        else if (UnitUtil::IsCombatUnit(unit->type) && unit->type.canAttack())
        {
            enemyCombatUnits.insert(unit);
            if (unit->isFlying) enemyFlyingUnit = true;
        }
        else if (unit->type.isRefinery())
        {
            gasSteal = true;
        }
    }

    // If there are more than two workers, consider this to be a worker rush and add them to the set of combat units
    if (enemyWorkers.size() > 2)
    {
        enemyCombatUnits.insert(enemyWorkers.begin(), enemyWorkers.end());
    }

    mineralLineWorkerDefense(enemyCombatUnits);

    enemyUnitsInBase = !enemyCombatUnits.empty();

    // Keep track of when the squad was last regrouping, considering an empty squad to be regrouping
    if (enemyUnitsInBase && (squad->getUnits().empty() || squad->hasClusterWithActivity(UnitCluster::Activity::Regrouping)))
    {
        lastRegroupFrame = BWAPI::Broodwar->getFrameCount();
    }

    // If the squad has been regrouping recently, consider this an emergency:
    // - reserve all existing zealots and dragoons for the squad, regardless of where they are
    // - order an infinite number of zealots (or dragoons if the enemy has flying units)
    emergencyProduction = BWAPI::UnitTypes::None;
    if (lastRegroupFrame > 0 && lastRegroupFrame > (BWAPI::Broodwar->getFrameCount() - REGROUP_EMERGENCY_TIMEOUT))
    {
        status.unitRequirements.emplace_back(1000, BWAPI::UnitTypes::Protoss_Zealot, squad->getTargetPosition());
        status.unitRequirements.emplace_back(1000, BWAPI::UnitTypes::Protoss_Dragoon, squad->getTargetPosition());
        emergencyProduction = enemyFlyingUnit ? BWAPI::UnitTypes::Protoss_Dragoon : BWAPI::UnitTypes::Protoss_Zealot;

#if DEBUG_PLAY_STATE
        CherryVis::log() << "Defend base: emergency, producing " << emergencyProduction;
#endif
        return;
    }

    // If there are no enemy units and we have a large enough army, disband the squad
    // TODO: Consider scouting information and matchup to determine what a "large enough" army is - currently using 4 units
    if (!enemyUnitsInBase && !gasSteal &&
        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon)) >= 4)
    {
        status.removedUnits = squad->getUnits();

#if DEBUG_PLAY_STATE
        CherryVis::log() << "Defend base: disband";
#endif
        return;
    }

    // Order base production of what we want in the squad until our army is large enough
    // TODO: Remember gas steals when this is changed
    int zealotsNeeded = 4 - squad->getUnits().size();
    if (gasSteal) zealotsNeeded = 1000;
    if (zealotsNeeded > 0)
    {
        status.unitRequirements.emplace_back(zealotsNeeded, BWAPI::UnitTypes::Protoss_Zealot, squad->getTargetPosition());
    }

#if DEBUG_PLAY_STATE
    CherryVis::log() << "Defend base: normal, need " << zealotsNeeded << " zealots";
#endif
}

void DefendMainBase::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // If we have an emergency production type, produce an infinite number of them off two gateways
    if (emergencyProduction != BWAPI::UnitTypes::None)
    {
        prioritizedProductionGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>, emergencyProduction, -1, 2);
        return;
    }

    // Otherwise produce to fulfill our unit requirements
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                    unitRequirement.type,
                                                    unitRequirement.count,
                                                    (unitRequirement.count + 1) / 2);
    }
}
