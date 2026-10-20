#include "DefendMyMain.h"

#include "Map.h"
#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Workers.h"

namespace
{
    const int REGROUP_EMERGENCY_TIMEOUT = 30 * 24;
}

DefendMyMain::DefendMyMain()
        : MainArmyPlay("DefendMyMain")
        , emergencyProduction(BWAPI::UnitTypes::None)
        , squad(std::make_shared<EarlyGameDefendMainBaseSquad>())
        , lastRegroupFrame(0)
        , reservedGasStealAttacker(nullptr)
{
    Marshal::addSquad(squad);
}

void DefendMyMain::update()
{
    if (reservedGasStealAttacker && !reservedGasStealAttacker->exists())
    {
        reservedGasStealAttacker = nullptr;
    }
    
    /* Get enemy combat units in our base */
    std::set<Unit> enemyCombatUnits;
    std::set<Unit> enemyWorkers;
    bool scoutHarass = true;
    bool requireDragoons = false;
    Unit gasSteal = nullptr;
    for (const Unit &gameUnit : UnitMgr::allEnemy())
    {
        if (!gameUnit->lastPositionValid) continue;

        bool isInArea = false;
        for (const auto &area : Map::getMyMainAreas())
        {
            if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos)) == area)
            {
                isInArea = true;
                break;
            }
        }
        if (!isInArea) continue;

        if (gameUnit->type.isWorker())
        {
            enemyWorkers.insert(gameUnit);
            if (gameUnit->lastSeenAttacking > (frameNow - 48)) enemyCombatUnits.insert(gameUnit);
        }
        else if (UnitHelper::IsCombatUnit(gameUnit->type) && gameUnit->type.canAttack())
        {
            scoutHarass = false;
            enemyCombatUnits.insert(gameUnit);
            requireDragoons = requireDragoons ||
                              (gameUnit->isFlying ||
                               gameUnit->type == BWAPI::UnitTypes::Protoss_Dragoon ||
                               gameUnit->type == BWAPI::UnitTypes::Terran_Vulture);
        }
        else if (gameUnit->type.isRefinery())
        {
            gasSteal = gameUnit;
        }
    }

    if (gasSteal)
    {
        /* Reserve a gas steal attacker if we have three units in the squad */
        auto units = (reservedGasStealAttacker ? 1 : 0) + squad->getUnits().size();
        if (units > 2 && !reservedGasStealAttacker)
        {
            int minDist = INT_MAX;
            for (const auto &gameUnit : squad->getUnits())
            {
                int span = gameUnit->getDistance(gasSteal);
                if (span < minDist)
                {
                    minDist = span;
                    reservedGasStealAttacker = gameUnit;
                }
            }

            if (reservedGasStealAttacker)
            {
                squad->removeUnit(reservedGasStealAttacker);
            }
        }
        else if (units < 3 && reservedGasStealAttacker)
        {
            /* Release the reserved gas steal attacker if we need it for defense */
            squad->addUnit(reservedGasStealAttacker);
            reservedGasStealAttacker = nullptr;
        }

        /* Pull workers to attack the gas steal */
        int desiredWorkers = 2;
        if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Zealot) > 0) desiredWorkers = 4;
        if (reservedWorkerGasStealAttackers.size() < desiredWorkers)
        {
            for (int i = 0; i < (desiredWorkers - reservedWorkerGasStealAttackers.size()); i++)
            {
                auto wrk = WorkerMgr::getClosestReassignableWorker(Map::getMyMain()->getPosition(), false);
                if (!wrk) break;

                WorkerMgr::reserveWorker(wrk);
                reservedWorkerGasStealAttackers.push_back(wrk);
            }
        }
        else if (reservedWorkerGasStealAttackers.size() > desiredWorkers)
        {
            auto iterRef = reservedWorkerGasStealAttackers.begin();
            WorkerMgr::releaseWorker(*iterRef);
            reservedWorkerGasStealAttackers.erase(iterRef);
        }

        /* Execute attack with our reserved units */
        if (reservedGasStealAttacker) reservedGasStealAttacker->attackUnit(gasSteal);
        for (auto &wrk : reservedWorkerGasStealAttackers)
        {
            std::static_pointer_cast<MyUnitImpl>(wrk)->attackUnit(gasSteal);
        }
    }
    else
    {
        /* Release the reserved gas steal attacker when it is no longer needed */
        if (reservedGasStealAttacker)
        {
            squad->addUnit(reservedGasStealAttacker);
            reservedGasStealAttacker = nullptr;
        }

        /* Release the worker gas steal attackers when they are no longer needed */
        if (!reservedWorkerGasStealAttackers.empty())
        {
            for (const auto &workerGasStealAttacker : reservedWorkerGasStealAttackers)
            {
                WorkerMgr::releaseWorker(workerGasStealAttacker);
            }
            reservedWorkerGasStealAttackers.clear();
        }
    }

    /* If there are more than two workers, consider this to be a worker rush and add them to the set of combat units */
    if (enemyWorkers.size() > 2)
    {
        scoutHarass = false;
        enemyCombatUnits.insert(enemyWorkers.begin(), enemyWorkers.end());
    }

    /* Keep track of when the squad was last regrouping, considering an empty squad to be regrouping */
    if (!enemyCombatUnits.empty() && !scoutHarass && (squad->getUnits().empty() || squad->hasClusterWithActivity(UnitCluster::Activity::Regrouping)))
    {
        lastRegroupFrame = frameNow;
    }

    /* Queue emergency production if: */
    /* - The squad has been regrouping recently */
    /* - The enemy has us outnumbered by more than two combat units */
    /* - We don't have any completed cannons (vs. Terran) */
    if (lastRegroupFrame > 0 && lastRegroupFrame > (frameNow - REGROUP_EMERGENCY_TIMEOUT) &&
        (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran || UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Photon_Cannon) == 0) &&
        (squad->combatUnitCount() == 0 || (enemyCombatUnits.size() + std::max(0, (int) enemyWorkers.size() - 1)) > (squad->combatUnitCount() + 2)))
    {
        auto desiredEmergencyProduction = requireDragoons ? BWAPI::UnitTypes::Protoss_Dragoon : BWAPI::UnitTypes::Protoss_Zealot;
        if (emergencyProduction != desiredEmergencyProduction)
        {
            emergencyProduction = desiredEmergencyProduction;
            Telemetry::log() << "DefendMyMain: emergency, producing " << emergencyProduction;
        }

        return;
    }
    else if (emergencyProduction != BWAPI::UnitTypes::None)
    {
        emergencyProduction = BWAPI::UnitTypes::None;
        Telemetry::log() << "DefendMyMain: emergency cleared";
    }
}

