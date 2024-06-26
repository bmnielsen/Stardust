#include "Workers.h"

#include "Base.h"
#include "Units.h"
#include "PathFinding.h"
#include "Map.h"
#include "NoGoAreas.h"
#include "WorkerOrderTimer.h"
#include "Boids.h"
#include "Strategist.h"

#include "DebugFlag_UnitOrders.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_LOG_WORKER_ASSIGNMENTS true
#endif
#if INSTRUMENTATION_ENABLED_VERBOSE
#define CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE false
#endif

namespace Workers
{
    namespace
    {
        enum class Job
        {
            None, Minerals, Gas, Reserved
        };

        int _desiredGasWorkerDelta;
        std::map<MyUnit, Job> workerJob;
        std::map<MyUnit, Base *> workerBase;
        std::map<Base *, std::set<MyUnit>> baseWorkers;
        std::map<MyUnit, Resource> workerMineralPatch;
        std::map<Resource, std::set<MyUnit>> mineralPatchWorkers;
        std::map<MyUnit, Resource> workerRefinery;
        std::map<Resource, std::set<MyUnit>> refineryWorkers;

        int mineralWorkerCount;
        std::pair<int, int> gasWorkerCount;

        int desiredRefineryWorkers(Base *base, BWAPI::TilePosition refineryTile)
        {
            if (base && base->gasRequiresFourWorkers(refineryTile))
            {
                return 4;
            }

            return 3;
        }

        // Removes a worker's base and mineral patch or gas assignments
        void removeFromResource(
                const MyUnit &unit,
                std::map<MyUnit, Resource> &workerAssignment,
                std::map<Resource, std::set<MyUnit>> &resourceAssignment)
        {
            auto baseIt = workerBase.find(unit);
            if (baseIt != workerBase.end())
            {
                if (baseIt->second)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                    CherryVis::log(unit->id) << "Removed from base @ " << BWAPI::WalkPosition(baseIt->second->getPosition())
                                             << " (removeFromResource)";
#endif
                    baseWorkers[baseIt->second].erase(unit);
                }
                workerBase.erase(baseIt);
            }

            auto resourceIt = workerAssignment.find(unit);
            if (resourceIt != workerAssignment.end())
            {
                resourceAssignment[resourceIt->second].erase(unit);
                workerAssignment.erase(resourceIt);
            }
        }

        // Cleans up data maps when a worker is lost
        void workerLost(const MyUnit &unit)
        {
            workerJob.erase(unit);
            removeFromResource(unit, workerMineralPatch, mineralPatchWorkers);
            removeFromResource(unit, workerRefinery, refineryWorkers);
        }

        int availableMineralAssignmentsAtBase(Base *base, int workersPerPatch = 2)
        {
            if (!base || base->owner != BWAPI::Broodwar->self()) return 0;
            if (!base->resourceDepot) return 0;

            size_t count = base->mineralPatchCount() * workersPerPatch;
            for (const auto &worker : baseWorkers[base])
            {
                if (workerJob[worker] == Job::Minerals) count--;
            }

            return (int)count;
        }

        int availableGasAssignmentsAtBase(Base *base)
        {
            if (!base || base->owner != BWAPI::Broodwar->self()) return 0;
            if (!base->resourceDepot) return 0;

            int count = 0;
            for (const auto &geyserOrRefinery : base->geysersOrRefineries())
            {
                if (!geyserOrRefinery->hasMyCompletedRefinery()) continue;
                count += desiredRefineryWorkers(base, geyserOrRefinery->tile);
            }
            for (const auto &worker : baseWorkers[base])
            {
                if (workerJob[worker] == Job::Gas) count--;
            }

            return count;
        }

