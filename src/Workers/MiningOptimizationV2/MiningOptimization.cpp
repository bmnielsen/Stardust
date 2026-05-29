#include "MiningOptimization.h"

#include "DataModel/Serialization.h"
#include "PathOptimizer.h"

#include "DebugFlag_MiningOptimization.h"
#include "Map.h"
#include "Units.h"
#include "Workers.h"
#include "Takeover/PatchOccupiedForecast.h"

#if OUTPUT_STATISTICS
#include "PathStatistics.h"
#endif

#define VALIDATE_PATCH_OCCUPIED_FORECAST false

namespace MiningOptimization
{
    namespace
    {
        MapData mapData;

        std::unique_ptr<PathOptimizer<GatherArrivalData>> gatherOptimizer;
        std::unique_ptr<PathOptimizer<ReturnArrivalData>> returnOptimizer;

#if OUTPUT_STATISTICS
        PathStatistics gatherPathStatistics;
        PathStatistics returnPathStatistics;
#endif

#if VALIDATE_PATCH_OCCUPIED_FORECAST
        std::map<Resource, PatchOccupiedForecast> previousFramePatchOccupiedForecast;
#endif
    }

    void initialize()
    {
        Serialization::setGameParameters(BWAPI::Broodwar->mapHash());
        Serialization::readMapData(mapData);

        gatherOptimizer = std::make_unique<PathOptimizer<GatherArrivalData>>(mapData, mapData.resourceToSerializedGatherPaths);
        returnOptimizer = std::make_unique<PathOptimizer<ReturnArrivalData>>(mapData, mapData.resourceToSerializedReturnPaths);

#if OUTPUT_STATISTICS
        gatherPathStatistics.reset();
        returnPathStatistics.reset();
#endif
    }

    void update()
    {
        gatherOptimizer->clearDeadWorkers();
        returnOptimizer->clearDeadWorkers();

#if OUTPUT_STATISTICS
        gatherOptimizer->updateStatistics(gatherPathStatistics);
        returnOptimizer->updateStatistics(returnPathStatistics);
#endif

#if VALIDATE_PATCH_OCCUPIED_FORECAST
        for (const auto &base : Map::allBases())
        {
            for (const auto &patch : base->mineralPatches())
            {
                auto it = previousFramePatchOccupiedForecast.find(patch);
                if (it != previousFramePatchOccupiedForecast.end())
                {
                    auto &previousFrameForecast = it->second;
                    MyWorker hasJustStartedMining = nullptr;
                    MyWorker isMining = nullptr;
                    MyWorker hasJustFinishedMining = nullptr;
                    for (auto &worker : Workers::getWorkersAssignedTo(patch))
                    {
                        if (worker->lastTransitionedToMiningOrder == currentFrame)
                        {
                            hasJustStartedMining = worker;
                        }
                        else if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                        {
                            isMining = worker;
                        }
                        if (worker->carryingResource && worker->lastCarryingResourceChange == currentFrame) hasJustFinishedMining = worker;
                    }
                    if (hasJustStartedMining && previousFrameForecast.atFrame(currentFrame) < 0.0001)
                    {
                        Log::Get() << "ERROR: Forecast says probability is 0 but worker just started mining; " << *hasJustStartedMining;
                    }
                    if (isMining && previousFrameForecast.atFrame(currentFrame) < 0.0001)
                    {
                        Log::Get() << "ERROR: Forecast says probability is 0 but worker is mining; " << *isMining;
                    }
                    if (hasJustFinishedMining && !hasJustStartedMining && previousFrameForecast.atFrame(currentFrame) > 0.9999)
                    {
                        Log::Get() << "ERROR: Forecast says probability is 1 but worker has just finished mining; " << *hasJustFinishedMining;
                    }
                    if (!isMining && !hasJustStartedMining && previousFrameForecast.atFrame(currentFrame) > 0.9999)
                    {
                        Log::Get() << "ERROR: Forecast says probability is 1 but no worker is mining; " << *patch;
                    }
                }

                auto forecast = PatchOccupiedForecast(patch);
                CherryVis::log(patch->id) << forecast;
                previousFramePatchOccupiedForecast.erase(patch);
                previousFramePatchOccupiedForecast.emplace(patch, std::move(forecast));
            }
        }
#endif
    }

