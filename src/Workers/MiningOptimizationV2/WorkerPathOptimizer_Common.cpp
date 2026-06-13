#include "WorkerPathOptimizer.h"

#include "Solver/Solver.h"

#include "DebugFlag_MiningOptimization.h"
#include "OrderProcessTimer.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define LOG_PATH_FAILURES false
#endif

namespace MiningOptimization
{
    namespace
    {
        PositionAndVelocity getCurrentPosition(const MyWorker &worker)
        {
            PositionAndVelocity currentPosition(worker);

            // If delivery happens on the frame immediately after arrival, the worker may still be reporting some residual velocity
            // that will be removed on the next frame and therefore should not be considered when trying to match path nodes
            if (worker->lastCarryingResourceChange == currentFrame && worker->frameLastMoved != currentFrame)
            {
                currentPosition.velocityX = 0;
                currentPosition.velocityY = 0;
            }

            return currentPosition;
        }
    }

    template <typename ObservationType>
    bool WorkerPathOptimizer<ObservationType>::updatePath()
    {
        if (skipPathOptimization()) return false;

        // If the worker hasn't been optimized last frame, reset the state and initialize the pathing
        if (lastProcessedFrame != (currentFrame - 1))
        {
            reset();
            setStartOfPathFlags();

            // If a root node for pathing is here, run the solver to plan our approach
            auto currentPosition = getCurrentPosition(worker);

#if VERBOSE_PATH_LOGGING
            CherryVis::log(worker->id) << "Trying to find root node @ " << currentPosition;
#endif

            auto path = pathCache.get(std::make_pair(patchTile, currentPosition));
            if (path)
            {
                setFlag(StatusFlags::CapturedPath);
                Solver<ObservationType> solver(mapData,
                                               resource,
                                               currentPosition,
                                               *path->path,
                                               currentFrame,
                                               worker->possibleOrderProcessTimerValues);
                expectedPath = std::make_unique<SolverResult<ObservationType>>(solver.execute());

#if VERBOSE_PATH_LOGGING
                CherryVis::log(worker->id) << "Captured path and ran solver; predicted frames:\n" << expectedPath->framePredictions();
#if IS_OPENBW
                CherryVis::log(worker->id) << worker->bwapiUnit->getExactPosition();
#endif
#endif

                pathCache.put(std::move(*path));
            }

#if IS_OPENBW
            startPosition = std::make_unique<BWAPI::ExactPosition>(worker->bwapiUnit->getExactPosition());
#endif
        }
        lastProcessedFrame = currentFrame;

        // Nothing to do if we don't have any path data
        if (!expectedPath)
        {
            // For gather, we return true if we are close to the patch to enable takeover optimization
            if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
            {
                if (resource->getDistance(worker) <= ASSUME_RESEND_ALWAYS_ARRIVES_DISTANCE) return true;
            }

            return false;
        }

        // Validate 10-distance on gather
        if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
        {
            if (!hasFlag(StatusFlags::ReachedTenDistance))
            {
                int dist = resource->getDistance(worker);
                if (dist <= 10)
                {
                    setFlag(StatusFlags::ReachedTenDistance);

#if LOGGING_ENABLED
                    if (!hasFlag(StatusFlags::LostPath))
                    {
                        int maxTenDistanceFrame = 0;
                        int minTenDistanceFrame = INT_MAX;
                        for (const auto &[arrivalData, _] : expectedPath->arrivalDataWithProbabilities)
                        {
                            minTenDistanceFrame = std::min(minTenDistanceFrame, arrivalData.tenDistanceFrame);
                            maxTenDistanceFrame = std::max(maxTenDistanceFrame, arrivalData.tenDistanceFrame);
                        }
                        if (currentFrame < minTenDistanceFrame)
                        {
                            Log::Get() << "WARNING: Ten distance reached before any predicted arrival value; " << *worker;
                        }
                        if (currentFrame > minTenDistanceFrame)
                        {
                            Log::Get() << "WARNING: Ten distance reached after any predicted arrival value; " << *worker;
                        }
                    }
#endif
                }
            }
        }

        // Check if we have reached the end of the path
        if (expectedPath->pathToNextBranch.empty() && expectedPath->nextBranches.empty())
        {
            if (hasFlag(StatusFlags::IssuedTakeoverResend)) return true;

            int transitionFrames;
            if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
            {
                transitionFrames = 1;
            }
            else
            {
                transitionFrames = 0;
            }

            // Check if we can rule out the next frame as action frame
            auto it = expectedPath->actionFramesWithProbabilities.find(currentFrame + transitionFrames);
            if (it != expectedPath->actionFramesWithProbabilities.end())
            {
                // Remove the action frame and adjust the probabilities of any remaining action frames
                double remainingProbability = 1.0 - it->second;
                expectedPath->actionFramesWithProbabilities.erase(it);
                for (auto &[_, probability] : expectedPath->actionFramesWithProbabilities)
                {
                    probability /= remainingProbability;
                }

                // If there are no remaining action frames, our pathing was wrong
                // Assume the transition frame will be the next time our order process timer hits 0, as we're probably close to the target
                if (expectedPath->actionFramesWithProbabilities.empty())
                {
                    auto orderProcessTimerValues =
                        OrderProcessTimer::atStartOfNextFrame(currentFrame, worker->possibleOrderProcessTimerValues);

                    double probabilityPerStep = 1.0 / (double)orderProcessTimerValues.size();
                    double accumulatedProbability = 0.0;
                    for (auto value : orderProcessTimerValues)
                    {
                        if (OrderProcessTimer::framesToNextReset(currentFrame + 1) > (value + transitionFrames))
                        {
                            expectedPath->actionFramesWithProbabilities[currentFrame + value + transitionFrames + 1] = probabilityPerStep;
                            accumulatedProbability += probabilityPerStep;
                            continue;
                        }

                        probabilityPerStep = (1.0 - accumulatedProbability) / 8.0;
                        for (auto f = OrderProcessTimer::nextResetFrame(currentFrame + 1);
                             f < OrderProcessTimer::nextResetFrame(currentFrame + 1) + 8;
                             ++f)
                        {
                            expectedPath->actionFramesWithProbabilities[f + transitionFrames] = probabilityPerStep;
                        }
                        break;
                    }
                }
            }

            return true;
        }

        auto lostPath = [&]()
        {
            setFlag(StatusFlags::LostPath);

            // If all branches from the last node were otherwise stable, assume that the worker will follow the expected path
            // The reason for doing this is to smooth over some path instabilities: we might be on a slightly different subpixel position than we have
            // trained on, but most likely will still get the same result even if an intermediate position differs
            if (expectedPath->arrivalDataWithProbabilities.size() == 1)
            {
                auto resendFrames = expectedPath->aggregatedResendFramesIfStable();
                if (resendFrames)
                {
                    // Patch the resend frames into the expected path and clear the remaining path information
                    expectedPath->resendFramesOnThisBranch = std::move(*resendFrames);
                    expectedPath->pathToNextBranch.clear();
                    expectedPath->nextBranches.clear();

                    setFlag(StatusFlags::LostPathWithAssumedResult);

#if VERBOSE_PATH_LOGGING
                    CherryVis::log(worker->id) << "Lost path, but all remaining branches are equivalent, so assuming worker will achieve same result";
#endif
                    return true;
                }
            }

            resetPath();
#if VERBOSE_PATH_LOGGING
            CherryVis::log(worker->id) << "Lost path";
#endif

            return false;
        };

        auto currentPosition = getCurrentPosition(worker);

        // Ensure we are following the path
        if (!expectedPath->pathToNextBranch.empty())
        {
            // We are at a non-branching node, so just validate the next node
            if (expectedPath->pathToNextBranch.front() != currentPosition)
            {
                return lostPath();
            }

            expectedPath->pathToNextBranch.pop_front();
        }
        else
        {
            // There is a branch here, so find the branch being followed
            bool foundNextBranch = false;
            for (auto &candidate : expectedPath->nextBranches)
            {
                // Guard against the path being empty, which should never happen
                if (candidate.pathToNextBranch.empty())
                {
#if LOGGING_ENABLED
                    Log::Get() << "ERROR: Empty path in branch";
#endif
                    resetPath();
                    return false;
                }

                if (candidate.pathToNextBranch.front() == currentPosition)
                {
                    foundNextBranch = true;
                    candidate.pathToNextBranch.pop_front();
                    expectedPath = std::make_unique<SolverResult<ObservationType>>(std::move(candidate));

#if VERBOSE_PATH_LOGGING
                    CherryVis::log(worker->id) << "Captured next branch; predicted frames\n" << expectedPath->framePredictions();
#endif

                    break;
                }
            }

            // If we didn't find the next branch, we lost the path here
            if (!foundNextBranch)
            {
                return lostPath();
            }
        }

        return true;
    }

