#include "WorkerDefenseSquad.h"

#include "UnitUtil.h"
#include "Workers.h"
#include "Geo.h"

#include "DebugFlag_UnitOrders.h"

std::vector<std::pair<MyUnit, Unit>> WorkerDefenseSquad::selectTargets(std::set<Unit> &enemyUnits)
{
    std::vector<std::pair<MyUnit, Unit>> result;

    auto selectTarget = [&enemyUnits](MyUnit &worker)
    {
        Unit closestEnemy = nullptr;
        int closestEnemyDist = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            if (!worker->isInEnemyWeaponRange(enemy, 48)) continue;

            int dist = worker->getDistance(enemy);
            if (dist < closestEnemyDist)
            {
                closestEnemy = enemy;
                closestEnemyDist = dist;
            }
        }

        return closestEnemy;
    };

    // For the list of units we've already reserved, remove the dead ones and release units that no longer have a target
    for (auto it = units.begin(); it != units.end();)
    {
        auto &worker = *it;

        // Dead units
        if (!worker->exists())
        {
            it = units.erase(it);
            continue;
        }

        auto target = selectTarget(worker);

        // Units not being threatened
        if (!target)
        {
            CherryVis::log(worker->id) << "Releasing from non-mining duties (not threatened)";
            Workers::releaseWorker(worker);
            it = units.erase(it);
            continue;
        }

        result.emplace_back(std::make_pair(worker, target));
        it++;
    }

    // For units not yet reserved, reserve those that are being threatened
    auto baseWorkers = Workers::getBaseWorkers(base);
    for (auto &worker : baseWorkers)
    {
        auto target = selectTarget(worker);
        if (target)
        {
            Workers::reserveWorker(worker);
            units.push_back(worker);
            result.emplace_back(std::make_pair(worker, target));
        }
    }

    return result;
}

void WorkerDefenseSquad::execute(std::vector<std::pair<MyUnit, Unit>> &workersAndTargets,
                                 std::vector<std::pair<MyUnit, Unit>> &combatUnitsAndTargets)
{
    auto healthLimit = 10;
    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
    {
        healthLimit = 12;
    }
    else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
    {
        healthLimit = 16;
    }

    // Execute the micro for each worker with a target
    // Rules are:
    // - Get a mineral patch we can use for fleeing or kiting
    // - If there is no suitable patch, attack
    // - If we are on cooldown, mine from the patch (currently disabled)
    // - If we are low on health, mine from the patch
    // - If a friendly combat unit is moving to attack the target but is not yet in position, mine from the patch
    // - Otherwise attack
    for (auto &workerAndTarget : workersAndTargets)
    {
        auto &worker = workerAndTarget.first;
        auto &target = workerAndTarget.second;

        // Select a mineral patch to use
        // We pick the patch that is furthest away, but closer to us than the enemy
        BWAPI::Unit furthestPatch = nullptr;
        int furthestPatchDist = 0;
        for (auto patch : base->mineralPatches())
        {
            if (!patch->exists()) continue;
            if (!patch->isVisible()) continue;

            int dist = Geo::EdgeToEdgeDistance(worker->type, worker->lastPosition, patch->getType(), patch->getPosition());
            if (dist < furthestPatchDist) continue;

            int targetDistToPatch = Geo::EdgeToEdgeDistance(target->type, target->lastPosition, patch->getType(), patch->getPosition());
            if (targetDistToPatch < dist) continue;

            furthestPatch = patch;
            furthestPatchDist = dist;
        }
        if (!furthestPatch)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(worker->id) << "No flee patch; attacking: " << target->type << " @ "
                                       << BWAPI::WalkPosition(target->lastPosition);
#endif
            worker->attackUnit(target, workersAndTargets, false);
            continue;
        }

        // Attack if we are so close to the patch that we can't flee
        if (furthestPatchDist < 10)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(worker->id) << "Close to patch; attacking: " << target->type << " @ "
                                       << BWAPI::WalkPosition(target->lastPosition);
#endif
            worker->attackUnit(target, workersAndTargets, false);
            continue;
        }

        // Flee if we are low on health
        if ((worker->health + worker->shields) <= healthLimit)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(worker->id) << "Low on health, moving to patch: " << BWAPI::WalkPosition(furthestPatch->getPosition());
#endif
            worker->gather(furthestPatch);
            continue;
        }

        // When on cooldown, kite if we are too close to our target
        if ((worker->cooldownUntil - BWAPI::Broodwar->getFrameCount()) > (BWAPI::Broodwar->getRemainingLatencyFrames() + 3) &&
            worker->getDistance(target) < (worker->groundRange() - 2))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(worker->id) << "Kiting from target @ " << BWAPI::WalkPosition(target->lastPosition)
                                       << "; d=" << worker->getDistance(target)
                                       << "; moving to patch: " << BWAPI::WalkPosition(furthestPatch->getPosition());
#endif
            worker->gather(furthestPatch);
            continue;
        }

        // Check if there is a combat unit attacking our target
        // If there is, and it isn't close to being in range yet, flee to give it time to get here
        bool combatUnitApproaching = false;
        for (auto &combatUnitAndTarget : combatUnitsAndTargets)
        {
            if (combatUnitAndTarget.second != target) continue;

            if (combatUnitAndTarget.first->isInOurWeaponRange(combatUnitAndTarget.second, 16))
            {
                // There is a unit in range, so clear and break
                combatUnitApproaching = false;
                break;
            }

            combatUnitApproaching = true;
        }
        if (combatUnitApproaching)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(worker->id) << "Wait for combat unit, moving to patch: " << BWAPI::WalkPosition(furthestPatch->getPosition());
#endif
            worker->gather(furthestPatch);
            continue;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(worker->id) << "Attacking: " << target->type << " @ "
                                   << BWAPI::WalkPosition(target->lastPosition);
#endif
        worker->attackUnit(target, workersAndTargets, false);
    }
}

void WorkerDefenseSquad::disband()
{
    for (const auto &unit : units)
    {
        CherryVis::log(unit->id) << "Releasing from non-mining duties (disband worker defense)";
        Workers::releaseWorker(unit);
    }

    units.clear();
}
