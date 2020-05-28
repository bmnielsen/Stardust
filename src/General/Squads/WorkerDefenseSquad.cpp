#include "WorkerDefenseSquad.h"

#include "UnitUtil.h"
#include "Map.h"
#include "Workers.h"

namespace
{
    Unit getTarget(const MyUnit &myUnit, const std::set<Unit> &enemyUnits, bool allowRetreating = true, int distThreshold = INT_MAX)
    {
        Unit bestTarget = nullptr;
        int bestTargetDist = INT_MAX;
        int bestTargetHealth = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            int dist = myUnit->getDistance(enemy);
            if (dist < bestTargetDist || (dist == bestTargetDist && (enemy->lastHealth + enemy->lastShields) < bestTargetHealth))
            {
                if (!allowRetreating && (myUnit->getDistance(enemy, enemy->predictPosition(1)) > dist))
                {
                    continue;
                }

                bestTarget = enemy;
                bestTargetDist = dist;
                bestTargetHealth = enemy->lastHealth + enemy->lastShields;
            }
        }

        if (bestTargetDist > distThreshold) return nullptr;
        return bestTarget;
    }
}

void WorkerDefenseSquad::execute(std::set<Unit> &enemiesInBase, const std::shared_ptr<Squad> &defendBaseSquad)
{
    // Prune dead workers from the list
    for (auto it = units.begin(); it != units.end();)
    {
        if ((*it)->exists())
        {
            it++;
        }
        else
        {
            it = units.erase(it);
        }
    }

    // Filter the units to get a set of units it makes sense to do worker defense against
    // The input set is already filtered to only contain units we consider to be threats
    std::set<Unit> enemyUnits;
    for (const auto &unit : enemiesInBase)
    {
        // First sort out units we can't fight against
        if (unit->isFlying) continue;
        if (UnitUtil::IsRangedUnit(unit->type)) continue;

        // Now determine if the unit is worth attacking
        // This is the case if:
        // - It is in or soon will be in our mineral line
        // - It is attacking one of our workers

        if (Map::isInOwnMineralLine(unit->getTilePosition()))
        {
            enemyUnits.insert(unit);
            continue;
        }

        auto comingPosition = unit->predictPosition(24);
        if (comingPosition.isValid() && Map::isInOwnMineralLine(BWAPI::TilePosition(comingPosition)))
        {
            enemyUnits.insert(unit);
            continue;
        }

        if (unit->lastSeenAttacking < BWAPI::Broodwar->getFrameCount() - 48) continue;

        bool attackingAWorker = false;
        for (const auto &myUnit : units)
        {
            if (myUnit->getDistance(unit) < 32)
            {
                attackingAWorker = true;
                break;
            }
        }
        if (!attackingAWorker) continue;
        enemyUnits.insert(unit);
    }

    if (enemyUnits.empty())
    {
        // Release all of the squad's units
        if (!units.empty())
        {
            for (const auto &unit : units)
            {
                Workers::releaseWorker(unit);
            }

            units.clear();
        }

        return;
    }

    // If the enemy has us outnumbered by more than one unit, rally all of our workers
    auto squadUnits = defendBaseSquad->getUnits();
    if (enemyUnits.size() - squadUnits.size() > 1)
    {
        executeFullWorkerDefense(enemyUnits, squadUnits);
        return;
    }

    // There is no serious threat, so just attack with workers that are not actively mining and are close to their potential targets
    auto baseWorkers = Workers::getBaseWorkers(base);

    // Start by handling the units already reserved: release them if they don't have a valid target, attack if they do
    for (auto it = units.begin(); it != units.end();)
    {
        auto target = getTarget(*it, enemyUnits, false, 48);
        if (target)
        {
            (*it)->attack(target->bwapiUnit);
            it++;
        }
        else
        {
            Workers::releaseWorker(*it);
            it = units.erase(it);
        }
    }

    // Now check if it makes sense to grab any of the other workers
    for (auto &worker : baseWorkers)
    {
        // Never grab a worker that is actively mining
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
            worker->bwapiUnit->getOrder() == BWAPI::Orders::HarvestGas)
        {
            continue;
        }

        auto target = getTarget(worker, enemyUnits, false, 32);
        if (target)
        {
            Workers::reserveWorker(worker);
            units.push_back(worker);
            worker->attack(target->bwapiUnit);
        }
    }
}

void WorkerDefenseSquad::disband()
{
    for (const auto &unit : units)
    {
        Workers::releaseWorker(unit);
    }

    units.clear();
}

void WorkerDefenseSquad::executeFullWorkerDefense(std::set<Unit> &enemyUnits, const std::vector<MyUnit> &defendBaseUnits)
{
    // Grab all of the base's workers
    Workers::reserveBaseWorkers(units, base);

    // Choose a target for each of our workers
    // Each attacks the closest enemy unit with the least health
    bool isEnemyInRange = false;
    std::vector<std::pair<MyUnit, Unit>> workersAndTargets;
    for (const auto &worker : units)
    {
        Unit bestTarget = getTarget(worker, enemyUnits);
        workersAndTargets.emplace_back(std::make_pair(worker, bestTarget));

        auto predictedPosition = bestTarget->predictPosition(BWAPI::Broodwar->getLatencyFrames());
        if (worker->isInOurWeaponRange(bestTarget, predictedPosition) ||
            worker->isInEnemyWeaponRange(bestTarget, predictedPosition))
        {
            isEnemyInRange = true;
        }
    }

    // Consider if any combat units are in range of an enemy
    if (!isEnemyInRange)
    {
        for (const auto &unit : defendBaseUnits)
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
    auto rallyPatch = base->workerDefenseRallyPatch;
    if (rallyPatch)
    {
        for (const auto &worker : units)
        {
            worker->gather(rallyPatch);
        }
    }
}
