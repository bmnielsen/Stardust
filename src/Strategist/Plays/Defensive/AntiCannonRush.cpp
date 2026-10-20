#include "AntiCannonRush.h"

#include "Workers.h"
#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Opponent.h"

#include "DebugFlag_UnitOrders.h"

/*
 * This play is added by the strategy engine when the enemy could be doing a cannon rush - either we have no
 * scouting information (scout hasn't arrived or enemy blocked it), have scouted an early forge, or have scouted
 * a probable proxy.
 *
 * There are various states the play can be in:
 * - Scouting our main
 *   - When we haven't observed any activity in our base yet
 * - Following enemy worker
 *   - When we have seen an enemy worker, but nothing dangerous yet
 * - Attacking enemy worker
 *   - When we have seen an enemy worker build something
 * - Attacking building
 *   - When there is a building we need to kill quickly, e.g. a warping cannon
 *
 * This play handles attacking with workers where needed and ordering zealots when required, but the actual
 * combat micro of the zealots is handled by the main base defense play.
 */

namespace
{
    void clearDead(std::set<OwnedWorker> &workers)
    {
        for (auto iterRef = workers.begin(); iterRef != workers.end();)
        {
            if ((*iterRef)->exists())
            {
                iterRef++;
            }
            else
            {
                iterRef = workers.erase(iterRef);
            }
        }
    }

    void releaseAll(std::set<OwnedWorker> &workers)
    {
        for (auto &wrk : workers)
        {
            Telemetry::log(wrk->ident) << "Releasing from non-mining duties (AntiCannonRush no longer needs it)";
            WorkerMgr::releaseWorker(wrk);
        }

        workers.clear();
    }
}

AntiCannonRush::AntiCannonRush()
        : PlayItem("AntiCannonRush")
        , safeEnemyStrategyDetermined(false)
        , scout(nullptr)
        , builtPylon(false)
        , builtCannon(false)
{
    /* Compute the scout tiles */
    auto areas = Map::getMyMainAreas();
    auto referenceHeight = BWAPI::Broodwar->getGroundHeight(Map::getMyMain()->getTilePosition());
    for (int y = 0; y < BWAPI::Broodwar->mapHeight() - 1; y++)
    {
        for (int x = 0; x < BWAPI::Broodwar->mapWidth() - 1; x++)
        {
            if (!Map::isWalkable(x, y) ||
                !Map::isWalkable(x + 1, y) ||
                !Map::isWalkable(x, y + 1) ||
                !Map::isWalkable(x + 1, y + 1))
            {
                continue;
            }

            BWAPI::TilePosition here(x, y);
            if (BWAPI::Broodwar->getGroundHeight(here) != referenceHeight) continue;
            if (!areas.contains(BWEM::Map::Instance().GetArea(here))) continue;

            tilesToScout.push_back(here);
        }
    }
}

