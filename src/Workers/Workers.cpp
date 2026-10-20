#include "Workers.h"

#include "Base.h"
#include "Units.h"
#include "PathFinding.h"
#include "Map.h"
#include "NoGoAreas.h"
#include "MiningOptimization/WorkerMiningOptimization.h"
#include "Boids.h"
#include "Strategist.h"

#include "DebugFlag_UnitOrders.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_LOG_WORKER_ASSIGNMENTS true
#endif
#if INSTRUMENTATION_ENABLED_VERBOSE
#define CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE false
#define WARN_ON_NO_RESOURCE_DATA true
#endif

namespace WorkerMgr
{
    namespace
    {
        enum class Job
        {
            None, Minerals, Gas, Reserved
        };

        int _desiredGasWorkerDelta;
        std::map<OwnedWorker, Job> workerJob;
        std::map<OwnedWorker, Base *> workerBase;
        std::map<Base *, std::set<OwnedWorker>> baseWorkers;
        std::map<OwnedWorker, Resource> workerMineralPatch;
        std::map<Resource, std::set<OwnedWorker>> mineralPatchWorkers;
        std::map<OwnedWorker, Resource> workerRefinery;
        std::map<Resource, std::set<OwnedWorker>> refineryWorkers;

        int mineralWorkerCount;
        std::pair<int, int> gasWorkerCount;

        int desiredRefineryWorkers(Base *baseLoc, BWAPI::TilePosition refineryTile)
        {
            if (baseLoc && baseLoc->gasRequiresFourWorkers(refineryTile))
            {
                return 4;
            }

            return 3;
        }

        /* Removes a worker's base and mineral patch or gas assignments */
        void removeFromResource(
                const OwnedWorker &gameUnit,
                std::map<OwnedWorker, Resource> &workerAssignment,
                std::map<Resource, std::set<OwnedWorker>> &resourceAssignment)
        {
            auto baseIt = workerBase.find(gameUnit);
            if (baseIt != workerBase.end())
            {
                if (baseIt->second)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                    Telemetry::log(gameUnit->ident) << "Removed from base @ " << BWAPI::WalkPosition(baseIt->second->getPosition())
                                             << " (removeFromResource)";
#endif
                    baseWorkers[baseIt->second].erase(gameUnit);
                }
                workerBase.erase(baseIt);
            }

            auto resourceIt = workerAssignment.find(gameUnit);
            if (resourceIt != workerAssignment.end())
            {
                resourceAssignment[resourceIt->second].erase(gameUnit);
                workerAssignment.erase(resourceIt);
            }
        }

        /* Cleans up data maps when a worker is lost */
        void workerLost(const OwnedWorker &gameUnit)
        {
            workerJob.erase(gameUnit);
            removeFromResource(gameUnit, workerMineralPatch, mineralPatchWorkers);
            removeFromResource(gameUnit, workerRefinery, refineryWorkers);
        }

        int availableMineralAssignmentsAtBase(Base *baseLoc, int workersPerPatch = 2)
        {
            if (!baseLoc || baseLoc->owner != BWAPI::Broodwar->self()) return 0;
            if (!baseLoc->resourceDepot) return 0;

            size_t count = baseLoc->mineralPatchCount() * workersPerPatch;
            for (const auto &wrk : baseWorkers[baseLoc])
            {
                if (workerJob[wrk] == Job::Minerals) count--;
            }

            return (int)count;
        }

        int availableGasAssignmentsAtBase(Base *baseLoc)
        {
            if (!baseLoc || baseLoc->owner != BWAPI::Broodwar->self()) return 0;
            if (!baseLoc->resourceDepot) return 0;

            int count = 0;
            for (const auto &geyserOrRefinery : baseLoc->geysersOrRefineries())
            {
                if (!geyserOrRefinery->hasMyCompletedRefinery()) continue;
                count += desiredRefineryWorkers(baseLoc, geyserOrRefinery->tilePos);
            }
            for (const auto &wrk : baseWorkers[baseLoc])
            {
                if (workerJob[wrk] == Job::Gas) count--;
            }

            return count;
        }