        void assignInitialMineralWorkers()
        {
            auto base = Map::getMyMain();

            // Sort the mineral patches by proximity to the nexus
            auto mineralPatches = base->mineralPatches();
            std::sort(mineralPatches.begin(), mineralPatches.end(), [&](const Resource &a, const Resource &b) -> bool
            {
                return a->getDistance(BWAPI::UnitTypes::Protoss_Nexus, base->getPosition()) <
                    b->getDistance(BWAPI::UnitTypes::Protoss_Nexus, base->getPosition());
            });

            // We are only interested in the first four
            mineralPatches.resize(4);
            std::set<Resource> availablePatches(mineralPatches.begin(), mineralPatches.end());

            // Greedily take the closest matches until all probes are assigned
            // TODO: Should really be optimizing for 7th collection
            for (int i = 0; i < 4; i++)
            {
                int bestDist = INT_MAX;
                MyUnit bestWorker = nullptr;
                Resource bestPatch = nullptr;
                for (auto &worker : Units::allMine())
                {
                    if (!worker->type.isWorker()) continue;
                    if (!worker->completed) continue;
                    if (workerMineralPatch[worker]) continue;

                    for (auto &patch : availablePatches)
                    {
                        int dist = patch->getDistance(worker);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            bestWorker = worker;
                            bestPatch = patch;
                        }
                    }
                }

                if (bestWorker && bestPatch)
                {
                    workerJob[bestWorker] = Job::Minerals;
#if CVIS_LOG_WORKER_ASSIGNMENTS
                    CherryVis::log(bestWorker->id) << "Assigned to base @ " << BWAPI::WalkPosition(base->getPosition());
                    CherryVis::log(bestWorker->id) << "Assigned to Minerals";
#endif

                    workerBase[bestWorker] = base;
                    baseWorkers[base].insert(bestWorker);
                    workerMineralPatch[bestWorker] = bestPatch;
                    mineralPatchWorkers[bestPatch].insert(bestWorker);
                    availablePatches.erase(bestPatch);
                }
            }
        }

        // Assign a worker to the closest base with either free mineral or gas assignments depending on job
        Base *assignBaseAndJob(const MyUnit &unit, Job preferredJob)
        {
            // TODO: Prioritize empty bases over nearly-full bases

            int bestFrames = INT_MAX;
            Base *bestBase = nullptr;
            bool bestHasPreferredJob = false;
            bool bestHasNonPreferredJob = false;
            for (auto &base : Map::getMyBases())
            {
                if (!base->resourceDepot) continue;

                bool hasPreferred, hasNonPreferred;
                if (preferredJob == Job::Minerals)
                {
                    hasPreferred = availableMineralAssignmentsAtBase(base) > 0;
                    if (bestHasPreferredJob && !hasPreferred) continue;

                    hasNonPreferred = availableGasAssignmentsAtBase(base) > 0;
                }
                else
                {
                    hasPreferred = availableGasAssignmentsAtBase(base) > 0;
                    if (bestHasPreferredJob && !hasPreferred) continue;

                    hasNonPreferred = availableMineralAssignmentsAtBase(base) > 0;
                }

                if (!hasPreferred && !hasNonPreferred) continue;

                int frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                             base->getPosition(),
                                                             unit->type,
                                                             PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                             -1);
                if (frames == -1) continue;

                // Logic if the base is far away (i.e. would require a worker transfer)
                // Exclusion if we don't have many workers yet, in which case this is probably our scout
                if (frames > 500 && Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) > 20)
                {
                    // Don't transfer to a threatened base
                    if (!Units::enemyAtBase(base).empty()) continue;

                    // Require our main army to be on the map
                    auto mainArmyPlay = Strategist::getMainArmyPlay();
                    if (!mainArmyPlay) continue;

                    auto vanguardCluster = mainArmyPlay->getSquad()->vanguardCluster();
                    if (!vanguardCluster) continue;

                    if (vanguardCluster->percentageToEnemyMain < 0.6) continue;
                }

                if (!base->resourceDepot->completed)
                    frames = std::max(frames, base->resourceDepot->bwapiUnit->getRemainingBuildTime());

                if (frames < bestFrames || (hasPreferred && !bestHasPreferredJob) || (hasNonPreferred && !bestHasNonPreferredJob))
                {
                    bestFrames = frames;
                    bestBase = base;
                    bestHasPreferredJob = hasPreferred;
                    bestHasNonPreferredJob = hasNonPreferred;
                }
            }

            Job job = Job::None;
            if (bestBase)
            {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                if (workerBase[unit] != bestBase)
                {
                    CherryVis::log(unit->id) << "Assigned to base @ " << BWAPI::WalkPosition(bestBase->getPosition());
                }
#endif

                workerBase[unit] = bestBase;
                baseWorkers[bestBase].insert(unit);

                if (bestHasPreferredJob)
                {
                    job = preferredJob;
                }
                else if (bestHasNonPreferredJob)
                {
                    job = (preferredJob == Job::Minerals ? Job::Gas : Job::Minerals);
                }
            }

#if CVIS_LOG_WORKER_ASSIGNMENTS
            if (workerJob[unit] != job)
            {
                if (job == Job::Minerals)
                {
                    CherryVis::log(unit->id) << "Assigned to Minerals";
                }
                else if (job == Job::Gas)
                {
                    CherryVis::log(unit->id) << "Assigned to Gas";
                }
                else if (job == Job::None)
                {
                    CherryVis::log(unit->id) << "Assigned to None";
                }
            }
