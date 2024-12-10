// Worker mining optimization is split into multiple files
// This file contains the logic to find the optimal path from a position for a worker mining a patch together with another worker

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "Geo.h"

#define EPSILON 0.001

/*
 * The algorithm implemented here is similar to the one for a single worker, but with the following differences:

 * - Our optimization goal is to be able to start mining as early as possible after the other worker is finished mining. In most cases, we will get
 *   to the patch earlier than this without much effort, so the optimization becomes trying to make sure we are in a position that avoids collisions.
 *
 * - The worker is not allowed to have its order process timer reach 0 within 10 pixels of the patch while the other worker is still mining. This
 *   would cause the worker to try to switch patches, likely incurring a large delay. We need to consider order process timer resets in this logic.
 *
 * - We still try to avoid collisions with the patch after mining completion, but these are penalized differently than in the single-worker case.
 *   In the single-worker case, every frame spent resolving the collision is a loss of efficiency. But in the two-worker case, collisions only
 *   matter when they prevent the worker from getting back to the patch in time to take over from the other worker. We therefore use a variable
 *   penalty based on a distance-based heuristic (workers that need to travel farther between depot and patch are assumed to be affected more
 *   severely by collisions). This is not perfect, as there are some variables we don't consider (such as the fact that even for close patches,
 *   there can be delays if both gather and return collide), but the potential loss is very small.
 */

namespace WorkerMiningOptimization
{
    namespace
    {
        bool shouldExploreCollisions(uint16_t collisions, uint16_t nonCollisions)
        {
            uint16_t total = collisions + nonCollisions;

            // Always explore until 2 observations and stop exploring after 5
            if (total < 2) return true;
            if (total >= 5) return false;

            // In the in-between period, explore if there is disagreement
            return collisions != total && nonCollisions != total;
        }

        int nextOrderProcessTimer(int currentFrame, int currentOrderProcessTimer)
        {
            if (currentOrderProcessTimer == -1 || OrderProcessTimer::isResetFrame(currentFrame + 1))
            {
                return -1;
            }
            int result = currentOrderProcessTimer - 1;
            if (result < 0) result = 8;
            return result;
        }

        struct PositionEvaluation
        {
            double expectedDelay = 100.0; // Relative to takeover frame
            int potentialPatchSwitchFrame = INT_MAX;
            bool positionToTryOnExpectedPath = false;
            bool explored = false;
            std::deque<PositionAndVelocity> expectedPath;
            std::shared_ptr<PositionAndVelocity> resendPosition;

            static PositionEvaluation patchSwitch(int frame)
            {
                return {0.0, frame};
            }

            static PositionEvaluation exploring(const PositionAndVelocity &firstResend, const PositionAndVelocity &secondResend)
            {
                return {0.0, INT_MAX, true, false, {secondResend}, std::make_shared<PositionAndVelocity>(firstResend)};
            }

            static PositionEvaluation resends(double delay, const PositionAndVelocity &firstResend, const PositionAndVelocity &secondResend)
            {
                return {delay, INT_MAX, false, true, {secondResend}, std::make_shared<PositionAndVelocity>(firstResend)};
            }
        };

        std::optional<double> computeExpectedDelay()
        {
            // No observations: return nullopt
            // Order process timer reset after sending: don't use?
            // Don't get to the patch on time: don't use?
            // Get to the patch before takeover frame: depends on how long in advance, if 11+ before we can resend after arrival
            
            return std::nullopt;
        }

        PositionEvaluation evaluateSecondResendPositions(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
                                                         int commandFrame,
                                                         int workerOrderProcessTimer,
                                                         const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &allPositionData,
                                                         const GatherPositionObservations &positionMetadata,
                                                         const PositionAndVelocity &here,
                                                         uint8_t deltaToFirstResend,
                                                         const GatherResendArrivalObservations &observations,
                                                         const std::unordered_map<PositionAndVelocity, uint16_t> &nextPositions)
        {
            // Check if there could be a patch switch here
            if (commandFrame < workerStatus.takeoverFrame)
            {
                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    here.pos(),
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    workerStatus.resource->center);
                if (dist <= 10 && workerOrderProcessTimer <= 0)
                {
                    return PositionEvaluation::patchSwitch(commandFrame);
                }
            }

