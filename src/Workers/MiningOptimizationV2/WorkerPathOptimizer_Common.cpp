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
        std::vector<std::pair<PathNode<ObservationType>, uint8_t>>* currentNodeNextPositions = nullptr;

        // Extract the path when we reach a root node
        if (!pathBeingFollowed)
        {
            auto it = pathData.find(currentPosition);
            if (it != pathData.end())
            {
                setFlag(StatusFlags::CapturedPath);
                pathBeingFollowed = std::make_unique<Path<ObservationType>>(it->second.get());
                currentNodeNextPositions = &pathBeingFollowed->nextPositions;
#if VERBOSE_PATH_LOGGING
                CherryVis::log(worker->id) << "Captured path";
#endif
            }
        }

        // If we are following a path, validate that we have reached the expected position
        if (pathPlanned && !expectedPath.empty())
        {
            if (!previousPosition)
            {
                Log::Get() << "ERROR: Previous position not available while following path"
                           << "; worker " << worker->id << " @ " << worker->getTilePosition();
                reset();
                return;
            }

            auto &expectedNode = *expectedPath.front();
            if (expectedNode.pos.matches(*previousPosition, currentPosition, positionDeltas))
            {
                // The path is being followed, so reference the appropriate next positions and update the expected path
                currentNodeNextPositions =
                        (executedResendFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames()))
                        ? &expectedNode.nextPositionsAfterResend
                        : &expectedNode.nextPositions;
                expectedPath.pop_front();
#if VERBOSE_PATH_LOGGING
                CherryVis::log(worker->id) << "Following path";
#endif
            }
            else
            {
                // We lost our expected path, but check if we can pick up a different branch from the previous frame
                if (previousNodeNextPositions)
                {
                    for (auto &[node, _] : *previousNodeNextPositions)
                    {
                        if (&node == &expectedNode) continue;
                        if (!node.pos.matches(*previousPosition, currentPosition, positionDeltas)) continue;

                        // We found our current node, so update the next positions to reflect that
                        currentNodeNextPositions =
                                (executedResendFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames()))
                                ? &node.nextPositionsAfterResend
                                : &node.nextPositions;
                        break;
                    }
                }

                if (!currentNodeNextPositions) setFlag(StatusFlags::LostPath);

#if VERBOSE_PATH_LOGGING
                if (currentNodeNextPositions)
                {
                    CherryVis::log(worker->id) << "Lost path; picked up alternative branch";
                }
                else
                {
                    CherryVis::log(worker->id) << "Lost path; no alternative branch available";
                }
#endif

                // Clear the state so we can replan
                pathPlanned = false;
                expectedPath.clear();
            }
        }

        if (!pathPlanned && currentNodeNextPositions)
        {
            expectedPath.clear();

            // TODO: Implement takeover logic

            Solver<ObservationType> solver(positionDeltas,
                                           minimumNextPathLength,
                                           resource,
                                           currentPosition,
                                           *currentNodeNextPositions,
                                           currentFrame,
                                           worker->orderProcessTimer);
            solver.execute();

            // TODO: Implement actual path planner
            // Temporary logic just assumes the most common no-resend path
            auto nextPositions = currentNodeNextPositions;
            while (nextPositions && !nextPositions->empty())
            {
                auto &nextNode = (*nextPositions)[0].first;
                expectedPath.push_back(&nextNode);
                nextPositions = &nextNode.nextPositions;
            }

#if VERBOSE_PATH_LOGGING
            std::ostringstream dbg;
            std::string sep;
            auto pos = currentPosition;
            for (auto &node : expectedPath)
            {
                auto delta = positionDeltas[node->pos.positionDeltaIndex()];
                pos.x += delta.first;
                pos.y += delta.second;
                dbg << sep << pos;
                sep = ", ";
            }
            CherryVis::log(worker->id) << "Expected path: " << dbg.str();
#endif

            pathPlanned = true;
        }

        // Send a planned resend for this frame
        if (plannedResendFrames.contains(currentFrame))
        {
            plannedResendFrames.erase(currentFrame);

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
                // We couldn't issue the resend, so replan next frame
                pathPlanned = false;

#if VERBOSE_PATH_LOGGING
                Log::Get() << "Failed to issue planned resend for " << worker->id << " @ " << worker->getTilePosition() << ": "
                           << BWAPI::Broodwar->getLastError();
                CherryVis::log(worker->id) << "Failed to issue planned resend; last error " << BWAPI::Broodwar->getLastError();
#endif
            }
        }

        // Update state for the next frame
        previousPosition = std::make_unique<PositionAndVelocity>(std::move(currentPosition));
        previousNodeNextPositions = currentNodeNextPositions;
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
