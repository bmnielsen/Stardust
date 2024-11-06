// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the return of minerals

#include "WorkerMiningOptimization.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
        double expectedCollisionAndStoppageDelay(const ReturnArrivalObservations &observations)
        {
            return 0.0;

            // TODO

//        int total = observations.collision + observations.stopped + observations.keptSpeed;
//        if (total == 0) return 0.0;
//
//        // If we are exploring and don't have enough data yet, set it to 0
//        if (WorkerMiningOptimization::isExploring() && total < 5) return 0.0;
//
//        // Collisions add 14+5=19 frames of delay and stoppages add 5
//        return (double)(19 * observations.collision + 5 * observations.stopped) / (double)total;
        }

        struct PositionEvaluation
        {
            double expectedDelay = 100.0;
            std::deque<PositionAndVelocity> expectedPath; // up to and including the resend position
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
            auto here = std::make_shared<PositionAndVelocity>(positionMetadata.pos);

            // If no resend from this position can take effect before reaching the depot, bail out now
            if (positionMetadata.noResendArrivalObservations.largestArrivalDelay() <= BWAPI::Broodwar->getLatencyFrames())
            {
                return {100.0, {*here}, here, false};
            }

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
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), positionMetadata.pos);

            if (WorkerMiningOptimization::isExploring() && positionMetadata.resendArrivalObservations.empty())
            {
                return {100.0, {*here}, here, true};
            }
            if (nextPositionsEvaluation.positionToTry) return nextPositionsEvaluation;

            double expectedDelay = computeExpectedDelay(commandFrame, positionMetadata.resendArrivalObservations);
            if (expectedDelay < (nextPositionsEvaluation.expectedDelay - EPSILON))
            {
                return {expectedDelay, {*here}, here, false};
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
                workerStatus.expectedPath = std::move(evaluation.expectedPath);

#if OPTIMALPOSITIONS_DEBUG
                std::ostringstream out;
                out << std::fixed << std::setprecision(1) << "Planned return command: ";
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

        void validatePlannedReturnPath(WorkerReturnStatus &workerStatus, const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (workerStatus.expectedPath.empty()) return; // have no further resends planned
            if (workerStatus.expectedPath.front() == *currentPosition) return; // path matches expectations

            // We have reached an unexpected position, so reset so we can potentially replan
            workerStatus.resendPlanned = false;
            workerStatus.expectedPath.clear();
            workerStatus.plannedResendPosition = nullptr;
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
            validatePlannedReturnPath(workerStatus, currentPosition);
        }

        if (!workerStatus.resendPlanned)
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
