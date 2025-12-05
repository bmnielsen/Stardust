#include "WorkerPathOptimizer.h"

#include "Solver/Solver.h"

#include "DebugFlag_MiningOptimization.h"

namespace MiningOptimization
{
    template <typename ObservationType>
    void WorkerPathOptimizer<ObservationType>::optimize()
    {
        if (skipPathOptimization()) return;

        // Reset if the worker hasn't been optimized last frame
        if (lastProcessedFrame != (currentFrame - 1))
        {
            reset();
            setStartOfPathFlags();

#if IS_OPENBW
            startPosition = std::make_unique<BWAPI::ExactPosition>(worker->bwapiUnit->getExactPosition());
#endif
        }
        lastProcessedFrame = currentFrame;

        PositionAndVelocity currentPosition(worker);

        // Extract the path when we reach a root node
        if (!pathBeingFollowed)
        {
            // If we don't find a node, we have nothing to do and can just return
            auto it = pathData.find(currentPosition);
            if (it == pathData.end()) return;

            setFlag(StatusFlags::CapturedPath);
            pathBeingFollowed = std::make_unique<Path<ObservationType>>(it->second.get());

            Solver<ObservationType> solver(positionDeltas,
                                           minimumNextPathLength,
                                           resource,
                                           currentPosition,
                                           *pathBeingFollowed,
                                           currentFrame,
                                           worker->orderProcessTimer);
            expectedPath = std::make_unique<SolverPathBranch>(solver.execute());

#if VERBOSE_PATH_LOGGING
            CherryVis::log(worker->id) << "Captured path and ran solver";
#endif
        }

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
            resetPath();
            return;
        }

        // Ensure we are following the path
        if (!expectedPath->pathToNextBranch.empty())
        {
            // We are at a non-branching node, so just validate the next node
            if (expectedPath->pathToNextBranch.front() != currentPosition)
            {
                resetPath();
                setFlag(StatusFlags::LostPath);
#if VERBOSE_PATH_LOGGING
                CherryVis::log(worker->id) << "Lost path";
#endif
                return;
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
                    return;
                }

                if (candidate.pathToNextBranch.front() == currentPosition)
                {
                    foundNextBranch = true;
                    candidate.pathToNextBranch.pop_front();
                    expectedPath = std::make_unique<SolverPathBranch>(std::move(candidate));
                    break;
                }
            }

            // If we didn't find the next branch, we lost the path here
            if (!foundNextBranch)
            {
                resetPath();
                setFlag(StatusFlags::LostPath);
#if VERBOSE_PATH_LOGGING
                CherryVis::log(worker->id) << "Lost path";
#endif
                return;
            }
        }

        // Send a planned resend for this frame
        if (expectedPath->resendFramesOnThisBranch.contains(currentFrame))
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
        if (hasFlag(StatusFlags::CapturedPath))
        {
            pathStatistics.withPath++;

            if (!hasFlag(StatusFlags::LostPath))
            {
                pathStatistics.withPathFollowedToCompletion++;
#if IS_OPENBW
            }
            else if (startPosition)
            {
                pathStatistics.startPositionsThatLostPath.insert(*startPosition);
#endif
            }
#if IS_OPENBW
        }
        else if (startPosition)
        {
            pathStatistics.startPositionsMissingPath.insert(*startPosition);
#endif
        }
    }
#endif

    template class WorkerPathOptimizer<GatherArrivalData>;
    template class WorkerPathOptimizer<ReturnArrivalData>;
}
