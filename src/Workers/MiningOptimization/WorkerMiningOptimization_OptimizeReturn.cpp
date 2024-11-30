// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the return of minerals

#include "WorkerMiningOptimization.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
        struct PositionEvaluation
        {
            bool explored = false;
            double expectedDelay = 0.0;
            std::deque<PositionAndVelocity> expectedPath; // up to and including the resend position
            std::shared_ptr<PositionAndVelocity> resendPosition;
            bool positionToTry = false;

            [[nodiscard]] double expectedDelayAtStartOfPath() const
            {
                return expectedDelay + (double)((int)expectedPath.size() - 1);
            }
        };

        PositionEvaluation evaluatePosition(int commandFrame,
                                            const std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &allPositionData,
                                            const ReturnPositionObservations &positionMetadata);

        PositionEvaluation evaluateNextPosition(int commandFrame,
                                                const std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &allPositionData,
                                                const PositionAndVelocity &nextPosition)
        {
            auto nextPositionDataIt = allPositionData.find(nextPosition);
            if (nextPositionDataIt == allPositionData.end())
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: No return metadata found for next position " << nextPosition;
#endif
                return {};
            }

            return evaluatePosition(commandFrame + 1, allPositionData, nextPositionDataIt->second);
        }

        PositionEvaluation evaluatePosition(int commandFrame,
                                            const std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &allPositionData,
                                            const ReturnPositionObservations &positionMetadata)
        {
            auto here = std::make_shared<PositionAndVelocity>(positionMetadata.pos);

            // If no resend from this position can take effect before reaching the depot, bail out now
            if (positionMetadata.noResendArrivalObservations.largestArrivalDelay() <= BWAPI::Broodwar->getLatencyFrames())
            {
                return {};
            }

            // Jump out of the recursion when we've exceeded the exploration horizon
            if (!positionMetadata.afterExplorationHorizon())
            {
                return {};
            }

            // Start by getting the data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            if (positionMetadata.nextPositionAndOccurrences.size() == 1)
            {
                nextPositionsEvaluation = evaluateNextPosition(
                        commandFrame,
                        allPositionData,
                        positionMetadata.nextPositionAndOccurrences.begin()->first);
            }
            else if (positionMetadata.nextPositionAndOccurrences.size() > 1)
            {
                double delayAccumulator = 0.0;
                uint32_t occurrenceCount = 0;
                uint32_t bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : positionMetadata.nextPositionAndOccurrences)
                {
                    auto nextPositionEvaluation = evaluateNextPosition(commandFrame, allPositionData, nextPosition);
                    if (nextPositionsEvaluation.explored)
                    {
                        delayAccumulator += nextPositionEvaluation.expectedDelay * occurrences;
                        occurrenceCount += occurrences;
                    }
                    if (occurrences > bestOccurrences)
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelay = (delayAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), positionMetadata.pos);

            // Explore positions within our exploration horizon that haven't been tried yet
            if (WorkerMiningOptimization::isExploring() && positionMetadata.suitableForExploration())
            {
                double deltaToBenchmark =
                        std::abs(8 + BWAPI::Broodwar->getLatencyFrames() - positionMetadata.noResendArrivalObservations.mostCommonArrivalDelay());
                if (!nextPositionsEvaluation.positionToTry || deltaToBenchmark < nextPositionsEvaluation.expectedDelay)
                {
                    return {false, deltaToBenchmark, {*here}, here, true};
                }
            }
            if (nextPositionsEvaluation.positionToTry) return nextPositionsEvaluation;

            // We haven't explored this position and aren't interested in exploring it, so return the next position data
            if (positionMetadata.resendArrivalObservations.empty())
            {
                return nextPositionsEvaluation;
            }

            double expectedDelay = positionMetadata.resendArrivalObservations.expectedDeliveryDelay(commandFrame);
            if (!nextPositionsEvaluation.explored || expectedDelay < (nextPositionsEvaluation.expectedDelayAtStartOfPath() - EPSILON))
            {
                return {true, expectedDelay, {*here}, here, false};
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
                return evaluation.explored;
            };

            auto evaluation = evaluatePosition(BWAPI::Broodwar->getFrameCount(), optimalPositions, positionMetadata);
            if (shouldResend(evaluation))
            {
                workerStatus.plannedResendPosition = evaluation.resendPosition;
                workerStatus.expectedPath = std::move(evaluation.expectedPath);
                workerStatus.plannedResendIsForExploration = evaluation.positionToTry;
                workerStatus.expectedDelayAfterResend = evaluation.expectedDelay;

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
            workerStatus.plannedResendIsForExploration = false;
            workerStatus.expectedDelayAfterResend = 100.0;
        }

        bool shouldPerformScheduledResendHere(WorkerReturnStatus &workerStatus,
                                              const std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &optimalPositions,
                                              const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (workerStatus.resentPosition) return false;
            if (!workerStatus.plannedResendPosition) return false;
            if (*workerStatus.plannedResendPosition != *currentPosition) return false;

            if (workerStatus.plannedResendIsForExploration) return true;

            // Check if not resending is more efficient
            auto &worker = workerStatus.worker;

            // First look up the metadata for this position
            auto positionMetadataIt = optimalPositions.find(*currentPosition);
            if (positionMetadataIt == optimalPositions.end()) // should never happen, since we've planned a resend here
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Metadata not found at planned resend position " << *currentPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                // Return false since we don't want observations for resends at positions we haven't observed without a resend yet
                return false;
            }

            // Now estimate the delay given that we do not resend
            auto noResendExpectedDelay = positionMetadataIt->second.noResendArrivalObservations.expectedNoResendDeliveryDelay(worker);
            if (workerStatus.expectedDelayAfterResend < (noResendExpectedDelay - EPSILON))
            {
                return true;
            }

#if OPTIMALRETURN_DEBUG
            CherryVis::log(worker->id) << "Not resending for " << *workerStatus.plannedResendPosition
                                       << std::fixed << std::setprecision(1)
                                       << ": no resend delay " << noResendExpectedDelay
                                       << " vs. resend delay " << workerStatus.expectedDelayAfterResend;
#endif
            workerStatus.plannedResendPosition = nullptr;

            return false;
        }
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto &workerStatus = returnStatusFor(worker, depot, resource);

        // Track the worker's visited positions
        auto currentPosition = workerStatus.appendCurrentPosition();

#if !ENABLE_RETURN_OPTIMIZATION
        return;
#endif

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
            if (shouldPerformScheduledResendHere(workerStatus, optimalPositions, currentPosition))
            {
#if OPTIMALRETURN_DEBUG
                CherryVis::log(worker->id) << "Resending for " << *workerStatus.plannedResendPosition
                                           << (workerStatus.plannedResendIsForExploration ? " (exploring)" : "");
#endif
                workerStatus.sendReturnCommand(currentPosition);
            }

            // Remove this position from the expected path
            if (!workerStatus.expectedPath.empty())
            {
                workerStatus.expectedPath.pop_front();
            }
        }
    }
}
