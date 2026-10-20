#include "TakeIslandExpansion.h"

#include "Builder.h"
#include "Workers.h"
#include "Units.h"
#include "Geo.h"
#include "Map.h"
#include "Players.h"

namespace
{
    void unloadNear(OwnedUnit &shuttle, BWAPI::Position target)
    {
        /* Ensure the target is walkable */
        auto tilePos = BWAPI::TilePosition(target);
        auto spiral = Geom::Spiral();
        while (!Map::isWalkable(tilePos))
        {
            spiral.Next();
            tilePos = BWAPI::TilePosition(target) + BWAPI::TilePosition(spiral.x, spiral.y);
            if (!tilePos.isValid())
            {
                tilePos = BWAPI::TilePosition(target);
                break;
            }
        }

        auto pt = BWAPI::Position(tilePos) + BWAPI::Position(16, 16);
        if (shuttle->getDistance(pt) < 128)
        {
            if (shuttle->rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Unload_All_Position) return;
            shuttle->unloadAll(pt);
        }
        else
        {
            shuttle->moveTo(pt);
        }
    }
}

TakeIslandExpansion::TakeIslandExpansion(Base *baseLoc, bool canCancel, bool transferWorkers)
        : TakeExpansion(baseLoc, 0)
        , canCancel(canCancel)
        , shuttle(nullptr)
        , workerTransferState(transferWorkers ? 0 : 2) {}

void TakeIslandExpansion::update()
{
    auto nexus = UnitMgr::myBuildingAt(depotPosition);

    /* Clean up dead or unneeded workers */
    if (builder && !builder->exists()) builder = nullptr;
    for (auto iterRef = workerTransfer.begin(); iterRef != workerTransfer.end();)
    {
        auto &wrk = *iterRef;
        if (wrk->exists() && (wrk->rawUnit->isLoaded() || wrk->getDistance(baseLoc->getPosition()) > 300))
        {
            iterRef++;
        }
        else
        {
            /* The worker has been unloaded near the island base, so unreserve it */
            if (wrk->exists()) WorkerMgr::releaseWorker(wrk);
            iterRef = workerTransfer.erase(iterRef);
        }
    }

    /* If the shuttle dies, we usually want to cancel the play */
    if (shuttle && !shuttle->exists())
    {
        shuttle = nullptr;

        if (!nexus && (!builder || builder->getDistance(baseLoc->getPosition()) > 300))
        {
            status.complete = true;
            return;
        }
    }

    /* Handle the case where we've discovered the expansion is already taken by the opponent */
    if (baseLoc->owner == BWAPI::Broodwar->enemy())
    {
        /* If the builder is loaded in the shuttle, tell the shuttle to unload it near our main */
        if (shuttle && builder && builder->rawUnit->isLoaded())
        {
            unloadNear(shuttle, Map::getMyMain()->mineralLineCenter);
            return;
        }

        /* Otherwise treat the play as complete */
        status.complete = true;
        return;
    }

    if (!nexus)
    {
        if (!shuttle)
        {
            status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Shuttle, Map::getMyMain()->getPosition());
            return;
        }

        if (!builder)
        {
            builder = WorkerMgr::getClosestReassignableWorker(shuttle->prevPos, false);
            if (!builder) return;

            WorkerMgr::reserveWorker(builder);
            builder->moveTo(shuttle->prevPos);
            shuttle->load(builder->rawUnit);
        }

        if (builder->rawUnit->isLoaded())
        {
            unloadNear(shuttle, baseLoc->getPosition());
            return;
        }

        /* Jump out now if the shuttle hasn't loaded the builder yet */
        if (builder->getDistance(baseLoc->getPosition()) > 128) return;

        /* The shuttle has transferred the builder and doesn't need to transfer workers, release it */
        if (shuttle && workerTransferState == 2)
        {
            status.removedUnits.push_back(shuttle);
        }

        /* Ensure the builder clears a blocking neutral */
        for (const auto &blockingNeutral : baseLoc->blockingNeutrals)
        {
            if (!blockingNeutral->exists()) continue;

            if (blockingNeutral->getType().isMineralField())
            {
                /* No need to do anything if the builder is gathering */
                if (builder->rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Gather &&
                    builder->rawUnit->getLastCommand().getTarget() == blockingNeutral)
                    return;

                builder->gather(blockingNeutral);
            }
            else
            {
                /* No need to do anything if the builder is attacking */
                if (builder->rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
                    builder->rawUnit->getLastCommand().getTarget() == blockingNeutral)
                    return;

                builder->attack(blockingNeutral);
            }

            return;
        }

        Constructor::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
    }
    else
    {
        if (builder)
        {
            WorkerMgr::releaseWorker(builder);
            builder = nullptr;
        }

        if (nexus->completed && !shuttle)
        {
            status.complete = true;
        }
    }

    /* Have the shuttle transfer some probes from a nearby base */
    if (shuttle)
    {
        switch (workerTransferState)
        {
            case 0:
            {
                /* Find the base to transfer from */
                Base *bestBase = nullptr;
                int bestScore = INT_MAX;
                for (auto &b : Map::getMyBases())
                {
                    if (b->island) continue;

                    int availableWorkers = WorkerMgr::baseMineralWorkerCount(b);
                    if (availableWorkers < 8) continue;

                    int score = shuttle->getDistance(b->getPosition()) - availableWorkers * 200;
                    if (score < bestScore)
                    {
                        bestScore = score;
                        bestBase = b;
                    }
                }

                if (!bestBase) break;

                /* First ensure the shuttle has arrived at the base */
                if (workerTransfer.empty() && shuttle->getDistance(bestBase->mineralLineCenter) > 320)
                {
                    shuttle->moveTo(bestBase->mineralLineCenter);
                    break;
                }

                /* Now wait until the nexus is sufficiently complete */
                /* We try to approximately handle transit time + 300 frames to load & unload the workers */
                if (!nexus) break;
                if (!nexus->completed && workerTransfer.empty())
                {
                    int frames = 600 +
                                 Pathfinder::ExpectedTravelTime(baseLoc->getPosition(), bestBase->getPosition(), BWAPI::UnitTypes::Protoss_Shuttle);
                    if (nexus->estimatedCompletionFrame - frameNow > frames)
                    {
                        break;
                    }
                }

                /* Then reserve the appropriate number of workers */
                int desiredWorkers = std::min(8, (int) (workerTransfer.size() + WorkerMgr::baseMineralWorkerCount(bestBase)) / 2);
                while (desiredWorkers > workerTransfer.size())
                {
                    auto wrk = WorkerMgr::getClosestReassignableWorker(shuttle->prevPos, false);
                    if (!wrk || shuttle->getDistance(wrk) > 500) break;

                    WorkerMgr::reserveWorker(wrk);
                    workerTransfer.push_back(wrk);
                }

                /* Then load all of the workers */
                bool allLoaded = true;
                for (auto &wrk : workerTransfer)
                {
                    if (!wrk->rawUnit->isLoaded())
                    {
                        wrk->moveTo(shuttle->prevPos);

                        /* Load the first not-loaded worker */
                        if (allLoaded)
                        {
                            shuttle->load(wrk->rawUnit);
                        }

                        allLoaded = false;
                    }
                }

                /* State transition when all desired workers are loaded */
                if (allLoaded && workerTransfer.size() == desiredWorkers)
                {
                    workerTransferState = 1;
                }

                break;
            }
            case 1:
            {
                unloadNear(shuttle, baseLoc->mineralLineCenter);

                /* State transition when all workers are unloaded */
                bool anyLoaded = false;
                for (const auto &wrk : workerTransfer)
                {
                    if (wrk->rawUnit->isLoaded())
                    {
                        anyLoaded = true;
                        break;
                    }
                }

                if (!anyLoaded)
                {
                    status.removedUnits.push_back(shuttle);
                    workerTransferState = 2;
                }

                break;
            }
            default:
                break;
        }
    }
}

