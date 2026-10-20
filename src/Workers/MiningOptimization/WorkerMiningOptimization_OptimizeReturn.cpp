/* Worker mining optimization is split into multiple files */
/* This file contains the logic that optimizes the return of minerals */

#include "WorkerMiningOptimization.h"
#include "PathTraversalLoopGuard.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#define EPSILON 0.000001

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
            return (*first.expectedPath.begin())->pt < (*second.expectedPath.begin())->pt;
        }

        PositionEvaluation evaluatePosition(int commandFrame, ReturnPositionObservations &positionMetadata) // NOLINT(*-no-recursion)
        {
            /* If no resend from this position can take effect before reaching the depot, bail out now */
            if (positionMetadata.noResendArrivalObservations.largestArrivalDelay() <= BWAPI::Broodwar->getLatencyFrames())
            {
                return {};
            }

            /* Jump out of the recursion when we've exceeded the exploration horizon */
            if (positionMetadata.afterExplorationHorizon())
            {
                return {};
            }

            /* Start by getting the data for all of the next positions */
            PositionEvaluation nextPositionsEvaluation;
            if (positionMetadata.nextPositions.size() == 1)
            {
                nextPositionsEvaluation = evaluatePosition(commandFrame + 1, *positionMetadata.nextPositions.begin());
            }
            else if (positionMetadata.nextPositions.size() > 1)
            {
                /* Evaluate all of the next positions, finding the most likely position and the weight of the explored position occurrence rates */
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

                /* Now tally up the scores, adjusting them to the resend frame of the most common occurrence */
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

            /* Explore positions within our exploration horizon that haven't been tried yet */
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

            /* We haven't explored this position and aren't interested in exploring it, so return the next position data */
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

        void planResend(WorkerReturnStatus &wrkStatus,
                        const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            /* Require a path node */
            if (!wrkStatus.currentNode) return;

            /* Don't plan anything until we have left the patch */
            if (!wrkStatus.hasLeftPatch) return;

            auto &positionMetadata = *wrkStatus.currentNode;

            /* Wait to start planning until we reach a position that is usable */
            if (!positionMetadata.usableForPathPlanning()) return;

            /* We are now sure that we will plan something, though we may choose not to perform a resend */
            wrkStatus.resendPlanned = true;
            wrkStatus.hasPathData = true;

            /* Check if we need to "explore" the no resend case */
            if (positionMetadata.noResendArrivalObservations.empty()) return;
            if (WorkerMiningOptimization::isExploring() && positionMetadata.noResendArrivalObservations.shouldExploreDeliverySpeeds()) return;

            auto shouldResend = [&](const PositionEvaluation &evaluation)
            {
                if (!evaluation.resendPosition) return false;
                if (evaluation.positionToTry) return true;
                return evaluation.explored;
            };

            auto evaluation = evaluatePosition(frameNow, positionMetadata);
            if (shouldResend(evaluation))
            {
                wrkStatus.plannedResendPosition = evaluation.resendPosition;
                wrkStatus.expectedPath = std::move(evaluation.expectedPath);
                wrkStatus.plannedResendIsForExploration = evaluation.positionToTry;
                wrkStatus.expectedDelayAfterResend = evaluation.expectedDelay;

#if OPTIMALRETURN_DEBUG
                std::ostringstream out;
                out << std::fixed << std::setprecision(1) << "Planned return command: ";
                if (wrkStatus.plannedResendPosition)
                {
                    out << *wrkStatus.plannedResendPosition;
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
                    if (wrkStatus.plannedResendPosition)
                    {
                        out << "\n" << wrkStatus.plannedResendPosition->resendArrivalObservations;
                    }
                }

                Telemetry::log(wrkStatus.wrk->ident) << out.str();
#endif
            }
        }

        void validatePlannedReturnPath(WorkerReturnStatus &wrkStatus, const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (wrkStatus.expectedPath.empty()) return; // have no further resends planned
            if (wrkStatus.expectedPath.front()->pt == *currentPosition) return; // path matches expectations

            /* We have reached an unexpected position, so reset so we can potentially replan */
            wrkStatus.resendPlanned = false;
            wrkStatus.expectedPath.clear();
            wrkStatus.plannedResendPosition = nullptr;
            wrkStatus.plannedResendIsForExploration = false;
            wrkStatus.expectedDelayAfterResend = 100.0;
        }

        bool shouldPerformScheduledResendHere(WorkerReturnStatus &wrkStatus,
                                              const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (wrkStatus.resentPosition) return false;
            if (!wrkStatus.plannedResendPosition) return false;
            if (wrkStatus.plannedResendPosition->pt != *currentPosition) return false;

            if (wrkStatus.plannedResendIsForExploration) return true;

            /* Check if not resending is more efficient */
            auto &wrk = wrkStatus.wrk;
            auto noResendExpectedDelay = wrkStatus.plannedResendPosition->noResendArrivalObservations.expectedNoResendDeliveryDelay(wrk);
            if (wrkStatus.expectedDelayAfterResend < (noResendExpectedDelay - EPSILON))
            {
                return true;
            }

#if OPTIMALRETURN_DEBUG
            Telemetry::log(wrk->ident) << "Not resending for " << *wrkStatus.plannedResendPosition
                                       << std::fixed << std::setprecision(1)
                                       << ": no resend delay " << noResendExpectedDelay
                                       << " vs. resend delay " << wrkStatus.expectedDelayAfterResend;
#endif
            wrkStatus.plannedResendPosition = nullptr;

            return false;
        }
    }

    void optimizeReturnOfResource(const OwnedWorker &wrk, const OwnedUnit &depot, const Resource &resource)
    {
        if (!resource) return;

        auto &wrkStatus = returnStatusFor(wrk, depot, resource);

        /* Track the worker's visited positions */
        auto currentPosition = wrkStatus.appendCurrentPosition();

#if !ENABLE_RETURN_OPTIMIZATION
        return;
#endif

        /* Advance the current node if we have one */
        if (wrkStatus.currentNode)
        {
            /* Reset the current node if we have issued a resend */
            if (wrkStatus.resentPosition != nullptr && wrkStatus.currentNode->pt == *wrkStatus.resentPosition)
            {
                wrkStatus.currentNode = nullptr;
            }
            else
            {
                wrkStatus.currentNode = wrkStatus.currentNode->nextPositionIfExists(*currentPosition);
            }
        }

        /* If we don't have a current node, check if this position is a root node */
        if (!wrkStatus.currentNode && !wrkStatus.resendPlanned)
        {
            auto rootNode = findReturnPositionObservations(resource, *currentPosition, false, &wrkStatus.rootNode);
            if (rootNode)
            {
                wrkStatus.currentNode = rootNode;
            }
        }

        if (wrkStatus.resendPlanned)
        {
            validatePlannedReturnPath(wrkStatus, currentPosition);
        }

        if (!wrkStatus.resendPlanned)
        {
            planResend(wrkStatus, currentPosition);
        }

        if (wrkStatus.resendPlanned)
        {
            if (shouldPerformScheduledResendHere(wrkStatus, currentPosition))
            {
#if OPTIMALRETURN_DEBUG
                Telemetry::log(wrk->ident) << "Resending for " << *wrkStatus.plannedResendPosition
                                           << (wrkStatus.plannedResendIsForExploration ? " (exploring)" : "");
#endif
                wrkStatus.sendReturnCommand(currentPosition);
            }

            /* Remove this position from the expected path */
            if (!wrkStatus.expectedPath.empty())
            {
                wrkStatus.expectedPath.pop_front();
            }
        }
    }
}
