#include "TakeIslandExpansion.h"

#include "Builder.h"
#include "Workers.h"
#include "Units.h"
#include "Geo.h"
#include "Map.h"
#include "Players.h"

namespace
{
    void unloadNear(MyUnit &shuttle, BWAPI::Position target)
    {
        // Ensure the target is walkable
        auto tile = BWAPI::TilePosition(target);
        auto spiral = Geo::Spiral();
        while (!Map::isWalkable(tile))
        {
            spiral.Next();
            tile = BWAPI::TilePosition(target) + BWAPI::TilePosition(spiral.x, spiral.y);
            if (!tile.isValid())
            {
                tile = BWAPI::TilePosition(target);
                break;
            }
        }

        auto pos = BWAPI::Position(tile) + BWAPI::Position(16, 16);
        if (shuttle->getDistance(pos) < 128)
        {
            if (shuttle->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Unload_All_Position) return;
            shuttle->unloadAll(pos);
        }
        else
        {
            shuttle->moveTo(pos);
        }
    }
}

TakeIslandExpansion::TakeIslandExpansion(Base *base, bool canCancel, bool transferWorkers)
        : TakeExpansion(base, 0)
        , canCancel(canCancel)
        , shuttle(nullptr)
        , workerTransferState(transferWorkers ? 0 : 2) {}

