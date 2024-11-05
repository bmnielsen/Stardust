// Worker mining optimization is split into multiple files
// This file contains the logic to find the optimal path from a position for a single worker mining a patch

#include "WorkerMiningOptimization.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
        double expectedPatchCollisionDelay(int observedCollisions, int observedNonCollisions)
        {
            int total = observedCollisions + observedNonCollisions;
            if (total == 0) return 0.0;

            // If we are exploring and don't have enough data yet, allow it no matter what
            if (WorkerMiningOptimization::isExploring() && total < 5) return 0.0;

            // A collision adds 14 frames of delay
            return 14.0 * (double)observedCollisions / (double)total;
        }

        struct PositionEvaluation
        {
            double expectedDelta = 100.0;
            std::deque<PositionAndVelocity> expectedPath;
            std::shared_ptr<PositionAndVelocity> resendPosition;
            bool positionToTryOnExpectedPath = false;
            int positionToTryDelta = 0;
        };

        double computeExpectedDelta(int commandFrame,
                                    const PositionObservationMetadata &positionMetadata,
                                    int deltaToFirstResend,
                                    const ResendPositionObservations &observations)
        {
            if (positionMetadata.deltaToNormalPathOptimalPosition.empty()) return 100.0;
            if ((positionMetadata.deltaToNormalPathOptimalPosition.size() > 1 || positionMetadata.next.size() > 1) &&
                positionMetadata.probableDeltaToNormalPathOptimalPosition() < -2)
            {
                return 100.0;
            }

            double expectedMiningDelay = observations.expectedMiningDelay(false, commandFrame);
            auto collisionDelay = expectedPatchCollisionDelay(observations.collisions, observations.nonCollisions);
            return positionMetadata.averageDeltaToNormalPathOptimalPosition() + deltaToFirstResend + expectedMiningDelay + collisionDelay;
        }

        PositionEvaluation evaluateSecondResendPositions(int commandFrame,
                                                         const PositionObservationMetadata &positionMetadata,
                                                         const PositionAndVelocity &here,
                                                         int deltaToFirstResend,
                                                         const ResendPositionObservations &observations,
                                                         const std::unordered_map<PositionAndVelocity, int> &nextPositions)
        {
            // Start by getting the data for doing a second resend at all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            if (positionMetadata.resendChangesPath == 1)
            {
                double deltaAccumulator = 0.0;
                int occurrenceCount = 0;
                int bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : nextPositions)
                {
                    auto nextPositionDataIt = positionMetadata.secondResendMetadata.find(nextPosition);
                    if (nextPositionDataIt == positionMetadata.secondResendMetadata.end())
                    {
#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "ERROR: No second resend metadata found for next position " << nextPosition
                                   << " from " << positionMetadata.pos;
#if OPTIMALPOSITIONS_DEBUG_VERBOSE
                        std::ostringstream dbg;
                        dbg << "Second resend positions:";
                        for (const auto &pos : positionMetadata.expectedPathAfterResend())
                        {
                            dbg << "\n" << pos->pos;
                            if (pos->next.empty())
                            {
                                dbg << " (no next)";
                            }
                            else
                            {
                                dbg << " : " << pos->next.begin()->first;
                            }
                        }
                        Log::Get() << dbg.str();
#endif
#endif
                        continue;
                    }

                    auto nextPositionEvaluation = evaluateSecondResendPositions(commandFrame + 1,
                                                                                positionMetadata,
                                                                                nextPosition,
                                                                                nextPositionDataIt->second.deltaToFirstResend,
                                                                                nextPositionDataIt->second.observations,
                                                                                nextPositionDataIt->second.next);
                    deltaAccumulator += nextPositionEvaluation.expectedDelta * occurrences;
                    occurrenceCount += occurrences;
                    if (occurrences > bestOccurrences)
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelta = (deltaAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), here);

            // We can't send another command at LF after previous command
            if (deltaToFirstResend == BWAPI::Broodwar->getLatencyFrames()) return nextPositionsEvaluation;

            // We can't send a command LF+1 frames before an order process timer reset
            if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // If we want to try this position and it is better than the current best, return this
            int probableDeltaToNormalPathOptimalPosition = positionMetadata.probableDeltaToNormalPathOptimalPosition();
            if (observations.empty()
                && probableDeltaToNormalPathOptimalPosition >= -EXPLORE_BEFORE
                && probableDeltaToNormalPathOptimalPosition <= EXPLORE_AFTER
                && (WorkerMiningOptimization::isExploring() || (probableDeltaToNormalPathOptimalPosition == 0 && deltaToFirstResend == 0)))
            {
                int positionToTryDelta = std::abs(probableDeltaToNormalPathOptimalPosition + deltaToFirstResend);
                if (!nextPositionsEvaluation.positionToTryOnExpectedPath || positionToTryDelta < nextPositionsEvaluation.positionToTryDelta)
                {
                    return {100, {here}, std::make_shared<PositionAndVelocity>(positionMetadata.pos), true, positionToTryDelta};
                }
            }

            // If the next positions' expected path has a position to try, return it
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // Compute the expected delta for this position
            double expectedDelta = computeExpectedDelta(commandFrame, positionMetadata, deltaToFirstResend, observations);
            if (expectedDelta < (nextPositionsEvaluation.expectedDelta - EPSILON))
            {
                return {expectedDelta, {here}, std::make_shared<PositionAndVelocity>(positionMetadata.pos), false, 0};
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(int commandFrame,
                                            const std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &allPositionData,
                                            const PositionObservationMetadata &positionMetadata)
        {
            // Start by getting the data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            {
                double deltaAccumulator = 0.0;
                int occurrenceCount = 0;
                int bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : positionMetadata.next)
                {
                    auto nextPositionDataIt = allPositionData.find(nextPosition);
                    if (nextPositionDataIt == allPositionData.end())
                    {
#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "ERROR: No metadata found for next position " << nextPosition;
#endif
                        continue;
                    }

                    auto nextPositionEvaluation = evaluatePosition(commandFrame + 1, allPositionData, nextPositionDataIt->second);
                    deltaAccumulator += nextPositionEvaluation.expectedDelta * occurrences;
                    occurrenceCount += occurrences;
                    if (occurrences > bestOccurrences)
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelta = (deltaAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), positionMetadata.pos);

            // Now evaluate this position using the second resend metadata
            auto evaluationHere = evaluateSecondResendPositions(commandFrame,
                                                                positionMetadata,
                                                                positionMetadata.pos,
                                                                0,
                                                                positionMetadata.noSecondResendObservations,
                                                                positionMetadata.next);

            // If one of the branches wants to explore, return it
            if (evaluationHere.positionToTryOnExpectedPath &&
                (!nextPositionsEvaluation.positionToTryOnExpectedPath
                 || evaluationHere.positionToTryDelta < nextPositionsEvaluation.positionToTryDelta))
            {
                return evaluationHere;
            }
            else if (nextPositionsEvaluation.positionToTryOnExpectedPath)
            {
                return nextPositionsEvaluation;
            }

            // Return the best branch
            if (evaluationHere.expectedDelta < (nextPositionsEvaluation.expectedDelta - 0.0001))
            {
                return evaluationHere;
            }

            return nextPositionsEvaluation;
        }
    }

    void planResendsSingle(WorkerGatherStatus &workerStatus,
                           const std::unordered_map <PositionAndVelocity, PositionObservationMetadata> &optimalPositions,
                           const std::shared_ptr <PositionAndVelocity> &currentPosition)
    {
        // Reference this position's metadata
        auto metadataIt = optimalPositions.find(*currentPosition);
        if (metadataIt == optimalPositions.end()) return; // haven't reached an observed position yet
        auto &positionMetadata = metadataIt->second;

        // For the single worker optimization case, we skip positions we haven't observed on a path with no resend, as we don't have anything
        // to compare against
        if (positionMetadata.deltaToNormalPathOptimalPosition.empty()) return;

        // Skip this position if it comes before our exploration horizon
        int delta = positionMetadata.largestDeltaToNormalPathOptimalPosition();
        if (delta < -EXPLORE_BEFORE) return;

        // Also skip this position if it is unstable and comes early in the path
        if ((positionMetadata.deltaToNormalPathOptimalPosition.size() > 1 || positionMetadata.next.size() > 1)
            && delta < -5)
        {
            return;
        }

        // We are now sure that we will plan something, though we may choose not to perform a resend
        workerStatus.resendsPlanned = true;

        auto shouldResend = [&](const PositionEvaluation &evaluation)
        {
            if (!evaluation.resendPosition) return false;
            if (evaluation.positionToTryOnExpectedPath) return true;

            // Ensure the path gets us to the patch better than the worst case of letting the worker be
            auto normalPathCollisionDelay = expectedPatchCollisionDelay(positionMetadata.noResendCollisions,
                                                                        positionMetadata.noResendNonCollisions);
            if (evaluation.expectedDelta > (9 + normalPathCollisionDelay)) return false;

            // If we can predict the worker's order process timer at normal arrival, check if it is better than the evaluated result
            double orderProcessTimerDelay = 4.5;
            int framesToNormalPathArrival = BWAPI::Broodwar->getLatencyFrames() + 10 - positionMetadata.probableDeltaToNormalPathOptimalPosition();
            int orderProcessTimerAtArrival =
                    OrderProcessTimer::unitOrderProcessTimerAtDelta(workerStatus.worker->orderProcessTimer, framesToNormalPathArrival);
            if (orderProcessTimerAtArrival != -1)
            {
                orderProcessTimerDelay = (double)orderProcessTimerAtArrival;
            }

            if ((normalPathCollisionDelay + orderProcessTimerDelay) < evaluation.expectedDelta)
            {
#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(workerStatus.worker->id) << std::fixed << std::setprecision(1)
                                                        << "Not resending as expected order timer delay " << orderProcessTimerDelay
                                                        << " and collision delay " << normalPathCollisionDelay
                                                        << " is better than expected delta " << evaluation.expectedDelta;
#endif

                return false;
            }

            return true;
        };

        auto evaluation = evaluatePosition(BWAPI::Broodwar->getFrameCount(), optimalPositions, positionMetadata);
        if (shouldResend(evaluation))
        {
            workerStatus.plannedResendPosition = evaluation.resendPosition;
            workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
            if ((*workerStatus.plannedResendPosition) == (*workerStatus.plannedSecondResendPosition))
            {
                workerStatus.plannedSecondResendPosition = nullptr;
            }

            workerStatus.expectedPath = std::move(evaluation.expectedPath);

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
            if (workerStatus.plannedSecondResendPosition)
            {
                out << " : " << *workerStatus.plannedSecondResendPosition;
            }
            if (evaluation.positionToTryOnExpectedPath)
            {
                out << " (exploring)";
            }
            else
            {
                out << " expected delta " << evaluation.expectedDelta;
            }

            CherryVis::log(workerStatus.worker->id) << out.str();
#endif
        }
    }

    void validatePlannedPathSingle(WorkerGatherStatus &workerStatus,
                                   BWAPI::Unit resourceBwapiUnit,
                                   const std::unordered_map <PositionAndVelocity, PositionObservationMetadata> &optimalPositions,
                                   const std::shared_ptr <PositionAndVelocity> &currentPosition)
    {
        if (workerStatus.expectedPath.empty()) return; // have no further resends planned
        if (workerStatus.expectedPath.front() == *currentPosition) return; // path matches expectations

        // We have reached an unexpected position

        // If we haven't passed the first resend position yet, then just clear the planned data so we can replan
        auto resentPosition = workerStatus.resentPosition();
        if (!resentPosition)
        {
#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path; expected " << workerStatus.expectedPath.front()
                                                            << "; actual " << *currentPosition
                                                            << "; falling through to replan resend position";
#endif

            workerStatus.resendsPlanned = false;
            workerStatus.expectedPath.clear();
            workerStatus.plannedResendPosition = nullptr;
            workerStatus.plannedSecondResendPosition = nullptr;
            return;
        }

        // We have sent the first resend, but hit a different path before reaching the second resend position
        auto resentPositionDataIt = optimalPositions.find(*resentPosition);
        if (resentPositionDataIt == optimalPositions.end())
        {
#if OPTIMALPOSITIONS_DEBUG
            Log::Get() << "ERROR: Didn't find resend position metadata: " << *resentPosition
                               << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
            return;
        }

        auto &resentPositionData = resentPositionDataIt->second;

        // Check if we have observed this path
        auto secondGatherPositionIt = resentPositionData.secondResendMetadata.find(*currentPosition);
        if (secondGatherPositionIt == resentPositionData.secondResendMetadata.end())
        {
            // We haven't observed this path, so let's just schedule a resend at the same delta and hope the result will be the same
            secondGatherPositionIt = resentPositionData.secondResendMetadata.find(*workerStatus.plannedSecondResendPosition);
            if (secondGatherPositionIt == resentPositionData.secondResendMetadata.end())
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "ERROR: Didn't find resend position in positions history: " << *resentPosition
                                   << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return;
            }

            // Get the delta from the first resend position to here
            auto positionIt = workerStatus.positionHistory.rbegin();
            for (; positionIt != workerStatus.positionHistory.rend(); positionIt++)
            {
                if ((**positionIt) == (*resentPosition)) break;
            }
            if (positionIt == workerStatus.positionHistory.rend())
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "ERROR: Didn't find resend position in positions history: " << *resentPosition
                                   << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return;
            }

            int resendIn = secondGatherPositionIt->second.deltaToFirstResend
                           - (int)std::distance(workerStatus.positionHistory.rbegin(), positionIt);
            if (resendIn == 0)
            {
                workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
            }
            else
            {
                workerStatus.resendCommandOnFrame = currentFrame + resendIn;
            }
            return;
        }

        // We have observed this path, so we can replan given that we already performed one resend

        // Evaluate second resends
        auto evaluation = evaluateSecondResendPositions(BWAPI::Broodwar->getFrameCount() + secondGatherPositionIt->second.deltaToFirstResend,
                                                        resentPositionData,
                                                        *currentPosition,
                                                        secondGatherPositionIt->second.deltaToFirstResend,
                                                        secondGatherPositionIt->second.observations,
                                                        secondGatherPositionIt->second.next);

        // Evaluate no resend
        double expectedDelta = computeExpectedDelta(BWAPI::Broodwar->getFrameCount(),
                                                    resentPositionData,
                                                    0,
                                                    resentPositionData.noSecondResendObservations);

        // Pick the best strategy - either resend at a different position or clear
        if (evaluation.positionToTryOnExpectedPath ||
            (evaluation.expectedDelta < 10 && evaluation.expectedDelta < (expectedDelta + EPSILON)))
        {
            workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
            workerStatus.expectedPath = std::move(evaluation.expectedPath);
        }
        else
        {
            workerStatus.plannedSecondResendPosition = nullptr;
            workerStatus.expectedPath.clear();
        }
    }
}