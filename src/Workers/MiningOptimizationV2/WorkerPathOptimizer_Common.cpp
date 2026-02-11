#include "WorkerPathOptimizer.h"

#include "Solver/Solver.h"

#include "DebugFlag_MiningOptimization.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define LOG_PATH_FAILURES true
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
    void WorkerPathOptimizer<ObservationType>::optimize()
    {
        if (skipPathOptimization()) return;

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

            auto it = pathData.find(currentPosition);
            if (it != pathData.end())
            {
                setFlag(StatusFlags::CapturedPath);
                pathBeingFollowed = std::make_unique<Path<ObservationType>>(it->second.get());

                Solver<ObservationType> solver(mapData,
                                               resource,
                                               currentPosition,
                                               *pathBeingFollowed,
                                               currentFrame,
                                               worker->possibleOrderProcessTimerValues);
                expectedPath = std::make_unique<SolverResult<ObservationType>>(solver.execute());

#if VERBOSE_PATH_LOGGING
                CherryVis::log(worker->id) << "Captured path and ran solver; predicted frames:\n" << expectedPath->framePredictions();
#if IS_OPENBW
                CherryVis::log(worker->id) << worker->bwapiUnit->getExactPosition();
#endif
#endif
            }

#if IS_OPENBW
            startPosition = std::make_unique<BWAPI::ExactPosition>(worker->bwapiUnit->getExactPosition());
#endif
        }
        lastProcessedFrame = currentFrame;

        // Initialize the takeover frames as needed
        initializeGatherTakeover();

        updatePath();

        // Send a planned resend for this frame
        if (expectedPath && expectedPath->resendFramesOnThisBranch.contains(currentFrame))
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
                expectedPath.reset();
                pathBeingFollowed.reset();

#if VERBOSE_PATH_LOGGING
                Log::Get() << "Failed to issue planned resend for " << worker->id << " @ " << worker->getTilePosition() << ": "
                           << BWAPI::Broodwar->getLastError();
                CherryVis::log(worker->id) << "Failed to issue planned resend; last error " << BWAPI::Broodwar->getLastError();
#endif
            }
        }
    }

    template <typename ObservationType>
    void WorkerPathOptimizer<ObservationType>::updatePath()
    {
        // Nothing to do if we didn't have any path data
        if (!pathBeingFollowed) return;

        auto currentPosition = getCurrentPosition(worker);

        // Guard against null expectedPath
        if (!expectedPath)
        {
#if LOGGING_ENABLED
            Log::Get() << "ERROR: expectedPath is empty while pathBeingFollowed is not";
#endif
            resetPath();
            return;
        }

        // TODO: Recalculate patch locking and re-run the solver if the conditions may have changed
        //       Maybe just store a pointer to the nodes in the solve result

        // Check if we have reached the end of the path
        if (expectedPath->pathToNextBranch.empty() && expectedPath->nextBranches.empty())
        {
            return;
        }

        auto lostPath = [&]()
        {
            setFlag(StatusFlags::LostPath);

            // If all branches from the last node were otherwise stable, assume that the worker will follow the expected path
            // The reason for doing this is to smooth over some path instabilities: we might be on a slightly different subpixel position than we have
            // trained on, but most likely will still get the same result even if an intermediate position differs
            if (expectedPath->arrivalFramesWithProbabilities.size() == 1)
            {
                auto resendFrames = expectedPath->aggregatedResendFramesIfStable();
                if (resendFrames)
                {
                    // Patch the resend frames into the expected path and clear the remaining path information
                    expectedPath->resendFramesOnThisBranch = std::move(*resendFrames);
                    expectedPath->pathToNextBranch.clear();
                    expectedPath->pathNodesToNextBranch.clear();
                    expectedPath->nextBranches.clear();

                    setFlag(StatusFlags::LostPathWithAssumedResult);

#if VERBOSE_PATH_LOGGING
                    CherryVis::log(worker->id) << "Lost path, but all remaining branches are equivalent, so assuming worker will achieve same result";
#endif
                    return;
                }
            }

            resetPath();
#if VERBOSE_PATH_LOGGING
            CherryVis::log(worker->id) << "Lost path";
#endif
        };

        // Ensure we are following the path
        if (!expectedPath->pathToNextBranch.empty())
        {
            // We are at a non-branching node, so just validate the next node
            if (expectedPath->pathToNextBranch.front() != currentPosition)
            {
                lostPath();
                return;
            }

            expectedPath->pathToNextBranch.pop_front();
            expectedPath->pathNodesToNextBranch.pop_front();
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
                    return;
                }

                if (candidate.pathToNextBranch.front() == currentPosition)
                {
                    foundNextBranch = true;
                    candidate.pathToNextBranch.pop_front();
                    candidate.pathNodesToNextBranch.pop_front();
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
                lostPath();
                return;
            }
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

        if (hasFlag(StatusFlags::GatherTakeover))
        {
            pathStatistics.withTakeover++;

            if (hasFlag(StatusFlags::SwitchedPatch))
            {
                pathStatistics.patchSwitches++;
            }
            else if (!hasFlag(StatusFlags::LostPath) && expectedPath
                    && SolverResult<ObservationType>::probabilitySum(expectedPath->patchLockFramesWithProbabilities) > PATCH_LOCK_THRESHOLD)
            {
                pathStatistics.withPlannedPatchLock++;

                if (actualPatchLockFrame != -1 && expectedPath->patchLockFramesWithProbabilities.contains(actualPatchLockFrame))
                {
                    pathStatistics.withExpectedPatchLockFrame++;
                }
            }
        }

        if (hasFlag(StatusFlags::CapturedPath))
        {
            pathStatistics.withPath++;

            if (!hasFlag(StatusFlags::LostPath) || hasFlag(StatusFlags::LostPathWithAssumedResult))
            {
                // If we still have a captured path, compare the actual arrival and action frames to the expected
                if (expectedPath)
                {
                    int actualArrivalFrame = worker->frameLastMoved;
                    int actualActionFrame = currentFrame;
                    if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) actualActionFrame++;

                    if (expectedPath->arrivalFramesWithProbabilities.contains(actualArrivalFrame)) pathStatistics.withExpectedArrivalFrame++;
                    if (expectedPath->actionFramesWithProbabilities.contains(actualActionFrame)) pathStatistics.withExpectedActionFrame++;

                    if (!expectedPath->arrivalFramesWithProbabilities.contains(actualArrivalFrame)
                        && !expectedPath->actionFramesWithProbabilities.contains(actualActionFrame))
                    {
                        CherryVis::log(worker->id) << "Unexpected arrival and action frame";
                    }
                    else if (!expectedPath->arrivalFramesWithProbabilities.contains(actualArrivalFrame))
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
                    if ((startPosition->x & 0b11111111) % 8 == 0 && (startPosition->y & 0b11111111) % 8 == 0)
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
