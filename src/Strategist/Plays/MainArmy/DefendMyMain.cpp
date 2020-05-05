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
        : emergencyProduction(BWAPI::UnitTypes::None)
        , squad(std::make_shared<DefendBaseSquad>(Map::getMyMain()))
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

    mineralLineWorkerDefense(enemyCombatUnits);

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

    // Also move the reserved gas steal attacker if we have one
    if (reservedGasStealAttacker)
    {
        movableUnitCallback(reservedGasStealAttacker);
    }

    // Make sure to release any workers that have been reserved
    if (!reservedWorkers.empty())
    {
        for (const auto &worker : reservedWorkers)
        {
            Workers::releaseWorker(worker);
        }

        reservedWorkers.clear();
    }
}

void DefendMyMain::mineralLineWorkerDefense(std::set<Unit> &enemiesInBase)
{
    // Check if there are enemy melee units in our mineral line
    std::set<Unit> enemyUnits;
    Units::enemy(enemyUnits, [](const Unit &unit)
    {
        if (!unit->lastPositionValid) return false;
        if (!UnitUtil::IsCombatUnit(unit->type) || UnitUtil::IsRangedUnit(unit->type) || !UnitUtil::CanAttackGround(unit->type)) return false;

        auto comingPosition = unit->predictPosition(24);
        return comingPosition.isValid() && Map::isInOwnMineralLine(BWAPI::TilePosition(comingPosition));
    });

    if (enemyUnits.empty())
    {
        if (!reservedWorkers.empty())
        {
            for (const auto &worker : reservedWorkers)
            {
                Workers::releaseWorker(worker);
            }

            reservedWorkers.clear();
        }

        return;
    }

    // Prune dead workers from the list
    for (auto it = reservedWorkers.begin(); it != reservedWorkers.end();)
    {
        if ((*it)->exists())
        {
            it++;
        }
        else
        {
            it = reservedWorkers.erase(it);
        }
    }

    // Grab all of the base's workers
    Workers::reserveBaseWorkers(reservedWorkers, Map::getMyMain());

    // Choose a target for each of our workers
    // Each attacks the closest enemy unit with the least health
    bool isEnemyInRange = false;
    std::vector<std::pair<MyUnit, Unit>> workersAndTargets;
    for (const auto &worker : reservedWorkers)
    {
        Unit bestTarget = nullptr;
        int bestTargetDist = INT_MAX;
        int bestTargetHealth = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            int dist = worker->getDistance(enemy);
            if (dist < bestTargetDist || (dist == bestTargetDist && enemy->lastHealth < bestTargetHealth))
            {
                bestTarget = enemy;
                bestTargetDist = dist;
                bestTargetHealth = enemy->lastHealth;
            }
        }

        workersAndTargets.emplace_back(std::make_pair(worker, bestTarget));

        auto predictedPosition = bestTarget->predictPosition(BWAPI::Broodwar->getLatencyFrames());
        if (worker->isInOurWeaponRange(bestTarget, predictedPosition) ||
            worker->isInEnemyWeaponRange(bestTarget, predictedPosition))
        {
            isEnemyInRange = true;
        }
    }

    // Consider if any combat units are in range of an enemy
    if (!isEnemyInRange && squad)
    {
        for (const auto &unit : squad->getUnits())
        {
            for (auto &enemy : enemyUnits)
            {
                if (unit->isInOurWeaponRange(enemy) || unit->isInEnemyWeaponRange(enemy))
                {
                    isEnemyInRange = true;
                    goto breakOuterLoop;
                }
            }
        }
        breakOuterLoop:;
    }

    // If any of our units are in range of an enemy, we attack
    if (isEnemyInRange)
    {
        for (auto &workerAndTarget : workersAndTargets)
        {
            workerAndTarget.first->attack(workerAndTarget.second->bwapiUnit);
        }

        return;
    }

    // Otherwise we rally our workers on the mineral patch
    auto rallyPatch = Map::getMyMain()->workerDefenseRallyPatch;
    if (rallyPatch)
    {
        for (const auto &worker : reservedWorkers)
        {
            worker->gather(rallyPatch);
        }
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
