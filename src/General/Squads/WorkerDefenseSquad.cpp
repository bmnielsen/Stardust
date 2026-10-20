#include "WorkerDefenseSquad.h"

#include "Workers.h"
#include "Geo.h"
#include "BuildingPlacement.h"
#include "Units.h"

#include "DebugFlag_UnitOrders.h"

std::vector<std::pair<OwnedWorker, Unit>> WorkerDefenseSquad::selectTargets(std::set<Unit> &enemyUnits)
{
    std::vector<std::pair<OwnedWorker, Unit>> outcome;

    bool hasCannonInMineralLine = false;
    auto &defenseLocations = BuildingPlacement::baseStaticDefenseLocations(baseLoc);
    if (defenseLocations.isValid())
    {
        auto cannon = UnitMgr::myBuildingAt(*defenseLocations.workerDefenseCannons.begin());
        if (cannon && cannon->completed && cannon->rawUnit->isPowered())
        {
            hasCannonInMineralLine = true;
        }
    }

    auto selectTarget = [&enemyUnits, &hasCannonInMineralLine](OwnedWorker &wrk)
    {
        Unit closestEnemy = nullptr;
        int closestEnemyDist = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            if (!wrk->canAttack(enemy)) continue;
            if (!wrk->isInEnemyWeaponRange(enemy, hasCannonInMineralLine ? 0 : 48)) continue;

            int span = wrk->getDistance(enemy);
            if (span < closestEnemyDist)
            {
                closestEnemy = enemy;
                closestEnemyDist = span;
            }
        }

        return closestEnemy;
    };

    /* For the list of units we've already reserved, remove the dead ones and release units that no longer have a target */
    for (auto iterRef = units.begin(); iterRef != units.end();)
    {
        auto &wrk = *iterRef;

        /* Dead units */
        if (!wrk->exists())
        {
            iterRef = units.erase(iterRef);
            continue;
        }

        auto target = selectTarget(wrk);

        /* Units not being threatened */
        if (!target)
        {
            Telemetry::log(wrk->ident) << "Releasing from non-mining duties (not threatened)";
            WorkerMgr::releaseWorker(wrk);
            iterRef = units.erase(iterRef);
            continue;
        }

        outcome.emplace_back(wrk, target);
        iterRef++;
    }

    /* For units not yet reserved, reserve those that are being threatened */
    auto baseWorkers = WorkerMgr::getBaseWorkers(baseLoc);
    for (auto &wrk : baseWorkers)
    {
        auto target = selectTarget(wrk);
        if (target)
        {
            WorkerMgr::reserveWorker(wrk);
            units.push_back(wrk);
            outcome.emplace_back(wrk, target);
        }
    }

    return outcome;
}

void WorkerDefenseSquad::execute(std::vector<std::pair<OwnedWorker, Unit>> &workersAndTargets,
                                 std::vector<std::pair<OwnedUnit, Unit>> &combatUnitsAndTargets)
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

    /* Execute the micro for each worker with a target */
    /* Rules are: */
    /* - Get a mineral patch we can use for fleeing or kiting */
    /* - If there is no suitable patch, attack */
    /* - If we are on cooldown, mine from the patch (currently disabled) */
    /* - If we are low on health, mine from the patch */
    /* - If a friendly combat unit is moving to attack the target but is not yet in position, mine from the patch */
    /* - Otherwise attack */
    for (auto &workerAndTarget : workersAndTargets)
    {
        auto &wrk = workerAndTarget.first;
        auto &target = workerAndTarget.second;

        if (!wrk->exists()) continue;

        /* Select a mineral patch to use */
        /* We pick the patch that is furthest away, but closer to us than the enemy */
        BWAPI::Unit furthestPatch = nullptr;
        int furthestPatchDist = 0;
        for (const auto &mineralPatch : baseLoc->mineralPatches())
        {
            auto patch = mineralPatch->getBwapiUnitIfVisible();
            if (!patch) continue;

            int span = Geom::EdgeToEdgeDistance(wrk->type, wrk->prevPos, patch->getType(), patch->getPosition());
            if (span < 10) continue;
            if (span < furthestPatchDist) continue;

            int targetDistToPatch = Geom::EdgeToEdgeDistance(target->type, target->simPosition, patch->getType(), patch->getPosition());
            if (targetDistToPatch < span) continue;

            furthestPatch = patch;
            furthestPatchDist = span;
        }
        if (!furthestPatch)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(wrk->ident) << "No flee patch; attacking: " << target->type << " @ "
                                       << BWAPI::WalkPosition(target->simPosition);
#endif
            wrk->attack(target->rawUnit, true);
            continue;
        }

        /* Flee if we just attacked */
        if (wrk->lastSeenAttacking >= (frameNow - 1))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(wrk->ident) << "Just attacked, moving to patch: " << BWAPI::WalkPosition(furthestPatch->getPosition());
#endif
            wrk->gather(furthestPatch);
            continue;
        }

        /* Flee if we are low on health */
        if ((wrk->health + wrk->shields) <= healthLimit)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(wrk->ident) << "Low on health, moving to patch: " << BWAPI::WalkPosition(furthestPatch->getPosition());
#endif
            wrk->gather(furthestPatch);
            continue;
        }

        /* Check if there is a combat unit attacking our target */
        /* If there is, and it isn't close to being in range yet, flee to give it time to get here */
        /* Don't do this if we are in the target's attack range */
        if (!wrk->isInEnemyWeaponRange(target))
        {
            bool combatUnitApproaching = false;
            for (auto &combatUnitAndTarget : combatUnitsAndTargets)
            {
                if (combatUnitAndTarget.second != target) continue;

                if (combatUnitAndTarget.first->isInOurWeaponRange(combatUnitAndTarget.second, 16))
                {
                    /* There is a unit in range, so clear and break */
                    combatUnitApproaching = false;
                    break;
                }

                combatUnitApproaching = true;
            }
            if (combatUnitApproaching)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(wrk->ident) << "Wait for combat unit, moving to patch: " << BWAPI::WalkPosition(furthestPatch->getPosition());
#endif
                wrk->gather(furthestPatch);
                continue;
            }
        }

        /* When on cooldown, kite if we are too close to our target */
        if ((wrk->cooldownUntil - frameNow) > (BWAPI::Broodwar->getRemainingLatencyFrames() + 6) &&
            wrk->getDistance(target) < (wrk->groundRange() - 2))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(wrk->ident) << "Kiting from target @ " << BWAPI::WalkPosition(target->simPosition)
                                       << "; d=" << wrk->getDistance(target)
                                       << "; moving to patch: " << BWAPI::WalkPosition(furthestPatch->getPosition());
#endif
            wrk->gather(furthestPatch);
            continue;
        }

#if DEBUG_UNIT_ORDERS
        Telemetry::log(wrk->ident) << "Attacking: " << target->type << " @ "
                                   << BWAPI::WalkPosition(target->simPosition);
#endif
        wrk->attack(target->rawUnit, true);
        /* worker->attackUnit(target, workersAndTargets, false); */
    }
}

void WorkerDefenseSquad::disband()
{
    for (const auto &gameUnit : units)
    {
        Telemetry::log(gameUnit->ident) << "Releasing from non-mining duties (disband worker defense)";
        WorkerMgr::releaseWorker(gameUnit);
    }

    units.clear();
}