    template <typename ObservationType>
    void WorkerPathOptimizer<ObservationType>::issueOrders()
    {
        auto resend = [&]()
        {
            auto result = issueResend();
            if (result)
            {
                executedResendFrames.insert(currentFrame);
#if VERBOSE_PATH_LOGGING
                CherryVis::log(worker->id) << "Issued planned resend";
#endif
            }
            else
            {
                // We couldn't issue the resend, so bail out
                resetPath();

#if VERBOSE_PATH_LOGGING
                Log::Get() << "Failed to issue planned resend for " << worker->id << " @ " << worker->getTilePosition() << ": "
                           << BWAPI::Broodwar->getLastError();
                CherryVis::log(worker->id) << "Failed to issue planned resend; last error " << BWAPI::Broodwar->getLastError();
#endif
            }
        };

        if (expectedPath && expectedPath->resendFramesOnThisBranch.contains(currentFrame))
        {
            expectedPath->resendFramesOnThisBranch.erase(currentFrame);
            resend();
            if (takeoverResendFrames.contains(currentFrame))
            {
                takeoverResendFrames.erase(currentFrame);
            }
        }
        else if (takeoverResendFrames.contains(currentFrame))
        {
            takeoverResendFrames.erase(currentFrame);
            resend();
            setFlag(StatusFlags::IssuedTakeoverResend);
        }
    }

