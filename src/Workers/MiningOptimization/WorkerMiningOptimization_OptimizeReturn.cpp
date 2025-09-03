// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the return of minerals

#include "WorkerMiningOptimization.h"
#include "PathTraversalLoopGuard.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
        struct PositionEvaluation
        {
            bool explored = false;
            double expectedDelay = 0.0;
            std::deque<ReturnPositionObservations*> expectedPath; // up to and including the resend position
            ReturnPositionObservations* resendPosition = nullptr;
            bool positionToTry = false;

            [[nodiscard]] double expectedDelayAtStartOfPath() const
            {
                return expectedDelay + (double)((int)expectedPath.size() - 1);
            }

            static PositionEvaluation exploring(double delta, ReturnPositionObservations* resendPos)
            {
                return {false, delta, {resendPos}, resendPos, true};
            }

            static PositionEvaluation resend(double delta, ReturnPositionObservations* resendPos)
            {
                return {true, delta, {resendPos}, resendPos, false};
            }
        };

        bool less(const PositionEvaluation &first, const PositionEvaluation &second)
        {
            if (first.expectedPath.empty() && second.expectedPath.empty()) return first.expectedDelay < second.expectedDelay;
            if (first.expectedPath.empty()) return true;
            if (second.expectedPath.empty()) return false;
            return (*first.expectedPath.begin())->pos < (*second.expectedPath.begin())->pos;
        }

        PositionEvaluation evaluatePosition(int commandFrame, ReturnPositionObservations &positionMetadata) // NOLINT(*-no-recursion)
        {
            // If no resend from this position can take effect before reaching the depot, bail out now
            if (positionMetadata.noResendArrivalObservations.largestArrivalDelay() <= BWAPI::Broodwar->getLatencyFrames())
            {
                return {};
            }

            // Jump out of the recursion when we've exceeded the exploration horizon
            if (positionMetadata.afterExplorationHorizon())
            {
                return {};
            }

            // Start by getting the data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            if (positionMetadata.nextPositions.size() == 1)
            {
                nextPositionsEvaluation = evaluatePosition(commandFrame + 1, *positionMetadata.nextPositions.begin());
            }
            else if (positionMetadata.nextPositions.size() > 1)
            {
                // Evaluate all of the next positions, finding the most likely position and the weight of the explored position occurrence rates
                std::vector<PositionEvaluation> nextPositionEvaluations;
                nextPositionEvaluations.reserve(positionMetadata.nextPositions.size());
                int mostCommon = -1;
                uint8_t totalOccurrenceRate = 0;
                uint8_t bestOccurrenceRate = 0;
                for (int i = 0; i < positionMetadata.nextPositions.size(); i++)
                {
                    auto nextPositionEvaluation = evaluatePosition(commandFrame + 1, positionMetadata.nextPositions[i]);

                    if (nextPositionEvaluation.explored)
                    {
                        totalOccurrenceRate += positionMetadata.nextPositions[i].occurrenceRate;
                    }

                    if (mostCommon == -1 || positionMetadata.nextPositions[i].occurrenceRate > bestOccurrenceRate ||
                        (positionMetadata.nextPositions[i].occurrenceRate == bestOccurrenceRate
                            && less(nextPositionEvaluation, nextPositionEvaluations[mostCommon])))
                    {
                        bestOccurrenceRate = positionMetadata.nextPositions[i].occurrenceRate;
                        mostCommon = i;
                    }

                    nextPositionEvaluations.emplace_back(std::move(nextPositionEvaluation));
                }

                // Now tally up the scores, adjusting them to the resend frame of the most common occurrence
                double delayAccumulator = 0.0;
                for (int i = 0; i < positionMetadata.nextPositions.size(); i++)
                {
                    if (!nextPositionEvaluations[i].explored) continue;

                    auto pathDifference = (double)nextPositionEvaluations[mostCommon].expectedPath.size()
                            - (double)nextPositionEvaluations[i].expectedPath.size();

                    delayAccumulator += (nextPositionEvaluations[i].expectedDelay + pathDifference)
                                    * ((double)positionMetadata.nextPositions[i].occurrenceRate / (double)totalOccurrenceRate);
                }

                nextPositionsEvaluation = std::move(nextPositionEvaluations[mostCommon]);
                nextPositionsEvaluation.expectedDelay = delayAccumulator;
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), &positionMetadata);

            // Explore positions within our exploration horizon that haven't been tried yet
            if (WorkerMiningOptimization::isExploring() && positionMetadata.suitableForExploration())
            {
                double deltaToBenchmark =
                        std::abs(8 + BWAPI::Broodwar->getLatencyFrames() - positionMetadata.noResendArrivalObservations.mostCommonArrivalDelay());
                if (!nextPositionsEvaluation.positionToTry || deltaToBenchmark < nextPositionsEvaluation.expectedDelay)
                {
                    return PositionEvaluation::exploring(deltaToBenchmark, &positionMetadata);
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
                return PositionEvaluation::resend(expectedDelay, &positionMetadata);
            }

            return nextPositionsEvaluation;
        }

        void planResend(WorkerReturnStatus &workerStatus,
                        const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            // Require a path node
            if (!workerStatus.currentNode) return;

            // Don't plan anything until we have left the patch
            if (!workerStatus.hasLeftPatch) return;

            auto &positionMetadata = *workerStatus.currentNode;

            // Wait to start planning until we reach a position that is usable
            if (!positionMetadata.usableForPathPlanning()) return;

            // We are now sure that we will plan something, though we may choose not to perform a resend
            workerStatus.resendPlanned = true;
            workerStatus.hasPathData = true;

            // Check if we need to "explore" the no resend case
            if (positionMetadata.noResendArrivalObservations.empty()) return;
            if (WorkerMiningOptimization::isExploring() && positionMetadata.noResendArrivalObservations.shouldExploreDeliverySpeeds()) return;

            auto shouldResend = [&](const PositionEvaluation &evaluation)
            {
                if (!evaluation.resendPosition) return false;
                if (evaluation.positionToTry) return true;
                return evaluation.explored;
            };

            auto evaluation = evaluatePosition(currentFrame, positionMetadata);
            if (shouldResend(evaluation))
            {
                workerStatus.plannedResendPosition = evaluation.resendPosition;
                workerStatus.expectedPath = std::move(evaluation.expectedPath);
                workerStatus.plannedResendIsForExploration = evaluation.positionToTry;
                workerStatus.expectedDelayAfterResend = evaluation.expectedDelay;

#if OPTIMALRETURN_DEBUG
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
                    if (workerStatus.plannedResendPosition)
                    {
                        out << "\n" << workerStatus.plannedResendPosition->resendArrivalObservations;
                    }
                }

                CherryVis::log(workerStatus.worker->id) << out.str();
#endif
            }
        }

        void validatePlannedReturnPath(WorkerReturnStatus &workerStatus, const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (workerStatus.expectedPath.empty()) return; // have no further resends planned
            if (workerStatus.expectedPath.front()->pos == *currentPosition) return; // path matches expectations

            // We have reached an unexpected position, so reset so we can potentially replan
            workerStatus.resendPlanned = false;
            workerStatus.expectedPath.clear();
            workerStatus.plannedResendPosition = nullptr;
            workerStatus.plannedResendIsForExploration = false;
            workerStatus.expectedDelayAfterResend = 100.0;
        }

        bool shouldPerformScheduledResendHere(WorkerReturnStatus &workerStatus,
                                              const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (workerStatus.resentPosition) return false;
            if (!workerStatus.plannedResendPosition) return false;
            if (workerStatus.plannedResendPosition->pos != *currentPosition) return false;

            if (workerStatus.plannedResendIsForExploration) return true;

            // Check if not resending is more efficient
            auto &worker = workerStatus.worker;
            auto noResendExpectedDelay = workerStatus.plannedResendPosition->noResendArrivalObservations.expectedNoResendDeliveryDelay(worker);
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
        if (!resource) return;

        auto &workerStatus = returnStatusFor(worker, depot, resource);

        // Track the worker's visited positions
        auto currentPosition = workerStatus.appendCurrentPosition();

#if !ENABLE_RETURN_OPTIMIZATION
        return;
#endif

        // Advance the current node if we have one
        if (workerStatus.currentNode)
        {
            // Reset the current node if we have issued a resend
            if (workerStatus.resentPosition != nullptr && workerStatus.currentNode->pos == *workerStatus.resentPosition)
            {
                workerStatus.currentNode = nullptr;
            }
            else
            {
                workerStatus.currentNode = workerStatus.currentNode->nextPositionIfExists(*currentPosition);
            }
        }

        // If we don't have a current node, check if this position is a root node
        if (!workerStatus.currentNode && !workerStatus.resendPlanned)
        {
            auto &rootNodes = returnPositionRootNodesFor(resource);
            auto rootNodeIt = rootNodes.find(*currentPosition);
            if (rootNodeIt != rootNodes.end())
            {
                workerStatus.currentNode = &rootNodeIt->second;
            }
        }

        if (workerStatus.resendPlanned)
        {
            validatePlannedReturnPath(workerStatus, currentPosition);
        }

        if (!workerStatus.resendPlanned)
        {
            planResend(workerStatus, currentPosition);
        }

        if (workerStatus.resendPlanned)
        {
            if (shouldPerformScheduledResendHere(workerStatus, currentPosition))
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