void TakeIslandExpansion::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                                  const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    /* TODO: Avoid stranding the worker on the island */

    if (builder) WorkerMgr::releaseWorker(builder);
    if (shuttle) removedUnitCallback(shuttle);
}

void TakeIslandExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;

        /* We only build a shuttle if we don't already have one */
        /* If we have one, we'll wait until it is available for island expo duties */
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Shuttle) continue;
        if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Shuttle) > 0) continue;

        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

void TakeIslandExpansion::addUnit(const OwnedUnit &gameUnit)
{
    if (gameUnit->type == BWAPI::UnitTypes::Protoss_Shuttle) shuttle = gameUnit;
}

void TakeIslandExpansion::removeUnit(const OwnedUnit &gameUnit)
{
    if (shuttle == gameUnit) shuttle = nullptr;
}

bool TakeIslandExpansion::cancellable()
{
    if (!canCancel) return false;
    if (!shuttle) return true;
    if (builder) return false;
    return workerTransfer.empty();
}

int TakeIslandExpansion::framesToClearBlocker()
{
    for (const auto &blockingNeutral : baseLoc->blockingNeutrals)
    {
        if (!blockingNeutral->exists()) continue;

        if (blockingNeutral->getType().isMineralField()) return 0;

        int hp = blockingNeutral->getInitialHitPoints();
        if (blockingNeutral->isVisible())
        {
            hp = blockingNeutral->getHitPoints();
        }

        return (hp * BWAPI::UnitTypes::Protoss_Probe.groundWeapon().damageCooldown())
               / PlayerInfo::attackDamage(BWAPI::Broodwar->self(),
                                       BWAPI::UnitTypes::Protoss_Probe,
                                       blockingNeutral->getPlayer(),
                                       blockingNeutral->getType());
    }

    return 0;
}