    template <typename ObservationType>
    std::optional<int> WorkerPathOptimizer<ObservationType>::earliestActionFrame()
    {
        if (expectedPath)
        {
            return expectedPath->actionFramesWithProbabilities.begin()->first;
        }
        return std::nullopt;
    }

    template <typename ObservationType>
    void WorkerPathOptimizer<ObservationType>::addExpectedActionDataToPatchForecast(PatchOccupiedForecast &forecast)
    {
        if (hasFlag(StatusFlags::IssuedTakeoverResend))
        {
            if (expectedTakeoverFrame != -1) forecast.useTakeoverFrame(expectedTakeoverFrame);
        }
        else if (expectedPath)
        {
            forecast.useActionFrameProbabilities(expectedPath->actionFramesWithProbabilities);
        }
    }

#if OUTPUT_STATISTICS
    template <typename ObservationType>
    void WorkerPathOptimizer<ObservationType>::updateStatistics(PathStatistics &pathStatistics)
    {
        // Only run this on the frame after the worker is stopped being optimized
        if (lastProcessedFrame != (currentFrame - 1)) return;

        // Ensure the path has been completed, as opposed to the worker just having been assigned to something else
        if (!isComplete()) return;

        // Ignore paths that didn't start at the expected position (patch, depot, or initial spawn position)
        if (!hasFlag(StatusFlags::StartedAtPreviousPathEnd) && !hasFlag(StatusFlags::StartedAtInitialSpawnPosition)) return;

        pathStatistics.count++;

        if (hasFlag(StatusFlags::IssuedTakeoverResend) || hasFlag(StatusFlags::SwitchedPatch) || actualPatchLockFrame != -1)
        {
            pathStatistics.withTakeover++;

            if (hasFlag(StatusFlags::SwitchedPatch))
            {
                pathStatistics.patchSwitches++;
            }
            if (actualPatchLockFrame != -1)
            {
                pathStatistics.withPatchLock++;
            }
        }

        if (hasFlag(StatusFlags::CapturedPath))
        {
            pathStatistics.withPath++;

            if (!hasFlag(StatusFlags::LostPath) || hasFlag(StatusFlags::LostPathWithAssumedResult))
            {
                // If we still have a captured path, compare the actual arrival and action frames to the expected
                if (expectedPath && !hasFlag(StatusFlags::IssuedTakeoverResend))
                {
                    int actualArrivalFrame = worker->frameLastMoved;
                    int actualActionFrame = currentFrame;
                    if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) actualActionFrame++;

                    bool validArrivalFrame = expectedPath->containsArrivalFrame(actualArrivalFrame);
                    if (validArrivalFrame) pathStatistics.withExpectedArrivalFrame++;
                    if (expectedPath->actionFramesWithProbabilities.contains(actualActionFrame)) pathStatistics.withExpectedActionFrame++;

                    if (!validArrivalFrame
                        && !expectedPath->actionFramesWithProbabilities.contains(actualActionFrame))
                    {
                        CherryVis::log(worker->id) << "Unexpected arrival and action frame";
                    }
                    else if (!validArrivalFrame)
                    {
                        CherryVis::log(worker->id) << "Unexpected arrival frame but expected action frame";
                    }
                    else if (!expectedPath->actionFramesWithProbabilities.contains(actualActionFrame))
                    {
                        CherryVis::log(worker->id) << "Unexpected action frame but expected arrival frame";
                        Log::Get() << "WARNING: Unexpected action frame with expected arrival frame"
                                   << " (" << ((worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) ? "gather" : "return") << ")"
                                   << "; worker " << worker->id << " @ " << worker->getTilePosition();
                    }
                }

                pathStatistics.withPathFollowedToStableResult++;
                if (!hasFlag(StatusFlags::LostPath))
                {
                    pathStatistics.withPathFollowedToCompletion++;
                }
#if IS_OPENBW
            }
            else if (startPosition)
            {
                pathStatistics.startPositionsThatLostPath.insert(*startPosition);
#if LOG_PATH_FAILURES
                Log::Get() << "Lost path: " << resource->tile << ": " << PositionAndVelocity(*startPosition) << ": " << (*startPosition);
                if constexpr (std::is_same_v<ObservationType, ReturnArrivalData>)
                {
                    if ((startPosition->x & 0b11111111) % 64 == 0 && (startPosition->y & 0b11111111) % 64 == 0)
                    {
                        Log::Get() << "ERROR: Return start position should have been covered but we lost the path";
                    }
                }
#endif
#endif
            }
#if IS_OPENBW
        }
        else if (startPosition)
        {
            pathStatistics.startPositionsMissingPath.insert(*startPosition);
#if LOG_PATH_FAILURES
            Log::Get() << "No path: " << resource->tile << ": " << PositionAndVelocity(*startPosition) << ": " << (*startPosition);
#endif
#endif
        }
    }
#endif

    template class WorkerPathOptimizer<GatherArrivalData>;
    template class WorkerPathOptimizer<ReturnArrivalData>;
}
