// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the return of minerals

#include "WorkerMiningOptimization.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    double expectedCollisionAndStoppageDelay(const ReturnArrivalObservations &observations)
    {
        int total = observations.collision + observations.stopped + observations.keptSpeed;
        if (total == 0) return 0.0;

        // If we are exploring and don't have enough data yet, set it to 0
        if (WorkerMiningOptimization::isExploring() && total < 5) return 0.0;

        // Collisions add 14+5=19 frames of delay and stoppages add 5
        return (double)(19 * observations.collision + 5 * observations.stopped) / (double)total;
    }

    struct PositionEvaluation
    {
        double expectedDelay = 100.0;
        std::shared_ptr<PositionAndVelocity> resendPosition;
        bool positionToTry = false;
    };

    double computeExpectedDelay(int commandFrame, const ReturnArrivalObservations &observations)
    {
        if (observations.empty()) return 100.0;

        return observations.expectedDeliveryDelay(commandFrame) + expectedCollisionAndStoppageDelay(observations);
    }

    PositionEvaluation evaluatePosition(int commandFrame,
                                        const std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &allPositionData,
                                        const ReturnPositionObservations &positionMetadata)
    {
        // Start by getting the data for all of the next positions
        PositionEvaluation nextPositionsEvaluation;
        {
            double delayAccumulator = 0.0;
            int occurrenceCount = 0;
            int bestOccurrences = 0;
            for (const auto &[nextPosition, occurrences] : positionMetadata.nextPositionAndOccurrences)
            {
                auto nextPositionDataIt = allPositionData.find(nextPosition);
                if (nextPositionDataIt == allPositionData.end())
                {
#if OPTIMALRETURN_DEBUG
                    Log::Get() << "ERROR: No return metadata found for next position " << nextPosition;
#endif
                    continue;
                }

                auto nextPositionEvaluation = evaluatePosition(commandFrame + 1, allPositionData, nextPositionDataIt->second);
                delayAccumulator += nextPositionEvaluation.expectedDelay * occurrences;
                occurrenceCount += occurrences;
                if (occurrences > bestOccurrences)
                {
                    bestOccurrences = occurrences;
                    nextPositionsEvaluation = std::move(nextPositionEvaluation);
                }
            }
            if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelay = (delayAccumulator / (double)occurrenceCount);
        }

        // We can't send a command LF+1 frames before an order process timer reset
        // TODO: Check if this applies!
//        if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

        if (nextPositionsEvaluation.positionToTry) return nextPositionsEvaluation;
        if (WorkerMiningOptimization::isExploring() && positionMetadata.resendArrivalObservations.empty())
        {
            return PositionEvaluation{100.0, std::make_shared<PositionAndVelocity>(positionMetadata.pos), true};
        }

        double expectedDelay = computeExpectedDelay(commandFrame, positionMetadata.resendArrivalObservations);
        if (expectedDelay < (nextPositionsEvaluation.expectedDelay - EPSILON))
        {
            return {expectedDelay, std::make_shared<PositionAndVelocity>(positionMetadata.pos), false};
        }

        return nextPositionsEvaluation;
    }

    void planResend(WorkerReturnStatus &workerStatus,
                    const std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &optimalPositions,
                    const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        // Reference this position's metadata
        auto metadataIt = optimalPositions.find(*currentPosition);
        if (metadataIt == optimalPositions.end()) return; // haven't reached an observed position yet
        auto &positionMetadata = metadataIt->second;

        // Always wait until we have observed a path with no resends before exploring
        if (positionMetadata.noResendArrivalObservations.empty()) return;

        // We are now sure that we will plan something, though we may choose not to perform a resend
        workerStatus.resendPlanned = true;

        auto shouldResend = [&](const PositionEvaluation &evaluation)
        {
            if (!evaluation.resendPosition) return false;
            if (evaluation.positionToTry) return true;

            // TODO: Compare with not resending

            return true;
        };

        auto evaluation = evaluatePosition(BWAPI::Broodwar->getFrameCount(), optimalPositions, positionMetadata);
        if (shouldResend(evaluation))
        {
            workerStatus.plannedResendPosition = evaluation.resendPosition;

#if OPTIMALPOSITIONS_DEBUG
            std::ostringstream out;
            out << std::fixed << std::setprecision(1) << "Planned gather command(s): ";
            if (workerStatus.plannedResendPosition)
            {
                out << *workerStatus.plannedResendPosition;
            }
            else
            {
                out << "none";
            }
            if (evaluation.positionToTry)
            {
                out << " (exploring)";
            }
            else
            {
                out << " expected delay " << evaluation.expectedDelay;
            }

            CherryVis::log(workerStatus.worker->id) << out.str();
#endif
        }
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto &workerStatus = returnStatusFor(worker, depot, resource);

        // Track the worker's visited positions
        auto currentPosition = workerStatus.appendCurrentPosition();

        auto &optimalPositions = optimalReturnPositionsFor(resource);

        if (workerStatus.resendPlanned)
        {
            // TODO: Validate and replan if needed
        }
        else
        {
            planResend(workerStatus, optimalPositions, currentPosition);
        }

        if (workerStatus.resendPlanned)
        {
            if (!workerStatus.resentPosition && workerStatus.plannedResendPosition && (*workerStatus.plannedResendPosition == *currentPosition))
            {
#if OPTIMALRETURN_DEBUG
                CherryVis::log(worker->id) << "Resending for " << *workerStatus.plannedResendPosition;
#endif
                workerStatus.sendReturnCommand(currentPosition);
            }

            // Remove this position from the expected path
            if (!workerStatus.expectedPath.empty()) workerStatus.expectedPath.pop_front();
        }
    }
}