void DefendMyMain::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* If we have an emergency production type, produce an infinite number of them off two gateways */
    if (emergencyProduction != BWAPI::UnitTypes::None)
    {
        prioProdGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>, label, emergencyProduction, -1, 2);
        return;
    }

    /* Otherwise produce to fulfill our unit requirements */
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioProdGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                      label,
                                                                      unitRequirement.type,
                                                                      unitRequirement.count,
                                                                      (unitRequirement.count + 1) / 2);
    }
}

void DefendMyMain::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                           const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    PlayItem::disband(removedUnitCallback, movableUnitCallback);

    /* Also move the reserved gas steal attacker if we have one */
    if (reservedGasStealAttacker)
    {
        movableUnitCallback(reservedGasStealAttacker);
    }

    for (const auto &workerGasStealAttacker : reservedWorkerGasStealAttackers)
    {
        WorkerMgr::releaseWorker(workerGasStealAttacker);
    }
}

bool DefendMyMain::canTransitionToAttack() const
{
    return squad->canTransitionToAttack();
}

void DefendMyMain::removeUnit(const OwnedUnit &gameUnit)
{
    if (gameUnit == reservedGasStealAttacker)
    {
        reservedGasStealAttacker = nullptr;
    }

    PlayItem::removeUnit(gameUnit);
}