            // Compute the order process timer for the next frame
            int nextWorkerOrderProcessTimer = nextOrderProcessTimer(commandFrame, workerOrderProcessTimer);

            // Get the data for doing a second resend at all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            auto evaluateNextPosition = [&](const PositionAndVelocity &nextPosition)->PositionEvaluation // NOLINT(*-no-recursion)
            {
                if (positionMetadata.resendChangesPath == ResendChangesPath::Yes)
                {
                    auto nextPositionDataIt = positionMetadata.secondResendObservations.find(nextPosition);
                    if (nextPositionDataIt == positionMetadata.secondResendObservations.end())
                    {
#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "ERROR: No second resend metadata found for next position " << nextPosition
                                   << " from " << positionMetadata.pos;
#endif
                        return {};
                    }

                    return evaluateSecondResendPositions(workerStatus,
                                                         commandFrame + 1,
                                                         nextWorkerOrderProcessTimer,
                                                         allPositionData,
                                                         positionMetadata,
                                                         nextPosition,
                                                         nextPositionDataIt->second.deltaToFirstResend,
                                                         nextPositionDataIt->second.arrivalObservations,
                                                         nextPositionDataIt->second.nextPositionAndOccurrences);
                }

                auto nextPositionDataIt = allPositionData.find(nextPosition);
                if (nextPositionDataIt == allPositionData.end())
                {
#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "ERROR: No metadata found for next position " << nextPosition;
#endif
                    return {};
                }

                return evaluateSecondResendPositions(workerStatus,
                                                     commandFrame + 1,
                                                     nextWorkerOrderProcessTimer,
                                                     allPositionData,
                                                     nextPositionDataIt->second,
                                                     nextPosition,
                                                     0,
                                                     nextPositionDataIt->second.noSecondResendArrivalObservations,
                                                     nextPositionDataIt->second.nextPositionAndOccurrences);
            };

            if (nextPositions.size() == 1)
            {
                nextPositionsEvaluation = evaluateNextPosition(nextPositions.begin()->first);
            }
            else
            {
                double delayAccumulator = 0.0;
                uint16_t occurrenceCount = 0;
                uint16_t bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : nextPositions)
                {
                    auto nextPositionEvaluation = evaluateNextPosition(nextPosition);
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
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), here);

            // If there is a potential patch switch, back off until the last safe frame
            if (nextPositionsEvaluation.potentialPatchSwitchFrame < (currentFrame + BWAPI::Broodwar->getLatencyFrames()))
            {
                return nextPositionsEvaluation;
            }

            // We can't send another command at LF after previous command
            if (deltaToFirstResend == BWAPI::Broodwar->getLatencyFrames()) return nextPositionsEvaluation;

            // We can't send a command LF+1 frames before an order process timer reset
            if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // If the next positions' expected path has a position to try, return it
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // Check if this position should be tried
            if (WorkerMiningOptimization::isExploring() &&
                (observations.empty() || shouldExploreCollisions(observations.collisions, observations.nonCollisions)))
            {
                return PositionEvaluation::exploring(positionMetadata.pos, here);
            }

            // Compute the expected delay for this position
            auto expectedDelay = computeExpectedDelay();
            if (!expectedDelay.has_value()) return nextPositionsEvaluation;

            // Use this position if the next ones have a potential patch switch or this one has a better delay
            if (nextPositionsEvaluation.potentialPatchSwitchFrame != INT_MAX
                || expectedDelay.value() < (nextPositionsEvaluation.expectedDelay - EPSILON))
            {
                return PositionEvaluation::resends(expectedDelay.value(), positionMetadata.pos, here);
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
                                            int commandFrame,
                                            int workerOrderProcessTimer,
                                            const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &allPositionData,
                                            const GatherPositionObservations &positionMetadata)
        {
            // Check if there could be a patch switch here
            if (commandFrame < workerStatus.takeoverFrame)
            {
                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    positionMetadata.pos.pos(),
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    workerStatus.resource->center);
                if (dist <= 10 && workerOrderProcessTimer <= 0)
                {
                    return PositionEvaluation::patchSwitch(commandFrame);
                }
            }