void TakeIslandExpansion::update()
{
    auto nexus = Units::myBuildingAt(depotPosition);

    // Clean up dead or unneeded workers
    if (builder && !builder->exists()) builder = nullptr;
    for (auto it = workerTransfer.begin(); it != workerTransfer.end();)
    {
        auto &worker = *it;
        if (worker->exists() && (worker->bwapiUnit->isLoaded() || worker->getDistance(base->getPosition()) > 300))
        {
            it++;
        }
        else
        {
            // The worker has been unloaded near the island base, so unreserve it
            if (worker->exists()) Workers::releaseWorker(worker);
            it = workerTransfer.erase(it);
        }
    }

    // If the shuttle dies, we usually want to cancel the play
    if (shuttle && !shuttle->exists())
    {
        shuttle = nullptr;

        if (!nexus && (!builder || builder->getDistance(base->getPosition()) > 300))
        {
            status.complete = true;
            return;
        }
    }

    // Handle the case where we've discovered the expansion is already taken by the opponent
    if (base->owner == BWAPI::Broodwar->enemy())
    {
        // If the builder is loaded in the shuttle, tell the shuttle to unload it near our main
        if (shuttle && builder && builder->bwapiUnit->isLoaded())
        {
            unloadNear(shuttle, Map::getMyMain()->mineralLineCenter);
            return;
        }

        // Otherwise treat the play as complete
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
            builder = Workers::getClosestReassignableWorker(shuttle->lastPosition, false);
            if (!builder) return;

            Workers::reserveWorker(builder);
            builder->moveTo(shuttle->lastPosition);
            shuttle->load(builder->bwapiUnit);
        }

        if (builder->bwapiUnit->isLoaded())
        {
            unloadNear(shuttle, base->getPosition());
            return;
        }

        // Jump out now if the shuttle hasn't loaded the builder yet
        if (builder->getDistance(base->getPosition()) > 128) return;

        // The shuttle has transferred the builder and doesn't need to transfer workers, release it
        if (shuttle && workerTransferState == 2)
        {
            status.removedUnits.push_back(shuttle);
        }

        // Ensure the builder clears a blocking neutral
        if (base->blockingNeutral && base->blockingNeutral->exists())
        {
            if (base->blockingNeutral->getType().isMineralField())
            {
                // No need to do anything if the builder is gathering
                if (builder->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Gather &&
                    builder->bwapiUnit->getLastCommand().getTarget() == base->blockingNeutral)
                    return;

                builder->gather(base->blockingNeutral);
            }
            else
            {
                // No need to do anything if the builder is attacking
                if (builder->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
                    builder->bwapiUnit->getLastCommand().getTarget() == base->blockingNeutral)
                    return;

                builder->attack(base->blockingNeutral);
            }

            return;
        }

        Builder::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
    }
    else
    {
        if (builder)
        {
            Workers::releaseWorker(builder);
            builder = nullptr;
        }

        if (nexus->completed && !shuttle)
        {
            status.complete = true;
        }
    }

    // Have the shuttle transfer some probes from a nearby base
    if (shuttle)
    {
        switch (workerTransferState)
        {
            case 0:
            {
                // Find the base to transfer from
                Base *bestBase = nullptr;
                int bestScore = INT_MAX;
                for (auto &b : Map::getMyBases())
                {
                    if (b->island) continue;

                    int availableWorkers = Workers::baseMineralWorkerCount(b);
                    if (availableWorkers < 8) continue;

                    int score = shuttle->getDistance(b->getPosition()) - availableWorkers * 200;
                    if (score < bestScore)
                    {
                        bestScore = score;
                        bestBase = b;
                    }
                }

                if (!bestBase) break;

                // First ensure the shuttle has arrived at the base
                if (workerTransfer.empty() && shuttle->getDistance(bestBase->mineralLineCenter) > 320)
                {
                    shuttle->moveTo(bestBase->mineralLineCenter);
                    break;
                }

                // Now wait until the nexus is sufficiently complete
                // We try to approximately handle transit time + 300 frames to load & unload the workers
                if (!nexus) break;
                if (!nexus->completed && workerTransfer.empty())
                {
                    int frames = 600 +
                                 PathFinding::ExpectedTravelTime(base->getPosition(), bestBase->getPosition(), BWAPI::UnitTypes::Protoss_Shuttle);
                    if (nexus->estimatedCompletionFrame - currentFrame > frames)
                    {
                        break;
                    }
                }

                // Then reserve the appropriate number of workers
                int desiredWorkers = std::min(8, (int) (workerTransfer.size() + Workers::baseMineralWorkerCount(bestBase)) / 2);
                while (desiredWorkers > workerTransfer.size())
                {
                    auto worker = Workers::getClosestReassignableWorker(shuttle->lastPosition, false);
                    if (!worker || shuttle->getDistance(worker) > 500) break;

                    Workers::reserveWorker(worker);
                    workerTransfer.push_back(worker);
                }

                // Then load all of the workers
                bool allLoaded = true;
                for (auto &worker : workerTransfer)
                {
                    if (!worker->bwapiUnit->isLoaded())
                    {
                        worker->moveTo(shuttle->lastPosition);

                        // Load the first not-loaded worker
                        if (allLoaded)
                        {
                            shuttle->load(worker->bwapiUnit);
                        }

                        allLoaded = false;
                    }
                }

                // State transition when all desired workers are loaded
                if (allLoaded && workerTransfer.size() == desiredWorkers)
                {
                    workerTransferState = 1;
                }

                break;
            }
            case 1:
            {
                unloadNear(shuttle, base->mineralLineCenter);

                // State transition when all workers are unloaded
                bool anyLoaded = false;
                for (const auto &worker : workerTransfer)
                {
                    if (worker->bwapiUnit->isLoaded())
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

void TakeIslandExpansion::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                                  const std::function<void(const MyUnit)> &movableUnitCallback)
{
    // TODO: Avoid stranding the worker on the island

    if (builder) Workers::releaseWorker(builder);
    if (shuttle) removedUnitCallback(shuttle);
}

void TakeIslandExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;

        // We only build a shuttle if we don't already have one
        // If we have one, we'll wait until it is available for island expo duties
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Shuttle) continue;
        if (Units::countAll(BWAPI::UnitTypes::Protoss_Shuttle) > 0) continue;

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

void TakeIslandExpansion::addUnit(const MyUnit &unit)
{
    if (unit->type == BWAPI::UnitTypes::Protoss_Shuttle) shuttle = unit;
}

void TakeIslandExpansion::removeUnit(const MyUnit &unit)
{
    if (shuttle == unit) shuttle = nullptr;
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
    if (!base->blockingNeutral || !base->blockingNeutral->exists()) return 0;

    if (base->blockingNeutral->getType().isMineralField()) return 0;

    int hp = base->blockingNeutral->getInitialHitPoints();
    if (base->blockingNeutral->isVisible())
    {
        hp = base->blockingNeutral->getHitPoints();
    }

    return (hp * BWAPI::UnitTypes::Protoss_Probe.groundWeapon().damageCooldown())
           / Players::attackDamage(BWAPI::Broodwar->self(),
                                   BWAPI::UnitTypes::Protoss_Probe,
                                   base->blockingNeutral->getPlayer(),
                                   base->blockingNeutral->getType());
}