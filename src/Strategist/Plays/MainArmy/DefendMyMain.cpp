#include "DefendMyMain.h"

#include "Map.h"
#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Workers.h"

#define DEBUG_PLAY_STATE false

namespace
{
    const int REGROUP_EMERGENCY_TIMEOUT = 30 * 24;
}

DefendMyMain::DefendMyMain()
        : MainArmyPlay("DefendMyMain")
        , emergencyProduction(BWAPI::UnitTypes::None)
        , squad(std::make_shared<EarlyGameDefendMainBaseSquad>())
        , workerDefenseSquad(std::make_shared<WorkerDefenseSquad>(Map::getMyMain()))
        , lastRegroupFrame(0)
        , reservedGasStealAttacker(nullptr)
{
    General::addSquad(squad);
}

void DefendMyMain::update()
{
    // Get enemy combat units in our base
    std::set<Unit> enemyCombatUnits;
    std::set<Unit> enemyWorkers;
    bool enemyFlyingUnit = false;
    Unit gasSteal = nullptr;
    for (const Unit &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != Map::getMyMain()->getArea()) continue;

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
            gasSteal = unit;
        }
    }

    if (gasSteal)
    {
        // Reserve a gas steal attacker if we have three units in the squad
        auto units = (reservedGasStealAttacker ? 1 : 0) + squad->getUnits().size();
        if (units > 2 && !reservedGasStealAttacker)
        {
            int minDist = INT_MAX;
            for (const auto &unit : squad->getUnits())
            {
                int dist = unit->getDistance(gasSteal);
                if (dist < minDist)
                {
                    minDist = dist;
                    reservedGasStealAttacker = unit;
                }
            }

            if (reservedGasStealAttacker)
            {
                squad->removeUnit(reservedGasStealAttacker);
            }
        }
        else if (units < 3 && reservedGasStealAttacker)
        {
            // Release the reserved gas steal attacker if we need it for defense
            squad->addUnit(reservedGasStealAttacker);
            reservedGasStealAttacker = nullptr;
        }
    }
    else if (reservedGasStealAttacker)
    {
        // Release the reserved gas steal attacker when it is no longer needed
        squad->addUnit(reservedGasStealAttacker);
        reservedGasStealAttacker = nullptr;
    }

    // If there are more than two workers, consider this to be a worker rush and add them to the set of combat units
    if (enemyWorkers.size() > 2)
    {
        enemyCombatUnits.insert(enemyWorkers.begin(), enemyWorkers.end());
    }

    workerDefenseSquad->execute(enemyCombatUnits, squad);

    // Keep track of when the squad was last regrouping, considering an empty squad to be regrouping
    if (!enemyCombatUnits.empty() && (squad->getUnits().empty() || squad->hasClusterWithActivity(UnitCluster::Activity::Regrouping)))
    {
        lastRegroupFrame = BWAPI::Broodwar->getFrameCount();
    }

    // If the squad has been regrouping recently, consider this an emergency
    emergencyProduction = BWAPI::UnitTypes::None;
    if (lastRegroupFrame > 0 && lastRegroupFrame > (BWAPI::Broodwar->getFrameCount() - REGROUP_EMERGENCY_TIMEOUT))
    {
        emergencyProduction = enemyFlyingUnit ? BWAPI::UnitTypes::Protoss_Dragoon : BWAPI::UnitTypes::Protoss_Zealot;

#if DEBUG_PLAY_STATE
        CherryVis::log() << "Defend base: emergency, producing " << emergencyProduction;
#endif
        return;
    }
}

void DefendMyMain::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
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

void DefendMyMain::disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                           const std::function<void(const MyUnit &)> &movableUnitCallback)
{
    Play::disband(removedUnitCallback, movableUnitCallback);

    workerDefenseSquad->disband();

    // Also move the reserved gas steal attacker if we have one
    if (reservedGasStealAttacker)
    {
        movableUnitCallback(reservedGasStealAttacker);
    }
}

void DefendMyMain::removeUnit(MyUnit unit)
{
    if (unit == reservedGasStealAttacker)
    {
        reservedGasStealAttacker = nullptr;
    }

    Play::removeUnit(unit);
}