            // Compute the order process timer for the next frame
            int nextWorkerOrderProcessTimer = nextOrderProcessTimer(commandFrame, workerOrderProcessTimer);

            // Get data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;

            auto evaluateNextPosition = [&](const PositionAndVelocity &nextPosition)->PositionEvaluation // NOLINT(*-no-recursion)
            {
                auto nextPositionDataIt = allPositionData.find(nextPosition);
                if (nextPositionDataIt == allPositionData.end())
                {
#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "ERROR: No metadata found for next position " << nextPosition;
#endif
                    return {};
                }

                return evaluatePosition(workerStatus,
                                        commandFrame + 1,
                                        nextWorkerOrderProcessTimer,
                                        allPositionData,
                                        nextPositionDataIt->second);
            };

            if (positionMetadata.nextPositionAndOccurrences.size() == 1)
            {
                nextPositionsEvaluation = evaluateNextPosition(positionMetadata.nextPositionAndOccurrences.begin()->first);
            }
            else
            {
                double delayAccumulator = 0.0;
                uint16_t occurrenceCount = 0;
                uint16_t bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : positionMetadata.nextPositionAndOccurrences)
                {
                    auto nextPositionEvaluation = evaluateNextPosition(nextPosition);
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

            // If there is a potential patch switch, back off until the last safe frame
            if (nextPositionsEvaluation.potentialPatchSwitchFrame < (currentFrame + BWAPI::Broodwar->getLatencyFrames()))
            {
                return nextPositionsEvaluation;
            }

            // When exploring, always explore the furthest position possible
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // We can't send a command LF+1 frames before an order process timer reset
            // Note that this is actually ok in cases where there is a second resend later, but we can't always trust that this will happen
            // if we discover a new path branch
            if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // Now evaluate this position using the second resend metadata
            auto evaluationHere = evaluateSecondResendPositions(workerStatus,
                                                                commandFrame,
                                                                workerOrderProcessTimer,
                                                                allPositionData,
                                                                positionMetadata,
                                                                positionMetadata.pos,
                                                                0,
                                                                positionMetadata.noSecondResendArrivalObservations,
                                                                positionMetadata.nextPositionAndOccurrences);

            // If exploring, return now
            if (evaluationHere.positionToTryOnExpectedPath) return evaluationHere;

            // If the next positions have a potential patch switch, use this evaluation
            if (nextPositionsEvaluation.potentialPatchSwitchFrame != INT_MAX)
            {
                return evaluationHere;
            }

            // Return the best branch
            if (!nextPositionsEvaluation.explored || evaluationHere.expectedDelay < (nextPositionsEvaluation.expectedDelay - EPSILON))
            {
                return evaluationHere;
            }

            return nextPositionsEvaluation;
        }
    }

    void planGatherResendsDouble(WorkerGatherStatus &workerStatus,
                                 const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                 const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        // Reference this position's metadata
        auto metadataIt = optimalPositions.find(*currentPosition);
        if (metadataIt == optimalPositions.end()) return; // haven't reached an observed position yet
        auto &positionMetadata = metadataIt->second;

        evaluatePosition(workerStatus,
                         BWAPI::Broodwar->getFrameCount(),
                         workerStatus.worker->orderProcessTimer,
                         optimalPositions,
                         positionMetadata);
    }

    void validatePlannedGatherPathDouble(WorkerGatherStatus &workerStatus,
                                         const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (workerStatus.expectedPath.empty()) return; // have no further resends planned
        if (workerStatus.expectedPath.front() == *currentPosition) return; // path matches expectations

        // We need to clear second resend and expected path no matter what
        workerStatus.plannedSecondResendPosition = nullptr;
        workerStatus.expectedPath.clear();

        // If we haven't passed the first resend position yet, then try to replan
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

            planGatherResendsDouble(workerStatus, optimalPositions, currentPosition);
            return;
        }

        // TODO: Try to replan based on the first resend having been sent

    }
}