void AntiCannonRush::update()
{
    /* Clear scout if it has died */
    if (scout && !scout->exists())
    {
        scout = nullptr;
    }

    /* Clear worker attackers that have died */
    clearDead(workerAttackers);

    /* Clear enemy cannons if they are dead */
    for (auto iterRef = cannonsAndAttackers.begin(); iterRef != cannonsAndAttackers.end();)
    {
        /* Clear cannon attackers that have died */
        clearDead(iterRef->second);

        if (iterRef->first->exists())
        {
            iterRef++;
        }
        else
        {
            releaseAll(iterRef->second);
            iterRef = cannonsAndAttackers.erase(iterRef);
        }
    }

    /* Collect all enemy units in our main areas */
    auto &areas = Map::getMyMainAreas();
    std::set<Unit> workers;
    std::set<Unit> pylons;
    for (const auto &gameUnit : UnitMgr::allEnemy())
    {
        if (!gameUnit->lastPositionValid) continue;
        if (!gameUnit->type.isWorker() &&
            gameUnit->type != BWAPI::UnitTypes::Protoss_Pylon &&
            gameUnit->type != BWAPI::UnitTypes::Protoss_Photon_Cannon)
        {
            continue;
        }

        if (!areas.contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos)))) continue;

        if (gameUnit->type.isWorker())
        {
            workers.insert(gameUnit);
        }
        else if (gameUnit->type == BWAPI::UnitTypes::Protoss_Pylon)
        {
            pylons.insert(gameUnit);

            if (!builtPylon)
            {
                AppLog::Get() << "Found enemy pylon in our base";
                builtPylon = true;

                int startFrame = (gameUnit->completed ? frameNow : gameUnit->estimatedCompletionFrame)
                        - UnitHelper::BuildTime(gameUnit->type);
                Opponent::setGameValue("pylonInOurMain", startFrame);
            }
        }
        else if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
        {
            if (!cannonsAndAttackers.contains(gameUnit))
            {
                cannonsAndAttackers[gameUnit] = {};
                builtCannon = true;
                AppLog::Get() << "Found enemy cannon in our base";

                /* Built cannon implies built pylon, even if we haven't seen it */
                if (!builtPylon)
                {
                    builtPylon = true;
                    int startFrame = (gameUnit->completed ? frameNow : gameUnit->estimatedCompletionFrame)
                                     - UnitHelper::BuildTime(gameUnit->type)
                                     - UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);
                    Opponent::setGameValue("pylonInOurMain", startFrame);
                }
            }
        }
    }

    /* Disband when we are fairly certain the cannon rush is not happening or is over */
    if (((frameNow >= 4000 || safeEnemyStrategyDetermined) && !builtPylon) ||
        (frameNow >= 7000 && pylons.empty() && cannonsAndAttackers.empty()))
    {
        status.complete = true;
        return;
    }

    /* Use our previous game results to determine when the play should go "active", i.e. reserve a worker */
    /* We start our scouting 500 frames before we've previously seen a pylon start */
    /* An exception is if we have never lost to this opponent, in which case we play it safe */
    if (Opponent::winLossRatio(0.0, 200) < 0.99)
    {
        int worstCasePylonFrame =
                Opponent::minValueInPreviousGames("pylonInOurMain", INT_MAX, 15);
        if (!builtPylon && worstCasePylonFrame > (frameNow + 500))
        {
            return;
        }
    }

    /* Gather units that should attack */
    std::vector<std::pair<OwnedUnit, Unit>> unitsAndTargets;

    /* Attack the enemy worker if needed */
    if (workers.size() == 1)
    {
        auto &wrk = *workers.begin();

        /* Take the scout if available */
        if (scout)
        {
            workerAttackers.insert(scout);
            scout = nullptr;
        }

        while (workerAttackers.size() < ((!pylons.empty() || !cannonsAndAttackers.empty()) ? 2 : 1))
        {
            auto gameUnit = WorkerMgr::getClosestReassignableWorker(wrk->prevPos, true);
            if (!gameUnit) break;

            WorkerMgr::reserveWorker(gameUnit);
            workerAttackers.insert(gameUnit);
        }

        for (auto &workerAttacker : workerAttackers)
        {
            unitsAndTargets.emplace_back(workerAttacker, wrk);
        }
    }
    else
    {
        releaseAll(workerAttackers);
    }

    /* Reserve attackers for enemy cannons */
    auto completionCutoff = frameNow + UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon) - 100;
    for (auto &cannonAndAttackers : cannonsAndAttackers)
    {
        auto &cannon = cannonAndAttackers.first;

        /* Reserve attackers if we have just seen it start to build */
        if (cannonAndAttackers.second.size() < 4 &&
            cannon->estimatedCompletionFrame > completionCutoff)
        {
            while (cannonAndAttackers.second.size() < 4)
            {
                auto gameUnit = WorkerMgr::getClosestReassignableWorker(cannon->prevPos, true);
                if (!gameUnit) break;

                WorkerMgr::reserveWorker(gameUnit);
                cannonAndAttackers.second.insert(gameUnit);
            }
        }

        for (auto &cannonAttacker : cannonAndAttackers.second)
        {
            unitsAndTargets.emplace_back(cannonAttacker, cannon);
        }
    }

    /* Issue attack orders */
    if (!unitsAndTargets.empty())
    {
        for (auto &unitAndTarget : unitsAndTargets)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(unitAndTarget.first->ident) << "Attacking " << *unitAndTarget.second;
#endif
            unitAndTarget.first->attackUnit(unitAndTarget.second, unitsAndTargets);
        }
        return;
    }

    /* Get the next tile to scout */
    auto tilePos = getNextScoutTile();

    /* If there is none, we are done for now */
    if (tilePos == BWAPI::TilePositions::Invalid)
    {
        if (scout)
        {
            Telemetry::log(scout->ident) << "Releasing from non-mining duties (AntiCannonRush no tiles to scout)";
            WorkerMgr::releaseWorker(scout);
        }
        return;
    }

    /* Ensure we have a worker scout */
    if (!scout)
    {
        scout = WorkerMgr::getClosestReassignableWorker(BWAPI::Position(tilePos) + BWAPI::Position(16, 16), false);
        if (!scout) return;

        WorkerMgr::reserveWorker(scout);
    }

#if DEBUG_UNIT_ORDERS
    Telemetry::log(scout->ident) << "Move to next AntiCannonRush tile @ " << (BWAPI::WalkPosition(tilePos) + BWAPI::WalkPosition(2, 2));
#endif
    scout->moveTo(BWAPI::Position(tilePos) + BWAPI::Position(16, 16));
}

void AntiCannonRush::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* If the enemy has cannons, build zealots at emergency priority */
    if (!cannonsAndAttackers.empty())
    {
        prioProdGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                    label,
                                                                    BWAPI::UnitTypes::Protoss_Zealot,
                                                                    -1,
                                                                    2);
    }
}

void AntiCannonRush::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                             const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    if (scout && scout->exists())
    {
        Telemetry::log(scout->ident) << "Releasing from non-mining duties (AntiCannonRush disband)";
        WorkerMgr::releaseWorker(scout);
    }

    releaseAll(workerAttackers);

    for (auto &cannonAndAttackers : cannonsAndAttackers)
    {
        releaseAll(cannonAndAttackers.second);
    }
}

BWAPI::TilePosition AntiCannonRush::getNextScoutTile()
{
    auto mainCenter = Map::getMyMain()->getPosition();

    int bestFrame = INT_MAX;
    int bestDist = INT_MAX;
    auto bestTile = BWAPI::TilePositions::Invalid;
    for (const auto &tilePos : tilesToScout)
    {
        int lastSeen = Map::lastSeen(tilePos);
        if (lastSeen > bestFrame) continue;

        int span = mainCenter.getApproxDistance(BWAPI::Position(tilePos) + BWAPI::Position(16, 16));
        if (scout) span += scout->prevPos.getApproxDistance(BWAPI::Position(tilePos) + BWAPI::Position(16, 16));
        if (lastSeen < bestFrame || span < bestDist)
        {
            bestTile = tilePos;
            bestFrame = lastSeen;
            bestDist = span;
        }
    }

    return bestTile;
}
