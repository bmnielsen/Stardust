// Worker mining optimization is split into multiple files
// This file contains the logic to find the optimal path from a position for a single worker mining a patch

#include "WorkerMiningOptimization.h"
#include "PathTraversalLoopGuard.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
        bool shouldExploreCollisions(uint16_t collisions, uint16_t nonCollisions)
        {
            if (!WorkerMiningOptimization::isExploring()) return false;

            uint16_t total = collisions + nonCollisions;

            // Always explore until 2 observations and stop exploring after 5
            if (total < 2) return true;
            if (total >= 5) return false;

            // In the in-between period, explore if there is disagreement
            return collisions != total && nonCollisions != total;
        }

        double expectedPatchCollisionDelay(uint16_t observedCollisions, uint16_t observedNonCollisions)
        {
            uint16_t total = observedCollisions + observedNonCollisions;
            if (total == 0) return 0.0;

            // A collision adds an extra order process timer cycle of delay
            return 9.0 * (double)observedCollisions / (double)total;
        }

        struct PositionEvaluation
        {
            bool explored = false;
            double expectedDelta = 100.0;
            std::deque<PositionAndVelocity> expectedPath;
            std::shared_ptr<PositionAndVelocity> resendPosition;
            bool positionToTryOnExpectedPath = false;
            int positionToTryDelta = 0;

            static PositionEvaluation exploring(const PositionAndVelocity &firstResend, const PositionAndVelocity &secondResend, int delta)
            {
                return {false,
                        100,
                        {secondResend},
                        std::make_shared<PositionAndVelocity>(firstResend),
                        true,
                        delta};
            }

            static PositionEvaluation resends(double delta,
                                              const PositionAndVelocity &firstResend,
                                              const PositionAndVelocity &secondResend)
            {
                return {true,
                        delta,
                        {secondResend},
                        std::make_shared<PositionAndVelocity>(firstResend)};
            }
        };

        bool less(const PositionEvaluation &first, const PositionEvaluation &second)
        {
            if (first.expectedPath.empty() && second.expectedPath.empty()) return first.expectedDelta < second.expectedDelta;
            if (first.expectedPath.empty()) return true;
            if (second.expectedPath.empty()) return false;
            return first.expectedPath.begin()->getHash() < second.expectedPath.begin()->getHash();
        }

        std::optional<double> computeExpectedDelta(int commandFrame,
                                                   const GatherPositionObservations &positionMetadata,
                                                   int deltaToFirstResend,
                                                   const GatherResendArrivalObservations &observations)
        {
            // If we don't know the normal delta to benchmark, or haven't observed this position, return nothing
            if (positionMetadata.deltaToBenchmarkAndOccurrences.empty() || observations.arrivalDelayAndOccurrences.empty())
            {
                return std::nullopt;
            }

            // Ignore positions with unstable paths where all deltas are below -2
            if ((positionMetadata.deltaToBenchmarkAndOccurrences.size() > 1 || positionMetadata.nextPositionAndOccurrences.size() > 1) &&
                positionMetadata.largestDeltaToBenchmark() < -2)
            {
                return std::nullopt;
            }

            double expectedMiningDelay = observations.expectedMiningDelay(commandFrame);
            auto collisionDelay = expectedPatchCollisionDelay(observations.collisions, observations.nonCollisions);
            return positionMetadata.averageDeltaToBenchmark() + deltaToFirstResend + expectedMiningDelay + collisionDelay;
        }

        PositionEvaluation evaluateSecondResendPositions(int commandFrame, // NOLINT(*-no-recursion)
                                                         const GatherPositionObservations &positionMetadata,
                                                         const PositionAndVelocity &here,
                                                         uint8_t deltaToFirstResend,
                                                         const GatherResendArrivalObservations &observations,
                                                         const std::unordered_map<PositionAndVelocity, uint16_t> &nextPositions,
                                                         PathTraversalLoopGuard &loopGuard)
        {
            // Ensure we don't process a looping path or recurse too deep
            if (deltaToFirstResend > 0)
            {
                if (loopGuard.push(here)) return {};
            }

            // Start by getting the data for doing a second resend at all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            if (positionMetadata.resendChangesPath == ResendChangesPath::Yes)
            {
                double deltaAccumulator = 0.0;
                uint16_t occurrenceCount = 0;
                uint16_t bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : nextPositions)
                {
                    auto nextPositionDataIt = positionMetadata.secondResendObservations.find(nextPosition);
                    if (nextPositionDataIt == positionMetadata.secondResendObservations.end())
                    {
#if LOGGING_ENABLED
                        Log::Get() << "ERROR: No second resend metadata found for next position " << nextPosition
                                   << " from " << positionMetadata.pos;
#endif
                        continue;
                    }

                    auto nextPositionEvaluation = evaluateSecondResendPositions(commandFrame + 1,
                                                                                positionMetadata,
                                                                                nextPosition,
                                                                                nextPositionDataIt->second.deltaToFirstResend,
                                                                                nextPositionDataIt->second.arrivalObservations,
                                                                                nextPositionDataIt->second.nextPositionAndOccurrences,
                                                                                loopGuard);
                    loopGuard.pop(nextPosition);
                    if (nextPositionEvaluation.explored)
                    {
                        deltaAccumulator += nextPositionEvaluation.expectedDelta * occurrences;
                        occurrenceCount += occurrences;
                    }
                    if (occurrences > bestOccurrences || (occurrences == bestOccurrences && less(nextPositionEvaluation, nextPositionsEvaluation)))
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
            int probableDeltaToBenchmark = positionMetadata.probableDeltaToBenchmark();
            if ((observations.empty() || shouldExploreCollisions(observations.collisions, observations.nonCollisions))
                && probableDeltaToBenchmark >= -GATHER_EXPLORE_BEFORE
                && probableDeltaToBenchmark <= GATHER_EXPLORE_AFTER
                && (WorkerMiningOptimization::isExploring() || (probableDeltaToBenchmark == 0 && deltaToFirstResend == 0)))
            {
                int positionToTryDelta = std::abs(probableDeltaToBenchmark + deltaToFirstResend);
                if (!nextPositionsEvaluation.positionToTryOnExpectedPath || positionToTryDelta < nextPositionsEvaluation.positionToTryDelta)
                {
                    return PositionEvaluation::exploring(positionMetadata.pos, here, positionToTryDelta);
                }
            }

            // If the next positions' expected path has a position to try, return it
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // Compute the expected delta for this position
            auto expectedDelta = computeExpectedDelta(commandFrame, positionMetadata, deltaToFirstResend, observations);
            if (!expectedDelta.has_value()) return nextPositionsEvaluation;

            if (expectedDelta.value() < (nextPositionsEvaluation.expectedDelta - EPSILON))
            {
                return PositionEvaluation::resends(expectedDelta.value(), positionMetadata.pos, here);
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(int commandFrame, // NOLINT(*-no-recursion)
                                            const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &allPositionData,
                                            const GatherPositionObservations &positionMetadata,
                                            PathTraversalLoopGuard &loopGuard)
        {
            // Ensure we don't process a looping path or recurse too deep
            if (loopGuard.push(positionMetadata.pos)) return {};

            // Jump out of the recursion when we've exceeded the exploration horizon
            if (positionMetadata.deltaToBenchmarkAndOccurrences.size() == 1 &&
                positionMetadata.deltaToBenchmarkAndOccurrences.begin()->first > GATHER_EXPLORE_AFTER)
            {
                return {};
            }

            // Start by getting the data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            {
                double deltaAccumulator = 0.0;
                uint16_t occurrenceCount = 0;
                uint16_t bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : positionMetadata.nextPositionAndOccurrences)
                {
                    auto nextPositionDataIt = allPositionData.find(nextPosition);
                    if (nextPositionDataIt == allPositionData.end())
                    {
#if LOGGING_ENABLED
                        Log::Get() << "ERROR: No metadata found for next position " << nextPosition;
#endif
                        continue;
                    }

                    auto nextPositionEvaluation = evaluatePosition(commandFrame + 1, allPositionData, nextPositionDataIt->second, loopGuard);
                    loopGuard.pop(nextPosition);
                    if (nextPositionEvaluation.explored)
                    {
                        deltaAccumulator += nextPositionEvaluation.expectedDelta * occurrences;
                        occurrenceCount += occurrences;
                    }
                    if (occurrences > bestOccurrences || (occurrences == bestOccurrences && less(nextPositionEvaluation, nextPositionsEvaluation)))
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelta = (deltaAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), positionMetadata.pos);

            // We can't send a command LF+1 frames before an order process timer reset
            // Note that this is actually ok in cases where there is a second resend later, but we can't always trust that this will happen
            // if we discover a new path branch
            if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // Now evaluate this position using the second resend metadata
            auto evaluationHere = evaluateSecondResendPositions(commandFrame,
                                                                positionMetadata,
                                                                positionMetadata.pos,
                                                                0,
                                                                positionMetadata.noSecondResendArrivalObservations,
                                                                positionMetadata.nextPositionAndOccurrences,
                                                                loopGuard);

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

            if (!evaluationHere.explored) return nextPositionsEvaluation;

            // Return the best branch
            if (!nextPositionsEvaluation.explored || evaluationHere.expectedDelta < (nextPositionsEvaluation.expectedDelta - EPSILON))
            {
                return evaluationHere;
            }

            return nextPositionsEvaluation;
        }
    }

    void planGatherResendsSingle(WorkerGatherStatus &workerStatus,
                                 const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                 const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        // Reference this position's metadata
        auto metadataIt = optimalPositions.find(*currentPosition);
        if (metadataIt == optimalPositions.end()) return; // haven't reached an observed position yet
        auto &positionMetadata = metadataIt->second;

        // Skip this position if it is unusable
        if (!positionMetadata.usableForPathPlanning())
        {
            return;
        }

        // We are now sure that we will plan something, though we may choose not to perform a resend
        workerStatus.resendsPlanned = true;

        // Check if we need to "explore" the no resend case
        if (positionMetadata.deltaToBenchmarkAndOccurrences.empty()) return;
        if (shouldExploreCollisions(positionMetadata.noResendCollisions, positionMetadata.noResendNonCollisions)) return;

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
            int framesToNormalPathArrival = BWAPI::Broodwar->getLatencyFrames() + 10 - positionMetadata.probableDeltaToBenchmark();
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

        PathTraversalLoopGuard loopGuard;
        auto evaluation = evaluatePosition(BWAPI::Broodwar->getFrameCount(), optimalPositions, positionMetadata, loopGuard);
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

    void validatePlannedGatherPathSingle(WorkerGatherStatus &workerStatus,
                                         const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (workerStatus.expectedPath.empty()) return; // have no further resends planned
        if (workerStatus.expectedPath.front() == *currentPosition) return; // path matches expectations

        // We need to clear second resend and expected path no matter what
        workerStatus.plannedSecondResendPosition = nullptr;
        workerStatus.expectedPath.clear();

        // If we haven't passed the first resend position yet, then just clear the planned data so we can replan
        auto resentPosition = workerStatus.resentPosition();
        if (!resentPosition)
        {
#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path; expected " << workerStatus.expectedPath.front()
                                                    << "; actual " << *currentPosition
                                                    << "; replanning";
#endif

            workerStatus.resendsPlanned = false;
            workerStatus.plannedResendPosition = nullptr;
            return;
        }

        // We have sent the first resend, but hit a different path before reaching the second resend position
        auto resentPositionDataIt = optimalPositions.find(*resentPosition);
        if (resentPositionDataIt == optimalPositions.end())
        {
#if LOGGING_ENABLED
            Log::Get() << "ERROR: Didn't find resend position metadata: " << *resentPosition
                       << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
            return;
        }

        auto &resentPositionData = resentPositionDataIt->second;

        // If we haven't observed this path, leave the worker alone to get data about this new path
        auto secondGatherPositionIt = resentPositionData.secondResendObservations.find(*currentPosition);
        if (secondGatherPositionIt == resentPositionData.secondResendObservations.end())
        {
            return;
        }

        // We have observed this path, so we can replan the second resend position
        int firstResendCommandFrame = BWAPI::Broodwar->getFrameCount() - secondGatherPositionIt->second.deltaToFirstResend;

        // Evaluate second resends
        PathTraversalLoopGuard loopGuard;
        auto evaluation = evaluateSecondResendPositions(BWAPI::Broodwar->getFrameCount(),
                                                        resentPositionData,
                                                        *currentPosition,
                                                        secondGatherPositionIt->second.deltaToFirstResend,
                                                        secondGatherPositionIt->second.arrivalObservations,
                                                        secondGatherPositionIt->second.nextPositionAndOccurrences,
                                                        loopGuard);

        // Use it if we want to explore
        if (evaluation.positionToTryOnExpectedPath)
        {
            workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
            workerStatus.expectedPath = std::move(evaluation.expectedPath);
            return;
        }

        // If we don't know anything about the path, and aren't exploring, leave the worker alone
        // TODO: Check if it is usually better to resend at the same delta as what we originally planned
        if (!evaluation.explored) return;

        // Evaluate no second resend
        auto expectedDelta = computeExpectedDelta(firstResendCommandFrame,
                                                  resentPositionData,
                                                  0,
                                                  resentPositionData.noSecondResendArrivalObservations);

        // Resend if the result is better than the no resend delta
        if (!expectedDelta.has_value() || evaluation.expectedDelta < (expectedDelta.value() + EPSILON))
        {
            workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
            workerStatus.expectedPath = std::move(evaluation.expectedPath);
        }
    }
}