        bool assignInitialMineralWorkersFromObservations()
        {
            /* There can be quite a bit of variance in the timings for each initial worker depending on its initial heading and how its order timer */
            /* is reset. We therefore are a bit conservative here by penalizing positions with high variance and optimizing all four workers even */
            /* though we only need returns from three to reach the 50 mineral mark */

            auto &mineralPatches = Map::getMyMain()->mineralPatches();
            std::vector<std::pair<std::array<unsigned int, 4>, unsigned int>> resourceData;
            for (auto &patch : mineralPatches)
            {
                auto &observations = WorkerMiningOptimization::resourceObservationsFor(patch);

                resourceData.emplace_back(std::array<unsigned int, 4>{
                        observations.startingWorkerObservationsFor(0).averageWithVariance(),
                        observations.startingWorkerObservationsFor(1).averageWithVariance(),
                        observations.startingWorkerObservationsFor(2).averageWithVariance(),
                        observations.startingWorkerObservationsFor(3).averageWithVariance(),
                }, observations.singleWorkerRotations.average);

                if (resourceData.back().first[0] == 0 ||
                    resourceData.back().first[1] == 0 ||
                    resourceData.back().first[2] == 0 ||
                    resourceData.back().first[3] == 0)
                {
#if WARN_ON_NO_RESOURCE_DATA
                    AppLog::Get() << "WARNING: No resource observation data available for initial split";
#endif
                    return false;
                }
            }

            std::array<int, 4> bestAssignments = {-1, -1, -1, -1};
            unsigned int bestCollectionTime = UINT32_MAX;
            unsigned int bestRotationTime = UINT32_MAX;
            for (int index1 = 0; index1 < mineralPatches.size(); index1++)
            {
                auto collection1 = resourceData[index1].first[0];

                for (int index2 = 0; index2 < mineralPatches.size(); index2++)
                {
                    if (index1 == index2) continue;

                    auto collection2 = resourceData[index2].first[1];

                    for (int index3 = 0; index3 < mineralPatches.size(); index3++)
                    {
                        if (index1 == index3) continue;
                        if (index2 == index3) continue;

                        auto collection3 = resourceData[index3].first[2];

                        for (int index4 = 0; index4 < mineralPatches.size(); index4++)
                        {
                            if (index1 == index4) continue;
                            if (index2 == index4) continue;
                            if (index3 == index4) continue;

                            auto largestCollectionTime = std::max({collection1, collection2, collection3, resourceData[index4].first[3]});
                            if (largestCollectionTime > bestCollectionTime) continue;

                            uint32_t rotationTime =
                                    resourceData[index1].second +
                                    resourceData[index2].second +
                                    resourceData[index3].second +
                                    resourceData[index4].second;
                            if (largestCollectionTime == bestCollectionTime && rotationTime >= bestRotationTime) continue;

                            bestCollectionTime = largestCollectionTime;
                            bestRotationTime = rotationTime;
                            bestAssignments[0] = index1;
                            bestAssignments[1] = index2;
                            bestAssignments[2] = index3;
                            bestAssignments[3] = index4;
                        }
                    }
                }
            }
            if (bestAssignments[0] == -1) return false;

            /* Now assign the workers appropriately */
            auto baseLoc = Map::getMyMain();
            auto startingWorkerPositions = Map::mapSpecificOverride()->startingWorkerPositions(BWAPI::Broodwar->self()->getStartLocation());
            if (startingWorkerPositions.size() != 4) return false;
            for (int i = 0; i < startingWorkerPositions.size(); i++)
            {
                /* Find the worker */
                OwnedWorker wrk = nullptr;
                for (auto &gameUnit : UnitMgr::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
                {
                    if (gameUnit->prevPos != startingWorkerPositions[i]) continue;
                    wrk = std::static_pointer_cast<MyWorkerImpl>(gameUnit);
                    break;
                }
                if (!wrk)
                {
                    AppLog::Get() << "ERROR: No starting worker found at " << startingWorkerPositions[i];
                    return false;
                }

                /* Assign it to the patch */
                workerJob[wrk] = Job::Minerals;
#if CVIS_LOG_WORKER_ASSIGNMENTS
                Telemetry::log(wrk->ident) << "Assigned to base @ " << BWAPI::WalkPosition(baseLoc->getPosition());
                Telemetry::log(wrk->ident) << "Assigned to Minerals";
                auto &observations =
                        WorkerMiningOptimization::resourceObservationsFor(mineralPatches[bestAssignments[i]]).startingWorkerObservationsFor(i);
                Telemetry::log(wrk->ident) << "Expected second collection at frame " << observations.average
                                           << " with variance " << observations.variance;
#endif

                workerBase[wrk] = baseLoc;
                baseWorkers[baseLoc].insert(wrk);
                workerMineralPatch[wrk] = mineralPatches[bestAssignments[i]];
                mineralPatchWorkers[mineralPatches[bestAssignments[i]]].insert(wrk);
            }

            AppLog::Get() << "Initial worker split made from observations; expect 7th collection before frame " << bestCollectionTime;
            return true;
        }

        void assignInitialMineralWorkers()
        {
            if (assignInitialMineralWorkersFromObservations()) return;

            auto baseLoc = Map::getMyMain();

            /* Sort the mineral patches by proximity to the nexus */
            auto mineralPatches = baseLoc->mineralPatches();
            std::sort(mineralPatches.begin(), mineralPatches.end(), [&](const Resource &a, const Resource &b) -> bool
            {
                return a->getDistance(BWAPI::UnitTypes::Protoss_Nexus, baseLoc->getPosition()) <
                       b->getDistance(BWAPI::UnitTypes::Protoss_Nexus, baseLoc->getPosition());
            });

            /* We are only interested in the first four */
            mineralPatches.resize(4);
            std::set<Resource> availablePatches(mineralPatches.begin(), mineralPatches.end());

            /* Greedily take the closest matches until all probes are assigned */
            /* TODO: Should really be optimizing for 7th collection */
            for (int i = 0; i < 4; i++)
            {
                int bestDist = INT_MAX;
                OwnedWorker bestWorker = nullptr;
                Resource bestPatch = nullptr;
                for (auto &gameUnit : UnitMgr::allMine())
                {
                    if (!gameUnit->type.isWorker()) continue;

                    auto wrk = std::static_pointer_cast<MyWorkerImpl>(gameUnit);
                    if (!wrk->completed) continue;
                    if (workerMineralPatch[wrk]) continue;

                    for (auto &patch : availablePatches)
                    {
                        int span = patch->getDistance(wrk);
                        if (span < bestDist)
                        {
                            bestDist = span;
                            bestWorker = wrk;
                            bestPatch = patch;
                        }
                    }
                }

                if (bestWorker && bestPatch)
                {
                    workerJob[bestWorker] = Job::Minerals;
#if CVIS_LOG_WORKER_ASSIGNMENTS
                    Telemetry::log(bestWorker->ident) << "Assigned to base @ " << BWAPI::WalkPosition(baseLoc->getPosition());
                    Telemetry::log(bestWorker->ident) << "Assigned to Minerals";
#endif

                    workerBase[bestWorker] = baseLoc;
                    baseWorkers[baseLoc].insert(bestWorker);
                    workerMineralPatch[bestWorker] = bestPatch;
                    mineralPatchWorkers[bestPatch].insert(bestWorker);
                    availablePatches.erase(bestPatch);
                }
            }
        }

        /* Assign a worker to the closest base with either free mineral or gas assignments depending on job */
        Base *assignBaseAndJob(const OwnedWorker &gameUnit, Job preferredJob)
        {
            /* TODO: Prioritize empty bases over nearly-full bases */

            int bestFrames = INT_MAX;
            Base *bestBase = nullptr;
            bool bestHasPreferredJob = false;
            bool bestHasNonPreferredJob = false;
            for (auto &baseLoc : Map::getMyBases())
            {
                if (!baseLoc->resourceDepot) continue;

                bool hasPreferred, hasNonPreferred;
                if (preferredJob == Job::Minerals)
                {
                    hasPreferred = availableMineralAssignmentsAtBase(baseLoc) > 0;
                    if (bestHasPreferredJob && !hasPreferred) continue;

                    hasNonPreferred = availableGasAssignmentsAtBase(baseLoc) > 0;
                }
                else
                {
                    hasPreferred = availableGasAssignmentsAtBase(baseLoc) > 0;
                    if (bestHasPreferredJob && !hasPreferred) continue;

                    hasNonPreferred = availableMineralAssignmentsAtBase(baseLoc) > 0;
                }

                if (!hasPreferred && !hasNonPreferred) continue;

                int frames = Pathfinder::ExpectedTravelTime(gameUnit->prevPos,
                                                             baseLoc->getPosition(),
                                                             gameUnit->type,
                                                             Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                             -1);
                if (frames == -1) continue;

                /* Logic if the base is far away (i.e. would require a worker transfer) */
                /* Exclusion if we don't have many workers yet, in which case this is probably our scout */
                if (frames > 500 && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Probe) > 20)
                {
                    /* Don't transfer to a threatened base */
                    if (!UnitMgr::enemyAtBase(baseLoc).empty()) continue;

                    /* Require our main army to be on the map */
                    auto mainArmyPlay = Director::getMainArmyPlay();
                    if (!mainArmyPlay) continue;

                    auto vanguardCluster = mainArmyPlay->getSquad()->vanguardCluster();
                    if (!vanguardCluster) continue;

                    if (vanguardCluster->percentageToEnemyMain < 0.6) continue;
                }

                if (!baseLoc->resourceDepot->completed)
                {
                    frames = std::max(frames, baseLoc->resourceDepot->rawUnit->getRemainingBuildTime());
                }

                if (frames < bestFrames || (hasPreferred && !bestHasPreferredJob) || (hasNonPreferred && !bestHasNonPreferredJob))
                {
                    bestFrames = frames;
                    bestBase = baseLoc;
                    bestHasPreferredJob = hasPreferred;
                    bestHasNonPreferredJob = hasNonPreferred;
                }
            }

            Job job = Job::None;
            if (bestBase)
            {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                if (workerBase[gameUnit] != bestBase)
                {
                    Telemetry::log(gameUnit->ident) << "Assigned to base @ " << BWAPI::WalkPosition(bestBase->getPosition());
                }
#endif

                workerBase[gameUnit] = bestBase;
                baseWorkers[bestBase].insert(gameUnit);

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
            if (workerJob[gameUnit] != job)
            {
                if (job == Job::Minerals)
                {
                    Telemetry::log(gameUnit->ident) << "Assigned to Minerals";
                }
                else if (job == Job::Gas)
                {
                    Telemetry::log(gameUnit->ident) << "Assigned to Gas";
                }
                else if (job == Job::None)
                {
                    Telemetry::log(gameUnit->ident) << "Assigned to None";
                }
            }
#endif

            workerJob[gameUnit] = job;

            return bestBase;
        }

        /* Assign a worker to a mineral patch in its current base */
        /* We only call this when the worker is close to the base depot, so we can assume minimal pathing issues */
        Resource assignMineralPatch(const OwnedWorker &gameUnit)
        {
            if (!workerBase[gameUnit]) return nullptr;

            /* First attempt to choose the patch using our resource observations */
            /* If any patch has no observations, we fall back to using the distance to the nexus */
            unsigned long bestRotation = ULONG_MAX;
            Resource best = nullptr;
            for (const auto &mineralPatch : workerBase[gameUnit]->mineralPatches())
            {
                size_t workers = mineralPatchWorkers[mineralPatch].size();
                if (workers >= 2) continue;

                auto &observations = WorkerMiningOptimization::resourceObservationsFor(mineralPatch);
                unsigned long rotationAverage = (workers == 0)
                        ? observations.singleWorkerRotations.average
                        : ((unsigned long)observations.doubleWorkerRotations.average * 4UL - observations.singleWorkerRotations.average);
                if (rotationAverage == 0)
                {
#if WARN_ON_NO_RESOURCE_DATA
                    AppLog::Get() << "WARNING: No resource observation data available for choosing patch";
#endif
                    best = nullptr;
                    break;
                }

                if (rotationAverage < bestRotation)
                {
                    bestRotation = rotationAverage;
                    best = mineralPatch;
                }
            }

            if (best)
            {
                workerMineralPatch[gameUnit] = best;
                mineralPatchWorkers[best].insert(gameUnit);
                return best;
            }

            int closestDist = INT_MAX;
            int furthestDist = 0;
            Resource closest = nullptr;
            Resource furthest = nullptr;
            for (const auto &mineralPatch : workerBase[gameUnit]->mineralPatches())
            {
                size_t workers = mineralPatchWorkers[mineralPatch].size();
                if (workers >= 2) continue;

                int span = mineralPatch->getDistance(BWAPI::UnitTypes::Protoss_Nexus, workerBase[gameUnit]->getPosition());
                if (workers == 0 && span < closestDist)
                {
                    closestDist = span;
                    closest = mineralPatch;
                }
                else if (workers == 1 && span > furthestDist)
                {
                    furthestDist = span;
                    furthest = mineralPatch;
                }
            }

            best = closest ? closest : furthest;
            if (best)
            {
                workerMineralPatch[gameUnit] = best;
                mineralPatchWorkers[best].insert(gameUnit);
            }

            return best;
        }

        /* Assign a worker to a refinery in its current base */
        /* We only call this when the worker is close to the base depot, so we can assume minimal pathing issues */
        Resource assignRefinery(const OwnedWorker &gameUnit)
        {
            if (!workerBase[gameUnit]) return nullptr;

            int bestDist = INT_MAX;
            Resource bestRefinery = nullptr;
            for (const auto &geyserOrRefinery : workerBase[gameUnit]->geysersOrRefineries())
            {
                if (!geyserOrRefinery->hasMyCompletedRefinery()) continue;

                size_t workers = refineryWorkers[geyserOrRefinery].size();
                if (workers >= desiredRefineryWorkers(workerBase[gameUnit], geyserOrRefinery->tilePos)) continue;

                int span = geyserOrRefinery->getDistance(gameUnit);
                if (span < bestDist)
                {
                    bestDist = span;
                    bestRefinery = geyserOrRefinery;
                }
            }

            if (bestRefinery)
            {
                workerRefinery[gameUnit] = bestRefinery;
                refineryWorkers[bestRefinery].insert(gameUnit);
            }

            return bestRefinery;
        }

        void assignGasWorker()
        {
            auto myCompletedRefineries = UnitMgr::myCompletedRefineries();

            /* Find worker closest to an available refinery */
            int bestDist = INT_MAX;
            OwnedWorker bestWorker = nullptr;
            for (const auto &gameUnit : UnitMgr::allMine())
            {
                if (!gameUnit->type.isWorker()) continue;

                auto wrk = std::static_pointer_cast<MyWorkerImpl>(gameUnit);
                if (!isAvailableForReassignment(wrk, false, false)) continue;

                for (const auto &refinery : myCompletedRefineries)
                {
                    if (refineryWorkers[refinery].size() >= desiredRefineryWorkers(workerBase[wrk], refinery->tilePos))
                    {
                        continue;
                    }

                    /* TODO: Consider depleted geysers */

                    int span = refinery->getDistance(wrk);
                    if (span < bestDist)
                    {
                        bestDist = span;
                        bestWorker = wrk;
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
            /* Remove a gas worker at a base that has available mineral assignments */
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

    void onUnitDestroy(const Unit &gameUnit)
    {
        /* Handle lost workers */
        if (gameUnit->type.isWorker() && gameUnit->player == BWAPI::Broodwar->self())
        {
            auto wrk = std::static_pointer_cast<MyWorkerImpl>(gameUnit);
            workerLost(wrk);
        }
    }

    void onMineralPatchDestroyed(const Resource &mineralPatch)
    {
        auto iterRef = mineralPatchWorkers.find(mineralPatch);
        if (iterRef != mineralPatchWorkers.end())
        {
            for (auto &wrk : iterRef->second)
            {
                workerMineralPatch.erase(wrk);
            }
            mineralPatchWorkers.erase(iterRef);
        }
    }

    void updateAssignments()
    {
        mineralWorkerCount = 0;
        auto countMineralWorker = [](const OwnedWorker &wrk, const Resource &mineralPatch)
        {
            if (!mineralPatch) return;
            if (mineralPatch->destroyed) return;
            if (mineralPatch->getDistance(wrk) > 200) return;

            mineralWorkerCount++;
        };
        gasWorkerCount = std::make_pair(0, 0);
        auto countGasWorker = [](const OwnedWorker &wrk, const Resource &refinery)
        {
            if (!refinery) return;
            if (!refinery->hasMyCompletedRefinery()) return;
            if (refinery->getDistance(wrk) > 200) return;

            if (refinery->currentAmount <= 0)
            {
                gasWorkerCount.second++;
            }
            else
            {
                gasWorkerCount.first++;
            }
        };

        /* Special case on frame 0: try to optimize for earliest possible seventh mineral returned */
        if (frameNow == 0)
        {
            assignInitialMineralWorkers();
        }

        for (auto &gameUnit : UnitMgr::allMine())
        {
            if (!gameUnit->type.isWorker()) continue;
            auto wrk = std::static_pointer_cast<MyWorkerImpl>(gameUnit);

            if (!wrk->completed) continue;
            if (workerJob[wrk] == Job::Reserved)
            {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                Telemetry::log(wrk->ident) << "Assignment: reserved";
#endif
                continue;
            }

            if (workerJob[wrk] == Job::Minerals)
            {
                /* If the worker is already assigned to a mineral patch, we don't need to do any more */
                auto mineralPatch = workerMineralPatch[wrk];
                if (mineralPatch)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    Telemetry::log(wrk->ident) << "Assignment: mineral patch @ " << BWAPI::WalkPosition(mineralPatch->getPosition());
#endif

                    countMineralWorker(wrk, mineralPatch);
                    continue;
                }

                /* Release from assigned base if it is mined out */
                auto baseLoc = workerBase[wrk];
                if (baseLoc && availableMineralAssignmentsAtBase(baseLoc) <= 0)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                    Telemetry::log(wrk->ident) << "Removed from base @ " << BWAPI::WalkPosition(baseLoc->getPosition()) << " (mined out)";
#endif
                    baseWorkers[baseLoc].erase(wrk);
                    workerBase[wrk] = nullptr;
                    baseLoc = nullptr;
                }
            }
            else if (workerJob[wrk] == Job::Gas)
            {
                /* If the worker is already assigned to a refinery, we don't need to do any more */
                auto refinery = workerRefinery[wrk];
                if (refinery && refinery->hasMyCompletedRefinery())
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    Telemetry::log(wrk->ident) << "Assignment: refinery @ " << BWAPI::WalkPosition(refinery->getPosition());
#endif

                    countGasWorker(wrk, refinery);
                    continue;
                }
            }

            /* If the worker doesn't have an assigned base, assign it one */
            auto baseLoc = workerBase[wrk];
            if (workerJob[wrk] == Job::None || !baseLoc || !baseLoc->resourceDepot || availableMineralAssignmentsAtBase(baseLoc) <= 0)
            {
                auto newBase = assignBaseAndJob(wrk, (workerJob[wrk] == Job::Gas) ? Job::Gas : Job::Minerals);

                if (baseLoc != newBase)
                {
                    if (baseLoc)
                    {
#if CVIS_LOG_WORKER_ASSIGNMENTS
                        Telemetry::log(wrk->ident) << "Removed from base @ " << BWAPI::WalkPosition(baseLoc->getPosition()) << " (new base)";
#endif
                        baseWorkers[baseLoc].erase(wrk);
                    }
                    baseLoc = newBase;
                }

                /* Maybe we have none */
                if (!baseLoc)
                {
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    Telemetry::log(wrk->ident) << "Assignment: no base available";
#endif

                    continue;
                }
            }

            /* Assign a resource when the worker is close enough to the base */
            if (wrk->getDistance(baseLoc->getPosition()) <= 300)
            {
                if (workerJob[wrk] == Job::Minerals)
                {
                    auto mineralPatch = assignMineralPatch(wrk);
                    countMineralWorker(wrk, mineralPatch);

#if CVIS_LOG_WORKER_ASSIGNMENTS
                    if (mineralPatch)
                    {
                        Telemetry::log(wrk->ident) << "Assignment: " << *mineralPatch;
                    }
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    else
                    {
                        Telemetry::log(wrk->ident) << "Assignment: no mineral patch available";
                    }
#endif
#endif
                }
                else
                {
                    auto refinery = assignRefinery(wrk);
                    countGasWorker(wrk, refinery);

#if CVIS_LOG_WORKER_ASSIGNMENTS
                    if (refinery)
                    {
                        Telemetry::log(wrk->ident) << "Assignment: " << *refinery;
                    }
#if CVIS_LOG_WORKER_ASSIGNMENTS_VERBOSE
                    else
                    {
                        Telemetry::log(wrk->ident) << "Assignment: no refinery available";
                    }
#endif
#endif
                }
            }
        }

        /* We assign 4 workers to bottom geysers, which would confuse our producer, since the fourth worker doesn't contribute */
        /* to higher income compared to a normal geyser */
        /* So reduce our gas worker counts by one for each refinery with 4 workers assigned to it */
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

        Telemetry::setBoardValue("mineralWorkers", (std::ostringstream() << mineralWorkerCount).str());
        Telemetry::setBoardValue("gasWorkers",
                                 (std::ostringstream() << gasWorkerCount.first << ":" << gasWorkerCount.second << "+" << excessGasWorkers).str());
    }

    void issueOrders()
    {
        /* Adjust number of gas workers to desired count */
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

            auto &wrk = pair.first;

            /* Move to avoid a no-go area */
            /* Move always if there is danger, otherwise only if we aren't mining */
            if (NoGoAreas::isNoGo(wrk->getTilePosition(), NoGoAreas::TypeFilter::OnlyDanger) || (
                NoGoAreas::isNoGo(wrk->getTilePosition()) && wrk->rawUnit->getOrder() != BWAPI::Orders::MiningMinerals))
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(wrk->ident) << "Moving to avoid no-go area";
#endif
                wrk->moveTo(Boids::AvoidNoGoArea(wrk.get()));
                continue;
            }

            switch (pair.second)
            {
                case Job::Minerals:
                case Job::Gas:
                {
                    /* Skip if the worker doesn't have a valid base */
                    auto baseLoc = workerBase[wrk];
                    if (!baseLoc || !baseLoc->resourceDepot)
                    {
                        continue;
                    }

                    auto mineralPatch = workerMineralPatch[wrk];

                    /* If the worker has cargo, return it */
                    if (wrk->rawUnit->isCarryingMinerals() || wrk->rawUnit->isCarryingGas())
                    {
                        /* Handle the special case when the worker is harvesting from a base without a completed depot */
                        if (!baseLoc->resourceDepot || !baseLoc->resourceDepot->completed)
                        {
                            /* Find the nearest base with a completed depot */
                            int closestTime = INT_MAX;
                            Base *closestBase = nullptr;
                            for (auto otherBase : Map::getMyBases())
                            {
                                if (otherBase == baseLoc) continue;
                                if (!otherBase->resourceDepot || !otherBase->resourceDepot->completed) continue;

                                int time = Pathfinder::ExpectedTravelTime(wrk->prevPos,
                                                                           otherBase->getPosition(),
                                                                           wrk->type,
                                                                           Pathfinder::PathFindingOptions::Default,
                                                                           -1);
                                if (time != -1 && time < closestTime)
                                {
                                    closestTime = time;
                                    closestBase = otherBase;
                                }
                            }

                            /* If there is one, and the worker can get there and back before the base depot is complete, deliver there */
                            if (closestBase)
                            {
                                int baseToBaseTime = Pathfinder::ExpectedTravelTime(baseLoc->getPosition(),
                                                                                     closestBase->getPosition(),
                                                                                     BWAPI::UnitTypes::Protoss_Probe,
                                                                                     Pathfinder::PathFindingOptions::Default,
                                                                                     -1);
                                if (baseToBaseTime != -1 && closestTime + baseToBaseTime < baseLoc->resourceDepot->rawUnit->getRemainingBuildTime())
                                {
                                    if (wrk->getDistance(closestBase->resourceDepot) > 200)
                                    {
                                        wrk->moveTo(closestBase->getPosition());
                                    }
                                    else if (wrk->rawUnit->getOrder() != BWAPI::Orders::ReturnMinerals &&
                                             wrk->rawUnit->getOrder() != BWAPI::Orders::ReturnGas &&
                                             wrk->lastCommandFrame > (frameNow - BWAPI::Broodwar->getLatencyFrames()))
                                    {
                                        wrk->rightClick(closestBase->resourceDepot->rawUnit);
                                    }
                                    else if (wrk->rawUnit->isCarryingMinerals())
                                    {
                                        auto myDepot = std::dynamic_pointer_cast<MyUnitImpl>(closestBase->resourceDepot);
                                        if (myDepot)
                                        {
                                            WorkerMiningOptimization::optimizeReturnOfResource(wrk, myDepot, mineralPatch);
                                        }
                                    }
                                    continue;
                                }
                            }

                            /* There wasn't another base to return to, or it would take too long, so move towards the preferred base instead */
                            if (baseLoc->resourceDepot)
                            {
                                wrk->moveTo(baseLoc->getPosition());
                            }
                            else
                            {
                                wrk->moveTo(baseLoc->mineralLineCenter);
                            }
                            continue;
                        }

                        /* Leave it alone if it already has the return order or is resetting after finishing gathering */
                        if (wrk->rawUnit->getOrder() == BWAPI::Orders::ReturnMinerals ||
                            wrk->rawUnit->getOrder() == BWAPI::Orders::ReturnGas ||
                            wrk->rawUnit->getOrder() == BWAPI::Orders::ResetCollision)
                        {
                            auto myDepot = std::dynamic_pointer_cast<MyUnitImpl>(baseLoc->resourceDepot);
                            if (myDepot)
                            {
                                WorkerMiningOptimization::optimizeReturnOfResource(wrk, myDepot, mineralPatch);
                            }
                            continue;
                        }

                        /* Leave it alone if we have just ordered it to do something, as that indicates we are trying to optimize */
                        if (wrk->lastCommandFrame > (frameNow - BWAPI::Broodwar->getLatencyFrames()))
                        {
                            continue;
                        }

                        wrk->returnCargo();
                        continue;
                    }

                    /* Handle mining from an assigned mineral patch */
                    if (mineralPatch)
                    {
                        /* If the unit is currently mining, leave it alone */
                        /* ResetCollision happens both on the frame where mining completes and LF after issuing a gather command, so we need */
                        /* to differentiate there */
                        if (wrk->rawUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
                            wrk->rawUnit->getOrder() == BWAPI::Orders::ReturnMinerals ||
                            (wrk->rawUnit->getOrder() == BWAPI::Orders::ResetCollision &&
                             (frameNow - wrk->lastCommandFrame - 1) > BWAPI::Broodwar->getLatencyFrames()))
                        {
                            continue;
                        }

                        /* If we don't have vision on the mineral patch, move towards it */
                        auto rawUnit = mineralPatch->getBwapiUnitIfVisible();
                        if (!rawUnit)
                        {
                            wrk->moveTo(mineralPatch->midpoint);
                            continue;
                        }

                        /* If the unit hasn't been ordered to gather, order it to do so */
                        if (wrk->rawUnit->getOrder() != BWAPI::Orders::MoveToMinerals &&
                            wrk->rawUnit->getOrder() != BWAPI::Orders::WaitForMinerals &&
                            ((wrk->lastCommandFrame < (frameNow - BWAPI::Broodwar->getLatencyFrames() - 1))
                             || wrk->rawUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Gather))
                        {
                            Telemetry::log(wrk->ident) << "hasn't been ordered to gather; order is " << wrk->rawUnit->getOrder();
                            wrk->gather(rawUnit);
                            continue;
                        }

                        auto myDepot = std::dynamic_pointer_cast<MyUnitImpl>(baseLoc->resourceDepot);
                        if (myDepot)
                        {
                            WorkerMiningOptimization::optimizeStartOfMining(wrk, myDepot, mineralPatch);
                        }
                        continue;
                    }

                    /* Handle gathering from an assigned refinery */
                    auto refinery = workerRefinery[wrk];
                    if (refinery && refinery->hasMyCompletedRefinery() && refinery->getDistance(wrk) < 500)
                    {
                        /* If the unit is currently gathering, leave it alone */
                        if (wrk->rawUnit->getOrder() == BWAPI::Orders::MoveToGas ||
                            wrk->rawUnit->getOrder() == BWAPI::Orders::WaitForGas ||
                            wrk->rawUnit->getOrder() == BWAPI::Orders::HarvestGas ||
                            wrk->rawUnit->getOrder() == BWAPI::Orders::Harvest1 ||
                            wrk->rawUnit->getOrder() == BWAPI::Orders::ReturnGas)
                        {
                            continue;
                        }

                        /* Otherwise click on the refinery */
                        auto rawUnit = refinery->getBwapiUnitIfVisible();
                        if (rawUnit)
                        {
                            wrk->gather(rawUnit);
                            continue;
                        }
                    }

                    /* If the worker is a long way from its base, move towards it */
                    if (wrk->getDistance(baseLoc->getPosition()) > 200)
                    {
#if DEBUG_UNIT_ORDERS
                        Telemetry::log(wrk->ident) << "moveTo: Assigned base (far away)";
#endif
                        wrk->moveTo(baseLoc->getPosition());
                        continue;
                    }

                    /* For some reason the worker doesn't have anything to do */
                    /* Clear its state so it gets a new assignment */
                    removeFromResource(wrk, workerMineralPatch, mineralPatchWorkers);
                    removeFromResource(wrk, workerRefinery, refineryWorkers);
#if DEBUG_UNIT_ORDERS
                    Telemetry::log(wrk->ident) << "Worker has nothing to do, clearing state";
#endif
                    wrk->moveTo(baseLoc->getPosition());
                    break;
                }
                case Job::None:
                {
                    /* Move towards the base if we aren't near it */
                    auto baseLoc = workerBase[wrk];
                    if (!baseLoc || !baseLoc->resourceDepot)
                    {
                        continue;
                    }

                    int span = wrk->getDistance(baseLoc->getPosition());
                    if (span > 200)
                    {
                        wrk->moveTo(baseLoc->getPosition());
                    }
                }
                default:
                {
                    /* Nothing needed for other cases */
                    break;
                }
            }
        }
    }