    void gameEnd()
    {
#if OUTPUT_STATISTICS
        auto outputStatistics = [](const PathStatistics &pathStatistics)
        {
            if (pathStatistics.count == 0) return (std::string)"No Data";

            std::ostringstream out;
            out << std::fixed << std::setprecision(1)
                << pathStatistics.count << " collections, "
                << pathStatistics.withPath << " with path data"
                << " (" << ((double)pathStatistics.withPath * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withPathFollowedToStableResult << " with path followed to stable"
                << " (" << ((double)pathStatistics.withPathFollowedToStableResult * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withPathFollowedToCompletion << " with path followed to completion"
                << " (" << ((double)pathStatistics.withPathFollowedToCompletion * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withExpectedArrivalFrame << " with expected arrival frame"
                << " (" << ((double)pathStatistics.withExpectedArrivalFrame * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withExpectedActionFrame << " with expected action frame"
                << " (" << ((double)pathStatistics.withExpectedActionFrame * 100.0 / (double)pathStatistics.count) << "%)";
            if (pathStatistics.withTakeover > 0)
            {
                out << ", " << pathStatistics.withTakeover << " takeover collections, "
                    << pathStatistics.patchSwitches << " with patch switch"
                    << " (" << ((double)pathStatistics.patchSwitches * 100.0 / (double)pathStatistics.withTakeover) << "%), "
                    << pathStatistics.withPlannedPatchLock << " with planned patch lock"
                    << " (" << ((double)pathStatistics.withPlannedPatchLock * 100.0 / (double)pathStatistics.withTakeover) << "%)";
                if (pathStatistics.withPlannedPatchLock > 0)
                {
                    out << ", " << pathStatistics.withExpectedPatchLockFrame << " with expected patch lock frame"
                        << " (" << ((double)pathStatistics.withExpectedPatchLockFrame * 100.0 / (double)pathStatistics.withPlannedPatchLock) << "%)";
                }
            }
            return out.str();
        };
        Log::Get() << "Gather path statistics: " << outputStatistics(gatherPathStatistics);
        Log::Get() << "Return path statistics: " << outputStatistics(returnPathStatistics);
#endif
    }

    bool optimizeStartOfMining(Base *base, std::vector<std::tuple<MyWorker, MyUnit, Resource>> &workersAndDepotsAndResources)
    {
        // Loop through the given workers, update their paths, and check if any require takeover optimization
        std::vector<WorkerPathOptimizer<GatherArrivalData>*> takeoverWorkers;
        for (auto it = workersAndDepotsAndResources.begin(); it != workersAndDepotsAndResources.end(); )
        {
            auto &[worker, depot, patch] = *it;
            auto &workerOptimizer = gatherOptimizer->forWorker(worker, depot, patch);
            if (workerOptimizer.updatePath())
            {
                // Detect takeover by checking if there is another worker already mining
                auto otherWorker = Workers::getOtherWorkerMining(patch, worker);
                if (otherWorker && otherWorker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    takeoverWorkers.push_back(&workerOptimizer);
                    ++it;
                    continue;
                }

                // There is no other worker mining the patch, so we can just issue the approach orders normally
                workerOptimizer.issueOrders();
            }

            // This worker is either not pathing properly or isn't taking over, so we don't need to consider it any further
            // TODO: Consider how to handle non-pathed workers that have reached the patch
            it = workersAndDepotsAndResources.erase(it);
        }

        // Nothing further is needed if none of the workers are taking over
        if (takeoverWorkers.empty()) return true;

        // At this point we have one or more workers that are approaching their patches where another worker is mining
        // Our priorities are to try to have them patch lock, or barring that, have them take over mining at the optimal frame

        std::map<Resource, PatchOccupiedForecast> patchToOccupiedForecast;

        // Start by planning a path to the takeover frame for each worker
        // Along the way we will initialize the occupied forecast for each assigned patch, giving us the takeover data
        // Any workers that cannot reach the takeover frame will be removed at this phase, since they won't be able to patch lock
        for (auto it = takeoverWorkers.begin(); it != takeoverWorkers.end(); )
        {
            auto &workerOptimizer = **it;

            auto &forecast = patchToOccupiedForecast.emplace(workerOptimizer.resource, workerOptimizer.resource).first->second;
            if (!forecast.miningWorkerLatestEndFrame)
            {
                Log::Get() << "ERROR: Forecast doesn't contain miningWorkerLatestEndFrame; " << *workerOptimizer.worker;
                workerOptimizer.issueOrders();
                it = takeoverWorkers.erase(it);
                continue;
            }

            // The takeover frame will be when the mining worker finishes mining, plus an extra frame if the taking over worker's orders
            // are processed first
            int desiredTakeoverFrame = *forecast.miningWorkerLatestEndFrame;
            if (workerOptimizer.worker->orderProcessIndex >= *forecast.miningWorkerOrderProcessIndex) ++desiredTakeoverFrame;

            // Get the set of frames the worker can achieve
            // These are based on the arrival frame along with the possibilities to resend prior to arrival
            auto takeoverActionFrames = workerOptimizer.takeoverActionFrames(desiredTakeoverFrame);

            // Pick the earliest action frame after the takeover frame that is guaranteed to succeed
            int earliestTakeoverFrame = INT_MAX;
            for (const auto &[frame, probability] : takeoverActionFrames)
            {
                if (probability > 0.999)
                {
                    earliestTakeoverFrame = std::min(earliestTakeoverFrame, frame);
                }
            }

            // Update the forecast using this frame
            if (earliestTakeoverFrame != INT_MAX)
            {
                forecast.useTakeoverFrame(earliestTakeoverFrame);
            }

            // If the worker can't arrive before the desired takeover frame, remove it now since it won't be able to patch lock
            if (earliestTakeoverFrame == INT_MAX || takeoverActionFrames.begin()->first >= desiredTakeoverFrame)
            {
                workerOptimizer.issueOrders();
                it = takeoverWorkers.erase(it);
                continue;
            }

            ++it;
        }

        // Nothing further is needed if none of the workers can possibly patch lock
        if (takeoverWorkers.empty()) return true;

        // Initialize the occupied forecast for any patches not already covered
        for (auto &patch : base->mineralPatches())
        {
            if (!patchToOccupiedForecast.contains(patch)) patchToOccupiedForecast.emplace(patch, patch);
        }

        // Now we repeatedly try to find earlier frames where a worker can patch lock and use this to update the forecasts
        // Because a worker patch locking might open up for another worker to patch lock, we repeat this as long as any worker has updated

        // TODO: Patch locking logic

        for (auto &workerOptimizer : takeoverWorkers)
        {
            workerOptimizer->issueOrders();
        }

        return true;
    }

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        // This happens for the final return after a patch has been mined out - we should really still be able to optimize, but it's probably not
        // worth the effort to refactor it
        if (!resource) return;

        auto &pathOptimizer = returnOptimizer->forWorker(worker, depot, resource);
        if (pathOptimizer.updatePath())
        {
            pathOptimizer.issueOrders();
        }
    }

    std::map<MyWorker, std::tuple<Resource, Resource, std::optional<InitialSplitData>>> initialWorkerSplit()
    {
        // Gather the patches and workers
        std::vector<Resource> patches = Map::getMyMain()->mineralPatches();
        std::vector<MyWorker> workers;
        for (auto unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
        {
            workers.emplace_back(std::static_pointer_cast<MyWorkerImpl>(unit));
        }

        // Sort the patches and workers by position to ensure stability between games
        std::sort(patches.begin(), patches.end(), [](const Resource &first, const Resource &second)
        {
            return first->tile < second->tile;
        });
        std::sort(workers.begin(), workers.end(), [](const MyWorker &first, const MyWorker &second)
        {
            return first->lastPosition < second->lastPosition;
        });

        if (workers.size() != 4)
        {
            Log::Get() << "ERROR: Don't have 4 workers when trying to run initial split";
            return {};
        }

        // Generate the PositionAndVelocity for each worker
        std::vector<PositionAndVelocity> workerPositionAndVelocity;
        for (const auto &worker : workers)
        {
            workerPositionAndVelocity.emplace_back(worker);
        }

        // Reference the appropriate initial split data depending on what we know about the opponent's race
        auto getInitialSplitDataForRace = [](BWAPI::Race opponentRace)
            -> std::unordered_map<PositionAndVelocity, std::map<std::pair<TilePosition, TilePosition>, InitialSplitData>>&
        {
            if (opponentRace == BWAPI::Races::Zerg)
            {
                return mapData.startLocationToPatchPairToInitialSplitDataZerg;
            }
            if (opponentRace == BWAPI::Races::Protoss || opponentRace == BWAPI::Races::Terran)
            {
                return mapData.startLocationToPatchPairToInitialSplitDataNotZerg;
            }
            return mapData.startLocationToPatchPairToInitialSplitDataUnknown;
        };
        auto &initialSplitData = getInitialSplitDataForRace(BWAPI::Broodwar->enemy()->getRace());

        // Generate combinations for all four workers to find the best solution
        unsigned int bestSeventhDelivery = UINT_MAX;
        unsigned int bestEighthDelivery = UINT_MAX;
        std::array<std::pair<TilePosition, TilePosition>, 4> bestSolution;
        for (auto &[firstWorkerAssignment, firstWorkerResult]
                : initialSplitData[workerPositionAndVelocity[0]])
        {
            for (auto &[secondWorkerAssignment, secondWorkerResult]
                    : initialSplitData[workerPositionAndVelocity[1]])
            {
                if (secondWorkerAssignment.first == firstWorkerAssignment.first) continue;
                if (secondWorkerAssignment.second == firstWorkerAssignment.second) continue;

                for (auto &[thirdWorkerAssignment, thirdWorkerResult]
                        : initialSplitData[workerPositionAndVelocity[2]])
                {
                    if (thirdWorkerAssignment.first == firstWorkerAssignment.first) continue;
                    if (thirdWorkerAssignment.second == firstWorkerAssignment.second) continue;
                    if (thirdWorkerAssignment.first == secondWorkerAssignment.first) continue;
                    if (thirdWorkerAssignment.second == secondWorkerAssignment.second) continue;

                    for (auto &[fourthWorkerAssignment, fourthWorkerResult]
                            : initialSplitData[workerPositionAndVelocity[3]])
                    {
                        if (fourthWorkerAssignment.first == firstWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == firstWorkerAssignment.second) continue;
                        if (fourthWorkerAssignment.first == secondWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == secondWorkerAssignment.second) continue;
                        if (fourthWorkerAssignment.first == thirdWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == thirdWorkerAssignment.second) continue;

                        std::multiset<unsigned int> result = {
                            firstWorkerResult.score(),
                            secondWorkerResult.score(),
                            thirdWorkerResult.score(),
                            fourthWorkerResult.score()
                        };

                        unsigned int seventhDelivery = *(std::prev(result.end(), 2));
                        unsigned int eighthDelivery = *result.rbegin();
                        if (seventhDelivery < bestSeventhDelivery || (seventhDelivery == bestSeventhDelivery && eighthDelivery < bestEighthDelivery))
                        {
                            bestSeventhDelivery = seventhDelivery;
                            bestEighthDelivery = eighthDelivery;
                            bestSolution = {
                                firstWorkerAssignment,
                                secondWorkerAssignment,
                                thirdWorkerAssignment,
                                fourthWorkerAssignment,
                            };
                        }
                    }
                }
            }
        }

        if (bestSeventhDelivery == UINT_MAX) return {};

        std::map<TilePosition, Resource> tileToPatch;
        for (auto &patch : patches)
        {
            tileToPatch[TilePosition::fromBWAPI(patch->tile)] = patch;
        }

        auto assignmentToTuple = [&](size_t workerIndex, std::pair<TilePosition, TilePosition> patchPair)
        {
            return std::make_tuple(
                tileToPatch[patchPair.first],
                tileToPatch[patchPair.second],
                initialSplitData[workerPositionAndVelocity[workerIndex]][patchPair]);
        };

        std::map<MyWorker, std::tuple<Resource, Resource, std::optional<InitialSplitData>>> result;
        for (size_t workerIndex = 0; workerIndex < 4; workerIndex++)
        {
            result.emplace(workers[workerIndex], assignmentToTuple(workerIndex, bestSolution[workerIndex]));
        }
        return result;
    }

    std::optional<int> averageRotationTimeFor(const Resource &resource)
    {
        return std::nullopt;
    }
}
