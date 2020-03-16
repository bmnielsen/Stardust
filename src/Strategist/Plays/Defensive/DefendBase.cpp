#include "DefendBase.h"

#include "Map.h"
#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Workers.h"

/*
 * General approach:
 *
 * - If the mineral line is under attack, do worker defense
 * - Otherwise, determine the level of risk to the base.
 * - If the base cannot be saved, evacuate workers
 * - If high risk (enemy units are here or expected to come soon), reserve some units for protection
 * - If at medium risk, get some static defense
 * - If at low risk, do nothing
 */

DefendBase::DefendBase(Base *base) : base(base), squad(std::make_shared<DefendBaseSquad>(base))
{
    General::addSquad(squad);
}

void DefendBase::update()
{
    // TODO
}

void DefendBase::mineralLineWorkerDefense(std::set<Unit> &enemiesInBase)
{
    // Check if there are enemy melee units in our mineral line
    std::set<Unit> enemyUnits;
    Units::enemy(enemyUnits, [this](const Unit &unit)
    {
        if (!unit->lastPositionValid) return false;
        if (!UnitUtil::IsCombatUnit(unit->type) || UnitUtil::IsRangedUnit(unit->type) || !UnitUtil::CanAttackGround(unit->type)) return false;

        auto comingPosition = unit->predictPosition(24);
        return base->isInMineralLine(BWAPI::TilePosition(comingPosition));
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
    Workers::reserveBaseWorkers(reservedWorkers, base);

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
    if (base->workerDefenseRallyPatch)
    {
        for (const auto &worker : reservedWorkers)
        {
            worker->gather(base->workerDefenseRallyPatch);
        }
    }
}