    bool isAvailableForReassignment(const OwnedWorker &gameUnit, bool allowCarryMinerals, bool allowMining)
    {
        if (!gameUnit || !gameUnit->exists() || !gameUnit->completed || !gameUnit->type.isWorker()) return false;

        auto job = workerJob[gameUnit];
        if (job == Job::None) return true;
        if (job == Job::Minerals)
        {
            if (gameUnit->rawUnit->isCarryingGas()) return false;
            if (!allowCarryMinerals && gameUnit->rawUnit->isCarryingMinerals()) return false;

            /* We allow a mining worker if it has only been mining for less than 10 frames */
            if (!allowMining && gameUnit->rawUnit->getOrder() == BWAPI::Orders::MiningMinerals && (frameNow - gameUnit->lastStartedMining) > 10)
            {
                return false;
            }

            return true;
        }

        return false;
    }

    OwnedWorker getClosestReassignableWorker(BWAPI::Position position, bool allowCarryMinerals, int *bestTravelTime)
    {
        int bestTime = INT_MAX;
        int bestScore = INT_MAX;
        OwnedWorker bestWorker = nullptr;
        for (auto &gameUnit : UnitMgr::allMine())
        {
            if (!gameUnit->exists()) continue;
            if (!gameUnit->type.isWorker()) continue;
            auto wrk = std::static_pointer_cast<MyWorkerImpl>(gameUnit);

            if (!isAvailableForReassignment(wrk, allowCarryMinerals, true)) continue;

            int travelTime =
                    Pathfinder::ExpectedTravelTime(wrk->prevPos,
                                                    position,
                                                    wrk->type,
                                                    Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                    -1);

            /* Disallow carrying minerals in all cases if the travel time is excessive */
            /* Rationale: we might be sending the unit to build something at another base and we want to return minerals first */
            if (travelTime > 100 && wrk->rawUnit->isCarryingMinerals()) continue;

            /* If the unit is currently mining, penalize it by 3 seconds to encourage selecting other workers */
            int score = travelTime;
            if (wrk->rawUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
                wrk->rawUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
            {
                score += 72;
            }

            /* If the unit is currently unassigned, give it a 4 second bonus to encourage selecting it */
            if (workerJob[wrk] == Job::None) score -= 96;

            if (travelTime != -1 && score < bestScore)
            {
                bestTime = travelTime;
                bestScore = score;
                bestWorker = wrk;
            }
        }

        if (bestTravelTime) *bestTravelTime = bestTime;
        return bestWorker;
    }

    size_t getBaseWorkerCount(Base *baseLoc)
    {
        auto baseAndWorkersIt = baseWorkers.find(baseLoc);
        if (baseAndWorkersIt == baseWorkers.end()) return 0;

        return baseAndWorkersIt->second.size();
    }

    std::vector<OwnedWorker> getBaseWorkers(Base *baseLoc)
    {
        std::vector<OwnedWorker> outcome;

        auto baseAndWorkersIt = baseWorkers.find(baseLoc);
        if (baseAndWorkersIt == baseWorkers.end()) return outcome;

        outcome.reserve(baseAndWorkersIt->second.size());
        for (auto &wrk : baseAndWorkersIt->second)
        {
            outcome.push_back(wrk);
        }
        return outcome;
    }

    int baseMineralWorkerCount(Base *baseLoc)
    {
        int outcome = 0;

        auto baseAndWorkersIt = baseWorkers.find(baseLoc);
        if (baseAndWorkersIt == baseWorkers.end()) return outcome;

        for (auto &wrk : baseAndWorkersIt->second)
        {
            auto iterRef = workerJob.find(wrk);
            if (iterRef != workerJob.end() && iterRef->second == Job::Minerals) outcome++;
        }
        return outcome;
    }

    void reserveBaseWorkers(std::vector<OwnedWorker> &workers, Base *baseLoc)
    {
        auto baseAndWorkersIt = baseWorkers.find(baseLoc);
        if (baseAndWorkersIt == baseWorkers.end()) return;

        for (auto &wrk : baseAndWorkersIt->second)
        {
            workers.push_back(wrk);
        }

        for (auto &wrk : workers)
        {
            reserveWorker(wrk);
        }
    }

    void reserveWorker(const OwnedWorker &gameUnit)
    {
        if (!gameUnit || !gameUnit->exists() || !gameUnit->type.isWorker() || !gameUnit->completed) return;

        if (workerJob[gameUnit] == Job::Reserved) return;

        workerJob[gameUnit] = Job::Reserved;
        removeFromResource(gameUnit, workerMineralPatch, mineralPatchWorkers);
        removeFromResource(gameUnit, workerRefinery, refineryWorkers);
        Telemetry::log(gameUnit->ident) << "Reserved for non-mining duties";
    }

    void releaseWorker(const OwnedWorker &gameUnit)
    {
        if (!gameUnit || !gameUnit->exists() || !gameUnit->type.isWorker() || !gameUnit->completed || workerJob[gameUnit] != Job::Reserved) return;

        workerJob[gameUnit] = Job::None;
        Telemetry::log(gameUnit->ident) << "Released from non-mining duties";
    }

    int availableMineralAssignments(Base *baseLoc, int workersPerPatch)
    {
        if (baseLoc) return availableMineralAssignmentsAtBase(baseLoc, workersPerPatch);

        int count = 0;
        for (auto &myBase : Map::getMyBases())
        {
            count += availableMineralAssignmentsAtBase(myBase, workersPerPatch);
        }

        return count;
    }

    int availableGasAssignments(Base *baseLoc)
    {
        if (baseLoc) return availableGasAssignmentsAtBase(baseLoc);

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
        /* Do an initial scan to find the number of gas slots and available mineral workers there are at each base */
        std::vector<std::tuple<Base *, int, int>> basesAndGasSlotsAndMineralWorkersAvailable;
        for (auto &baseAndWorkers : baseWorkers)
        {
            if (!baseAndWorkers.first || baseAndWorkers.first->owner != BWAPI::Broodwar->self()) continue;
            if (!baseAndWorkers.first->resourceDepot) return 0;

            int gasAvailable = 0;
            for (const auto &refinery : baseAndWorkers.first->geysersOrRefineries())
            {
                if (refinery->hasMyCompletedRefinery())
                {
                    gasAvailable += desiredRefineryWorkers(baseAndWorkers.first, refinery->tilePos);
                }
            }

            int mineralWorkersAvailable = 0;
            for (const auto &wrk : baseAndWorkers.second)
            {
                if (workerJob[wrk] == Job::Minerals) mineralWorkersAvailable++;
                if (workerJob[wrk] == Job::Gas) gasAvailable--;
            }

            basesAndGasSlotsAndMineralWorkersAvailable.emplace_back(baseAndWorkers.first, std::max(gasAvailable, 0), mineralWorkersAvailable);
        }

        /* Now count the number of mineral workers that can be transferred to gas, allowing transfer to close bases */
        int outcome = 0;
        for (auto &[baseLoc, gasAvailable, mineralWorkersAvailable] : basesAndGasSlotsAndMineralWorkersAvailable)
        {
            if (gasAvailable == 0) continue;
            if (mineralWorkersAvailable >= gasAvailable)
            {
                outcome += gasAvailable;
                continue;
            }

            /* Check if we can borrow mineral workers from a nearby base */
            int borrowedMineralWorkers = 0;
            for (auto &[otherBase, otherGasAvailable, otherMineralWorkersAvailable] : basesAndGasSlotsAndMineralWorkersAvailable)
            {
                if (baseLoc == otherBase) continue;
                if (otherMineralWorkersAvailable <= otherGasAvailable) continue;
                int frames = Pathfinder::ExpectedTravelTime(baseLoc->getPosition(),
                                                             otherBase->getPosition(),
                                                             BWAPI::UnitTypes::Protoss_Probe,
                                                             Pathfinder::PathFindingOptions::Default,
                                                             -1);
                if (frames == -1) continue;
                if (frames < 400)
                {
                    borrowedMineralWorkers += (otherMineralWorkersAvailable - otherGasAvailable);
                }
            }

            outcome += std::min(gasAvailable, mineralWorkersAvailable + borrowedMineralWorkers);
        }

        return outcome;
    }

    int reassignableGasWorkers()
    {
        size_t outcome = 0;
        for (auto &baseAndWorkers : baseWorkers)
        {
            if (!baseAndWorkers.first || baseAndWorkers.first->owner != BWAPI::Broodwar->self()) continue;
            if (!baseAndWorkers.first->resourceDepot) return 0;

            size_t mineralsAvailable = baseAndWorkers.first->mineralPatchCount() * 2;
            if (mineralsAvailable == 0) continue;

            size_t gasWorkersAvailable = 0;
            for (const auto &wrk : baseAndWorkers.second)
            {
                if (workerJob[wrk] == Job::Gas) gasWorkersAvailable++;
                if (workerJob[wrk] == Job::Minerals) mineralsAvailable--;
            }

            outcome += std::min(mineralsAvailable, gasWorkersAvailable);
        }

        return (int)outcome;
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

    std::map<Resource, std::set<OwnedWorker>> &mineralsAndAssignedWorkers()
    {
        return mineralPatchWorkers;
    }

    std::set<OwnedWorker> getWorkersAssignedTo(const Resource &resource)
    {
        if (resource->isMinerals)
        {
            return mineralPatchWorkers[resource];
        }
        return refineryWorkers[resource];
    }

    OwnedWorker getOtherWorkerMining(const Resource &resource, const OwnedWorker &wrk)
    {
        for (const auto &gameUnit : mineralPatchWorkers[resource])
        {
            if (gameUnit != wrk && gameUnit->exists()) return gameUnit;
        }
        return nullptr;
    }

    void setWorkerMineralPatch(const OwnedWorker &wrk, const Resource &resource, Base *baseLoc)
    {
        removeFromResource(wrk, workerMineralPatch, mineralPatchWorkers);

        workerBase[wrk] = baseLoc;
        baseWorkers[baseLoc].insert(wrk);
        workerMineralPatch[wrk] = resource;
        mineralPatchWorkers[resource].insert(wrk);

#if CVIS_LOG_WORKER_ASSIGNMENTS
        Telemetry::log(wrk->ident) << "Forced reassignment to " << *resource << " in base @ " << BWAPI::WalkPosition(baseLoc->getPosition());
#endif
    }
}