#endif

            workerJob[unit] = job;

            return bestBase;
        }

        // Assign a worker to a mineral patch in its current base
        // We only call this when the worker is close to the base depot, so we can assume minimal pathing issues
        Resource assignMineralPatch(const MyUnit &unit)
        {
            if (!workerBase[unit]) return nullptr;

            int closestDist = INT_MAX;
            int furthestDist = 0;
            Resource closest = nullptr;
            Resource furthest = nullptr;
            for (const auto &mineralPatch : workerBase[unit]->mineralPatches())
            {
                size_t workers = mineralPatchWorkers[mineralPatch].size();
                if (workers >= 2) continue;

                int dist = mineralPatch->getDistance(BWAPI::UnitTypes::Protoss_Nexus, workerBase[unit]->getPosition());
                if (workers == 0 && dist < closestDist)
                {
                    closestDist = dist;
                    closest = mineralPatch;
                }
                else if (workers == 1 && dist > furthestDist)
                {
                    furthestDist = dist;
                    furthest = mineralPatch;
                }
            }

            Resource best = closest ? closest : furthest;
            if (best)
            {
                workerMineralPatch[unit] = best;
                mineralPatchWorkers[best].insert(unit);
            }

            return best;
        }

        // Assign a worker to a refinery in its current base
        // We only call this when the worker is close to the base depot, so we can assume minimal pathing issues
        Resource assignRefinery(const MyUnit &unit)
        {
            if (!workerBase[unit]) return nullptr;

            int bestDist = INT_MAX;
            Resource bestRefinery = nullptr;
            for (const auto &geyserOrRefinery : workerBase[unit]->geysersOrRefineries())
            {
                if (!geyserOrRefinery->hasMyCompletedRefinery()) continue;

                size_t workers = refineryWorkers[geyserOrRefinery].size();
                if (workers >= desiredRefineryWorkers(workerBase[unit], geyserOrRefinery->tile)) continue;

                int dist = geyserOrRefinery->getDistance(unit);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestRefinery = geyserOrRefinery;
                }
            }

            if (bestRefinery)
            {
                workerRefinery[unit] = bestRefinery;
                refineryWorkers[bestRefinery].insert(unit);
            }

            return bestRefinery;
        }

        void assignGasWorker()
        {
            auto myCompletedRefineries = Units::myCompletedRefineries();

            // Find worker closest to an available refinery
            int bestDist = INT_MAX;
            MyUnit bestWorker = nullptr;
            for (const auto &worker : Units::allMine())
            {
                if (!isAvailableForReassignment(worker, false)) continue;

                for (const auto &refinery : myCompletedRefineries)
                {
                    if (refineryWorkers[refinery].size() >= desiredRefineryWorkers(workerBase[worker], refinery->tile))
                    {
                        continue;
                    }

                    // TODO: Consider depleted geysers

                    int dist = refinery->getDistance(worker);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestWorker = worker;
                    }
                }
            }

            if (bestWorker)
            {
                removeFromResource(bestWorker, workerMineralPatch, mineralPatchWorkers);
                assignBaseAndJob(bestWorker, Job::Gas);
                assignRefinery(bestWorker);
            }
        }

        void removeGasWorker()
        {
            // Remove a gas worker at a base that has available mineral assignments
            for (const auto &workerAndRefinery : workerRefinery)
            {
                if (availableMineralAssignmentsAtBase(workerBase[workerAndRefinery.first]) > 0)
                {
                    workerJob[workerAndRefinery.first] = Job::None;
                    removeFromResource(workerAndRefinery.first, workerRefinery, refineryWorkers);
                    return;
                }
            }
        }
    }

    void initialize()
    {
        _desiredGasWorkerDelta = 0;
        workerJob.clear();
        workerBase.clear();
        baseWorkers.clear();
        workerMineralPatch.clear();
        mineralPatchWorkers.clear();
        workerRefinery.clear();
        refineryWorkers.clear();
    }

    void onUnitDestroy(const Unit &unit)
    {
        // Handle lost workers
        if (unit->type.isWorker() && unit->player == BWAPI::Broodwar->self())
        {
            workerLost(Units::mine(unit->bwapiUnit));
        }
    }

    void onMineralPatchDestroyed(const Resource &mineralPatch)
    {
        auto it = mineralPatchWorkers.find(mineralPatch);
        if (it != mineralPatchWorkers.end())
        {
            for (auto &worker : it->second)
            {
                workerMineralPatch.erase(worker);
            }
            mineralPatchWorkers.erase(it);
        }
    }

    void updateAssignments()
    {
        mineralWorkerCount = 0;
        auto countMineralWorker = [](const MyUnit &worker, const Resource &mineralPatch)
        {
            if (!mineralPatch) return;
            if (mineralPatch->destroyed) return;
            if (mineralPatch->getDistance(worker) > 200) return;

            mineralWorkerCount++;
        };
        gasWorkerCount = std::make_pair(0, 0);
        auto countGasWorker = [](const MyUnit &worker, const Resource &refinery)
        {
            if (!refinery) return;
            if (!refinery->hasMyCompletedRefinery()) return;
            if (refinery->getDistance(worker) > 200) return;

            if (refinery->currentAmount <= 0)
            {
                gasWorkerCount.second++;
            }
            else
            {
                gasWorkerCount.first++;
            }
        };

        // Special case on frame 0: try to optimize for earliest possible seventh mineral returned
        if (currentFrame == 0)
        {
            assignInitialMineralWorkers();
        }

        for (auto &worker : Units::allMine())
        {
            if (!worker->type.isWorker()) continue;
            if (!worker->completed) continue;
            if (workerJob[worker] == Job::Reserved)
            {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                CherryVis::log(worker->id) << "Assignment: reserved";
#endif
                continue;
            }

            if (workerJob[worker] == Job::Minerals)
            {
                // If the worker is already assigned to a mineral patch, we don't need to do any more
                auto mineralPatch = workerMineralPatch[worker];
                if (mineralPatch)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    CherryVis::log(worker->id) << "Assignment: mineral patch @ " << BWAPI::WalkPosition(mineralPatch->getPosition());
#endif

                    countMineralWorker(worker, mineralPatch);
                    continue;
                }

                // Release from assigned base if it is mined out
                auto base = workerBase[worker];
                if (base && availableMineralAssignmentsAtBase(base) <= 0)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                    CherryVis::log(worker->id) << "Removed from base @ " << BWAPI::WalkPosition(base->getPosition()) << " (mined out)";
#endif
                    baseWorkers[base].erase(worker);
                    workerBase[worker] = nullptr;
                    base = nullptr;
                }
            }
            else if (workerJob[worker] == Job::Gas)
            {
                // If the worker is already assigned to a refinery, we don't need to do any more
                auto refinery = workerRefinery[worker];
                if (refinery && refinery->hasMyCompletedRefinery())
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    CherryVis::log(worker->id) << "Assignment: refinery @ " << BWAPI::WalkPosition(refinery->getPosition());
#endif

                    countGasWorker(worker, refinery);
                    continue;
                }
            }

            // If the worker doesn't have an assigned base, assign it one
            auto base = workerBase[worker];
            if (workerJob[worker] == Job::None || !base || !base->resourceDepot || availableMineralAssignmentsAtBase(base) <= 0)
            {
                auto newBase = assignBaseAndJob(worker, (workerJob[worker] == Job::Gas) ? Job::Gas : Job::Minerals);

                if (base != newBase)
                {
                    if (base)
                    {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                        CherryVis::log(worker->id) << "Removed from base @ " << BWAPI::WalkPosition(base->getPosition()) << " (new base)";
#endif
                        baseWorkers[base].erase(worker);
                    }
                    base = newBase;
                }

                // Maybe we have none
                if (!base)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    CherryVis::log(worker->id) << "Assignment: no base available";
#endif

                    continue;
                }
            }

            // Assign a resource when the worker is close enough to the base
            if (worker->getDistance(base->getPosition()) <= 300)
            {
                if (workerJob[worker] == Job::Minerals)
                {
                    auto mineralPatch = assignMineralPatch(worker);
                    countMineralWorker(worker, mineralPatch);

#if CVIS_LOG_WORKER_ASSIGNMENTS
                    if (mineralPatch)
                    {
                        CherryVis::log(worker->id) << "Assignment: " << *mineralPatch;
                    }
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    else
                    {
                        CherryVis::log(worker->id) << "Assignment: no mineral patch available";
                    }
#endif
#endif
                }
                else
                {
                    auto refinery = assignRefinery(worker);
                    countGasWorker(worker, refinery);

#if CVIS_LOG_WORKER_ASSIGNMENTS
                    if (refinery)
                    {
                        CherryVis::log(worker->id) << "Assignment: " << *refinery;
                    }
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    else
                    {
                        CherryVis::log(worker->id) << "Assignment: no refinery available";
                    }
#endif
#endif
                }
            }
        }

        // We assign 4 workers to bottom geysers, which would confuse our producer, since the fourth worker doesn't contribute
        // to higher income compared to a normal geyser
        // So reduce our gas worker counts by one for each refinery with 4 workers assigned to it
        int excessGasWorkers = 0;
        for (auto &refineryAndWorkers : refineryWorkers)
        {
            if (refineryAndWorkers.second.size() <= 3) continue;
            excessGasWorkers++;
            if (refineryAndWorkers.first->currentAmount <= 0)
            {
                gasWorkerCount.second--;
            }
            else
            {
                gasWorkerCount.first--;
            }
        }

        CherryVis::setBoardValue("mineralWorkers", (std::ostringstream() << mineralWorkerCount).str());
        CherryVis::setBoardValue("gasWorkers",
                                 (std::ostringstream() << gasWorkerCount.first << ":" << gasWorkerCount.second << "+" << excessGasWorkers).str());
    }

    void issueOrders()
    {
        // Adjust number of gas workers to desired count
        for (int i = 0; i < _desiredGasWorkerDelta; i++)
        {
            assignGasWorker();
        }
        for (int i = 0; i > _desiredGasWorkerDelta; i--)
        {
            removeGasWorker();
        }

        for (auto &pair : workerJob)
        {
            if (pair.second == Job::Reserved) continue;

            auto &worker = pair.first;

            if (NoGoAreas::isNoGo(worker->getTilePosition()))
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(worker->id) << "Moving to avoid no-go area";
#endif
                worker->moveTo(Boids::AvoidNoGoArea(worker.get()));
                continue;
            }

            switch (pair.second)
            {
                case Job::Minerals:
                case Job::Gas:
                {
                    // Skip if the worker doesn't have a valid base
                    auto base = workerBase[worker];
                    if (!base || !base->resourceDepot)
                    {
                        continue;
                    }

                    // If the worker has cargo, return it
                    if (worker->bwapiUnit->isCarryingMinerals() || worker->bwapiUnit->isCarryingGas())
                    {
                        // Handle the special case when the worker is harvesting from a base without a completed depot
                        if (!base->resourceDepot || !base->resourceDepot->completed)
                        {
                            // Find the nearest base with a completed depot
                            int closestTime = INT_MAX;
                            Base *closestBase = nullptr;
                            for (auto otherBase : Map::getMyBases())
                            {
                                if (otherBase == base) continue;
                                if (!otherBase->resourceDepot || !otherBase->resourceDepot->completed) continue;

                                int time = PathFinding::ExpectedTravelTime(worker->lastPosition,
                                                                           otherBase->getPosition(),
                                                                           worker->type,
                                                                           PathFinding::PathFindingOptions::Default,
                                                                           -1);
                                if (time != -1 && time < closestTime)
                                {
                                    closestTime = time;
                                    closestBase = otherBase;
                                }
                            }

                            // If there is one, and the worker can get there and back before the base depot is complete, deliver there
                            if (closestBase)
                            {
                                int baseToBaseTime = PathFinding::ExpectedTravelTime(base->getPosition(),
                                                                                     closestBase->getPosition(),
                                                                                     BWAPI::UnitTypes::Protoss_Probe,
                                                                                     PathFinding::PathFindingOptions::Default,
                                                                                     -1);
                                if (baseToBaseTime != -1 && closestTime + baseToBaseTime < base->resourceDepot->bwapiUnit->getRemainingBuildTime())
                                {
                                    if (worker->getDistance(closestBase->resourceDepot) > 200)
                                    {
                                        worker->moveTo(closestBase->getPosition());
                                    }
                                    else if (worker->bwapiUnit->getOrder() != BWAPI::Orders::ReturnMinerals &&
                                             worker->bwapiUnit->getOrder() != BWAPI::Orders::ReturnGas &&
                                             worker->lastCommandFrame > (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
                                    {
                                        worker->rightClick(closestBase->resourceDepot->bwapiUnit);
                                    }
                                    else if (worker->bwapiUnit->isCarryingMinerals())
                                    {
                                        auto mineralPatch = workerMineralPatch[worker];
                                        if (mineralPatch)
                                        {
                                            WorkerOrderTimer::optimizeReturn(worker, mineralPatch, closestBase->resourceDepot);
                                        }
                                    }
                                    continue;
                                }
                            }

                            // There wasn't another base to return to, or it would take too long, so move towards the preferred base instead
                            if (base->resourceDepot)
                            {
                                worker->moveTo(base->getPosition());
                            }
                            else
                            {
                                worker->moveTo(base->mineralLineCenter);
                            }
                            continue;
                        }

                        // Leave it alone if it already has the return order or is resetting after finishing gathering
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnMinerals ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ResetCollision)
                        {
                            // Potentially optimize the return if we are returning minerals
                            if (worker->bwapiUnit->isCarryingMinerals())
                            {
                                auto mineralPatch = workerMineralPatch[worker];
                                if (mineralPatch)
                                {
                                    WorkerOrderTimer::optimizeReturn(worker, mineralPatch, base->resourceDepot);
                                }
                            }

                            continue;
                        }

                        // Leave it alone if we have just ordered it to do something, as that indicates we are trying to optimize
                        if (worker->lastCommandFrame > (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
                        {
                            continue;
                        }

                        worker->returnCargo();
                        continue;
                    }

                    // Handle mining from an assigned mineral patch
                    auto mineralPatch = workerMineralPatch[worker];
                    if (mineralPatch)
                    {
                        if (WorkerOrderTimer::optimizeStartOfMining(worker, mineralPatch)) continue;

                        // If the unit is currently mining, leave it alone
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ResetCollision)
                        {
                            continue;
                        }

                        // If the unit is returning cargo, leave it alone
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnMinerals) continue;

                        // If we don't have vision on the mineral patch, move towards it
                        auto bwapiUnit = mineralPatch->getBwapiUnitIfVisible();
                        if (!bwapiUnit)
                        {
                            worker->moveTo(mineralPatch->center);
                            continue;
                        }

                        // Check if another worker is currently mining this patch
                        MyUnit otherWorker = nullptr;
                        for (const auto &unit : mineralPatchWorkers[mineralPatch])
                        {
                            if (unit != worker)
                            {
                                otherWorker = unit;
                                break;
                            }
                        }

                        // Resend the gather command when we expect the other worker to be finished mining in 11+LF frames
                        if (otherWorker && otherWorker->exists() && otherWorker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals &&
                            (otherWorker->bwapiUnit->getOrderTimer() + 7) == (11 + BWAPI::Broodwar->getLatencyFrames()))
                        {
                            // Exception: If we are not at the patch yet, and our last command was sent a long time ago,
                            // we probably want to wait and allow our order timer optimization to send the command instead.
                            int dist = mineralPatch->getDistance(worker);
                            if (dist > 20 && worker->lastCommandFrame < (currentFrame - 20)) continue;

                            worker->gather(bwapiUnit);
                            continue;
                        }

                        // Mineral locking: if the unit is moving to or waiting for minerals, make sure it doesn't switch targets
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
                        {
                            if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
                                && worker->bwapiUnit->getOrderTarget() != bwapiUnit
                                && worker->lastCommandFrame
                                   < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
                            {
                                worker->gather(bwapiUnit);
                            }

                            continue;
                        }

                        // Otherwise for all other orders click on the mineral patch
                        worker->gather(bwapiUnit);
                        continue;
                    }

                    // Handle gathering from an assigned refinery
                    auto refinery = workerRefinery[worker];
                    if (refinery && refinery->hasMyCompletedRefinery() && refinery->getDistance(worker) < 500)
                    {
                        // If the unit is currently gathering, leave it alone
                        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::HarvestGas ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::Harvest1 ||
                            worker->bwapiUnit->getOrder() == BWAPI::Orders::ReturnGas)
                        {
                            continue;
                        }

                        // Otherwise click on the refinery
                        auto bwapiUnit = refinery->getBwapiUnitIfVisible();
                        if (bwapiUnit)
                        {
                            worker->gather(bwapiUnit);
                            continue;
                        }
                    }

                    // If the worker is a long way from its base, move towards it
                    if (worker->getDistance(base->getPosition()) > 200)
                    {
#if DEBUG_UNIT_ORDERS
                        CherryVis::log(worker->id) << "moveTo: Assigned base (far away)";
#endif
                        worker->moveTo(base->getPosition());
                        continue;
                    }

                    // For some reason the worker doesn't have anything to do
                    // Clear its state so it gets a new assignment
                    removeFromResource(worker, workerMineralPatch, mineralPatchWorkers);
                    removeFromResource(worker, workerRefinery, refineryWorkers);
#if DEBUG_UNIT_ORDERS
                    CherryVis::log(worker->id) << "Worker has nothing to do, clearing state";
#endif
                    worker->moveTo(base->getPosition());
                    break;
                }
                case Job::None:
                {
                    // Move towards the base if we aren't near it
                    auto base = workerBase[worker];
                    if (!base || !base->resourceDepot)
                    {
                        continue;
                    }

                    int dist = worker->getDistance(base->getPosition());
                    if (dist > 200)
                    {
                        worker->moveTo(base->getPosition());
                    }
                }
                default:
                {
                    // Nothing needed for other cases
                    break;
                }
            }
        }
    }

    bool isAvailableForReassignment(const MyUnit &unit, bool allowCarryMinerals)
    {
        if (!unit || !unit->exists() || !unit->completed || !unit->type.isWorker()) return false;

        auto job = workerJob[unit];
        if (job == Job::None) return true;
        if (job == Job::Minerals)
        {
            if (unit->bwapiUnit->isCarryingGas()) return false;
            if (!allowCarryMinerals && unit->bwapiUnit->isCarryingMinerals()) return false;

            return true;
        }

        return false;
    }

    MyUnit getClosestReassignableWorker(BWAPI::Position position, bool allowCarryMinerals, int *bestTravelTime)
    {
        int bestTime = INT_MAX;
        int bestScore = INT_MAX;
        MyUnit bestWorker = nullptr;
        for (auto &unit : Units::allMine())
        {
            if (!isAvailableForReassignment(unit, allowCarryMinerals)) continue;

            int travelTime =
                    PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                    position,
                                                    unit->type,
                                                    PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                    -1);

            // Disallow carrying minerals in all cases if the travel time is excessive
            // Rationale: we might be sending the unit to build something at another base and we want to return minerals first
            if (travelTime > 100 && unit->bwapiUnit->isCarryingMinerals()) continue;

            // If the unit is currently mining, penalize it by 3 seconds to encourage selecting other workers
            int score = travelTime;
            if (unit->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
                unit->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
            {
                score += 72;
            }

            // If the unit is currently unassigned, give it a 4 second bonus to encourage selecting it
            if (workerJob[unit] == Job::None) score -= 96;

            if (travelTime != -1 && score < bestScore)
            {
                bestTime = travelTime;
                bestScore = score;
                bestWorker = unit;
            }
        }

        if (bestTravelTime) *bestTravelTime = bestTime;
        return bestWorker;
    }

    size_t getBaseWorkerCount(Base *base)
    {
        auto baseAndWorkersIt = baseWorkers.find(base);
        if (baseAndWorkersIt == baseWorkers.end()) return 0;

        return baseAndWorkersIt->second.size();
    }

    std::vector<MyUnit> getBaseWorkers(Base *base)
    {
        std::vector<MyUnit> result;

        auto baseAndWorkersIt = baseWorkers.find(base);
        if (baseAndWorkersIt == baseWorkers.end()) return result;

        result.reserve(baseAndWorkersIt->second.size());
        for (auto &worker : baseAndWorkersIt->second)
        {
            result.push_back(worker);
        }
        return result;
    }

    int baseMineralWorkerCount(Base *base)
    {
        int result = 0;

        auto baseAndWorkersIt = baseWorkers.find(base);
        if (baseAndWorkersIt == baseWorkers.end()) return result;

        for (auto &worker : baseAndWorkersIt->second)
        {
            auto it = workerJob.find(worker);
            if (it != workerJob.end() && it->second == Job::Minerals) result++;
        }
        return result;
    }

    void reserveBaseWorkers(std::vector<MyUnit> &workers, Base *base)
    {
        auto baseAndWorkersIt = baseWorkers.find(base);
        if (baseAndWorkersIt == baseWorkers.end()) return;

        for (auto &worker : baseAndWorkersIt->second)
        {
            workers.push_back(worker);
        }

        for (auto &worker : workers)
        {
            reserveWorker(worker);
        }
    }

    void reserveWorker(const MyUnit &unit)
    {
        if (!unit || !unit->exists() || !unit->type.isWorker() || !unit->completed) return;

        if (workerJob[unit] == Job::Reserved) return;

        workerJob[unit] = Job::Reserved;
        removeFromResource(unit, workerMineralPatch, mineralPatchWorkers);
        removeFromResource(unit, workerRefinery, refineryWorkers);
        CherryVis::log(unit->id) << "Reserved for non-mining duties";
    }

    void releaseWorker(const MyUnit &unit)
    {
        if (!unit || !unit->exists() || !unit->type.isWorker() || !unit->completed || workerJob[unit] != Job::Reserved) return;

        workerJob[unit] = Job::None;
        CherryVis::log(unit->id) << "Released from non-mining duties";
    }

    int availableMineralAssignments(Base *base, int workersPerPatch)
    {
        if (base) return availableMineralAssignmentsAtBase(base, workersPerPatch);

        int count = 0;
        for (auto &myBase : Map::getMyBases())
        {
            count += availableMineralAssignmentsAtBase(myBase, workersPerPatch);
        }

        return count;
    }

    int availableGasAssignments(Base *base)
    {
        if (base) return availableGasAssignmentsAtBase(base);

        int count = 0;
        for (auto &myBase : Map::getMyBases())
        {
            count += availableGasAssignmentsAtBase(myBase);
        }

        return count;
    }

    void setDesiredGasWorkerDelta(int gasWorkerDelta)
    {
        _desiredGasWorkerDelta = gasWorkerDelta;
    }

    int mineralWorkers()
    {
        return mineralWorkerCount;
    }

    std::pair<int, int> gasWorkers()
    {
        return gasWorkerCount;
    }

    int reassignableMineralWorkers()
    {
        // Do an initial scan to find the number of gas slots and available mineral workers there are at each base
        std::vector<std::tuple<Base*, int, int>> basesAndGasSlotsAndMineralWorkersAvailable;
        for (auto &baseAndWorkers : baseWorkers)
        {
            if (!baseAndWorkers.first || baseAndWorkers.first->owner != BWAPI::Broodwar->self()) continue;
            if (!baseAndWorkers.first->resourceDepot) return 0;

            int gasAvailable = 0;
            for (const auto &refinery : baseAndWorkers.first->geysersOrRefineries())
            {
                if (refinery->hasMyCompletedRefinery())
                {
                    gasAvailable += desiredRefineryWorkers(baseAndWorkers.first, refinery->tile);
                }
            }

            int mineralWorkersAvailable = 0;
            for (const auto &worker : baseAndWorkers.second)
            {
                if (workerJob[worker] == Job::Minerals) mineralWorkersAvailable++;
                if (workerJob[worker] == Job::Gas) gasAvailable--;
            }

            basesAndGasSlotsAndMineralWorkersAvailable.emplace_back(baseAndWorkers.first, std::max(gasAvailable, 0), mineralWorkersAvailable);
        }

        // Now count the number of mineral workers that can be transferred to gas, allowing transfer to close bases
        int result = 0;
        for (auto &[base, gasAvailable, mineralWorkersAvailable] : basesAndGasSlotsAndMineralWorkersAvailable)
        {
            if (gasAvailable == 0) continue;
            if (mineralWorkersAvailable >= gasAvailable)
            {
                result += gasAvailable;
                continue;
            }

            // Check if we can borrow mineral workers from a nearby base
            int borrowedMineralWorkers = 0;
            for (auto &[otherBase, otherGasAvailable, otherMineralWorkersAvailable] : basesAndGasSlotsAndMineralWorkersAvailable)
            {
                if (base == otherBase) continue;
                if (otherMineralWorkersAvailable <= otherGasAvailable) continue;
                int frames = PathFinding::ExpectedTravelTime(base->getPosition(),
                                                             otherBase->getPosition(),
                                                             BWAPI::UnitTypes::Protoss_Probe,
                                                             PathFinding::PathFindingOptions::Default,
                                                             -1);
                if (frames == -1) continue;
                if (frames < 400)
                {
                    borrowedMineralWorkers += (otherMineralWorkersAvailable - otherGasAvailable);
                }
            }

            result += std::min(gasAvailable, mineralWorkersAvailable + borrowedMineralWorkers);
        }

        return result;
    }

    int reassignableGasWorkers()
    {
        size_t result = 0;
        for (auto &baseAndWorkers : baseWorkers)
        {
            if (!baseAndWorkers.first || baseAndWorkers.first->owner != BWAPI::Broodwar->self()) continue;
            if (!baseAndWorkers.first->resourceDepot) return 0;

            size_t mineralsAvailable = baseAndWorkers.first->mineralPatchCount() * 2;
            if (mineralsAvailable == 0) continue;

            size_t gasWorkersAvailable = 0;
            for (const auto &worker : baseAndWorkers.second)
            {
                if (workerJob[worker] == Job::Gas) gasWorkersAvailable++;
                if (workerJob[worker] == Job::Minerals) mineralsAvailable--;
            }

            result += std::min(mineralsAvailable, gasWorkersAvailable);
        }

        return (int)result;
    }

    int idleWorkerCount()
    {
        int count = 0;
        for (const auto &workerAndJob : workerJob)
        {
            if (workerAndJob.second == Job::None)
            {
                count++;
            }
        }
        return count;
    }

    std::map<Resource, std::set<MyUnit>> &mineralsAndAssignedWorkers()
    {
        return mineralPatchWorkers;
    }
}
