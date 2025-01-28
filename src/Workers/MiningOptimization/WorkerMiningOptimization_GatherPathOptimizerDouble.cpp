// Worker mining optimization is split into multiple files
// This file contains the logic to find the optimal path from a position for a worker mining a patch together with another worker

#include "WorkerMiningOptimization.h"
#include "PathTraversalLoopGuard.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "Geo.h"

#define EPSILON 0.001

/*
 * The algorithm implemented here is similar to the one for a single worker, but with the following differences:

 * - Our optimization goal is to be able to start mining as early as possible after the other worker is finished mining. In most cases, we will get
 *   to the patch earlier than this without much effort, so the optimization becomes trying to make sure we are in a position that avoids collisions.
 *
 * - The worker is not allowed to have its order process timer reach 0 within 10 pixels of the patch while the other worker is still mining. This
 *   would cause the worker to try to switch patches, likely incurring a large delay. We consider order process timer resets in this logic.
 *
 * - We still try to avoid collisions with the patch after mining completion, but these cannot be penalized the same as for single workers.
 *   In the single-worker case, every frame spent resolving the collision is a loss of efficiency. But in the two-worker case, collisions only
 *   matter when they prevent the worker from getting back to the patch in time to take over from the other worker. We therefore cannot easily
 *   know how much a collision should be penalized in our algorithm. Instead, we are only using collision rates to break ties - if we have two
 *   paths that allow the worker to take over at the same delay, we will prefer the one with the lowest collision rate.
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

        // Attempts to predict what the worker's order process timer will be on the next frame
        int nextOrderProcessTimer(int simulationFrame, int currentOrderProcessTimer, int firstResendFrame = -1)
        {
            if (firstResendFrame != -1 && (simulationFrame + 1) == (firstResendFrame + BWAPI::Broodwar->getLatencyFrames()))
            {
                // Really it sets to 0 for two frames while the worker recomputes its path, but for our logic we don't care
                return 10;
            }
            if (currentOrderProcessTimer == -1 || OrderProcessTimer::isResetFrame(simulationFrame + 1))
            {
                return -1;
            }
            int result = currentOrderProcessTimer - 1;
            if (result < 0) result = 8;
            return result;
        }

        bool isPatchSwitchPossible(const WorkerGatherStatus &workerStatus,
                                   int simulationFrame,
                                   int orderProcessTimer,
                                   const PositionAndVelocity &pos)
        {
            if (simulationFrame >= workerStatus.takeoverFrame) return false;

            auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                pos.pos(),
                                                BWAPI::UnitTypes::Resource_Mineral_Field,
                                                workerStatus.resource->center);
            return dist <= 10 && orderProcessTimer <= 0;
        }

        double expectedPatchCollisionRate(uint16_t observedCollisions, uint16_t observedNonCollisions)
        {
            uint16_t total = observedCollisions + observedNonCollisions;
            if (total == 0) return 0.0;

            return (double)observedCollisions / (double)total;
        }

        struct PositionEvaluation
        {
            double expectedDelay = 100.0; // Relative to takeover frame
            double expectedCollisionRate = 0.0;
            int expectedArrivalFrame = -1;
            int potentialPatchSwitchFrame = INT_MAX;
            bool positionToTryOnExpectedPath = false;
            bool hasUnexploredPositionOnExpectedPath = false;
            bool explored = false;
            std::deque<PositionAndVelocity> expectedPath;
            std::shared_ptr<PositionAndVelocity> resendPosition;

            static PositionEvaluation patchSwitch(int frame)
            {
                return {0.0, 0.0, -1, frame};
            }

            static PositionEvaluation exploring(const PositionAndVelocity &firstResend, const PositionAndVelocity &secondResend)
            {
                return {0.0, 0.0, -1, INT_MAX, true, false, false, {secondResend}, std::make_shared<PositionAndVelocity>(firstResend)};
            }

            static PositionEvaluation resends(double delay,
                                              double collisionRate,
                                              int expectedArrivalFrame,
                                              const PositionAndVelocity &firstResend,
                                              const PositionAndVelocity &secondResend,
                                              bool unexploredPositionOnExpectedPath)
            {
                return {delay,
                        collisionRate,
                        expectedArrivalFrame,
                        INT_MAX,
                        false,
                        unexploredPositionOnExpectedPath,
                        true,
                        {secondResend},
                        std::make_shared<PositionAndVelocity>(firstResend)};
            }
        };

        bool less(const PositionEvaluation &first, const PositionEvaluation &second)
        {
            if (first.expectedPath.empty() && second.expectedPath.empty()) return first.expectedDelay < second.expectedDelay;
            if (first.expectedPath.empty()) return true;
            if (second.expectedPath.empty()) return false;
            return first.expectedPath.begin()->getHash() < second.expectedPath.begin()->getHash();
        }

        std::optional<double> computeExpectedDelay(const WorkerGatherStatus &workerStatus,
                                                   int simulationFrame,
                                                   const GatherResendArrivalObservations &observations)
        {
            // Given an arrival delay, figures out how long after the takeover frame mining will start
            // Returns nullopt if this arrival delay is unusable
            auto arrivalDelayToMiningDelay = [&](int arrivalDelay)->std::optional<double>
            {
                int miningStartFrame = simulationFrame + BWAPI::Broodwar->getLatencyFrames() + 11;
                int arrivalFrame = miningStartFrame + arrivalDelay;

                // If we arrive 11 or more frames ahead of takeover, we can always optimize perfectly since we can freely resend commands after
                // arrival, but we have to make sure we arrive at the patch before our order timer reaches 0
                if (arrivalFrame <= (workerStatus.takeoverFrame - 11))
                {
                    if (arrivalDelay > 0) return std::nullopt;
                    return 0.0;
                }

                // If our mining start time is before the takeover frame, we can't use this position
                if (miningStartFrame < workerStatus.takeoverFrame) return std::nullopt;

                // From here we can use the same logic as for single-worker takeover, as we know our mining starts at or after the takeover frame
                // and there are no order process timer resets prior to takeover

                // Get the delay with respect to the mining start frame
                double miningDelay = GatherResendArrivalObservations::arrivalDelayToMiningDelay(arrivalDelay, simulationFrame);

                // Adjust it to be relative to the takeover frame
                return miningDelay + (miningStartFrame - workerStatus.takeoverFrame);
            };

            if (observations.arrivalDelayAndOccurrences.size() == 1)
            {
                return arrivalDelayToMiningDelay(observations.arrivalDelayAndOccurrences.begin()->first);
            }

            double totalMiningDelay = 0.0;
            uint32_t totalOccurrences = 0;
            for (const auto &[arrivalDelay, occurrences] : observations.arrivalDelayAndOccurrences)
            {
                auto miningDelay = arrivalDelayToMiningDelay(arrivalDelay);
                if (!miningDelay.has_value()) return std::nullopt;

                totalMiningDelay += (miningDelay.value() * occurrences);
                totalOccurrences += occurrences;
            }

            return totalMiningDelay / (double)totalOccurrences;
        }

        PositionEvaluation evaluateSecondResendPositions(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
                                                         int firstResendFrame,
                                                         int simulationFrame,
                                                         int workerOrderProcessTimer,
                                                         const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &allPositionData,
                                                         const GatherPositionObservations &positionMetadata,
                                                         const PositionAndVelocity &here,
                                                         uint16_t occurrencesHere,
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

            // If we have no further next positions, we are at the end of our recorded path
            // We return a patch switch to indicate that we don't want to resend commands close to the end
            if (nextPositions.empty())
            {
                return PositionEvaluation::patchSwitch(simulationFrame + 1);
            }

            // Do not resend from positions that are at the patch, unless this is a stable path moving parallel with the patch
            if (Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                        here.pos(),
                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                        workerStatus.resource->center) == 0)
            {
                // Check total next occurrences
                uint16_t nextOccurrencesTotal = 0;
                for (const auto &[nextPos, occurrences] : nextPositions)
                {
                    if (nextPos.pos() == here.pos()) return {};

                    nextOccurrencesTotal += occurrences;
                }
                if ((nextOccurrencesTotal * 3) < (occurrencesHere * 2)) return {};
            }

            // Compute the order process timer for the next frame
            int nextWorkerOrderProcessTimer = nextOrderProcessTimer(simulationFrame, workerOrderProcessTimer, firstResendFrame);

            // Get the data for doing a second resend at all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            auto evaluateNextPosition = [&](const PositionAndVelocity &nextPosition, // NOLINT(*-no-recursion)
                                            uint16_t occurrences)->PositionEvaluation
            {
                if (isPatchSwitchPossible(workerStatus, simulationFrame, workerOrderProcessTimer, nextPosition))
                {
                    return PositionEvaluation::patchSwitch(simulationFrame + 1);
                }

                if (positionMetadata.resendChangesPath == ResendChangesPath::Yes)
                {
                    auto nextPositionDataIt = positionMetadata.secondResendObservations.find(nextPosition);
                    if (nextPositionDataIt == positionMetadata.secondResendObservations.end())
                    {
#if LOGGING_ENABLED
                        Log::Get() << "ERROR: No second resend metadata found for next position " << nextPosition
                                   << " from " << positionMetadata.pos;
#endif
                        return {};
                    }

                    auto result = evaluateSecondResendPositions(workerStatus,
                                                                firstResendFrame,
                                                                simulationFrame + 1,
                                                                nextWorkerOrderProcessTimer,
                                                                allPositionData,
                                                                positionMetadata,
                                                                nextPosition,
                                                                occurrences,
                                                                nextPositionDataIt->second.deltaToFirstResend,
                                                                nextPositionDataIt->second.arrivalObservations,
                                                                nextPositionDataIt->second.nextPositionAndOccurrences,
                                                                loopGuard);
                    loopGuard.pop(nextPosition);
                    return result;
                }

                auto nextPositionDataIt = allPositionData.find(nextPosition);
                if (nextPositionDataIt == allPositionData.end())
                {
#if LOGGING_ENABLED
                    Log::Get() << "ERROR: No metadata found for next position " << nextPosition;
#endif
                    return {};
                }

                auto result = evaluateSecondResendPositions(workerStatus,
                                                            firstResendFrame,
                                                            simulationFrame + 1,
                                                            nextWorkerOrderProcessTimer,
                                                            allPositionData,
                                                            positionMetadata,
                                                            nextPosition,
                                                            occurrences,
                                                            deltaToFirstResend + 1,
                                                            nextPositionDataIt->second.noSecondResendArrivalObservations,
                                                            nextPositionDataIt->second.nextPositionAndOccurrences,
                                                            loopGuard);
                loopGuard.pop(nextPosition);
                return result;
            };

            if (nextPositions.size() == 1)
            {
                auto nextIt = nextPositions.begin();
                nextPositionsEvaluation = evaluateNextPosition(nextIt->first, nextIt->second);
            }
            else
            {
                double delayAccumulator = 0.0;
                uint16_t occurrenceCount = 0;
                uint16_t bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : nextPositions)
                {
                    auto nextPositionEvaluation = evaluateNextPosition(nextPosition, occurrences);
                    if (nextPositionEvaluation.explored)
                    {
                        delayAccumulator += nextPositionEvaluation.expectedDelay * occurrences;
                        occurrenceCount += occurrences;
                    }
                    if (occurrences > bestOccurrences || (occurrences == bestOccurrences && less(nextPositionEvaluation, nextPositionsEvaluation)))
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelay = (delayAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), here);

            // If there is a potential patch switch, back off until the last safe frame
            if (nextPositionsEvaluation.potentialPatchSwitchFrame < (simulationFrame + BWAPI::Broodwar->getLatencyFrames()))
            {
                return nextPositionsEvaluation;
            }

            // We can't send another command at LF after previous command
            if (deltaToFirstResend == BWAPI::Broodwar->getLatencyFrames()) return nextPositionsEvaluation;

            // We can't send a command LF+1 frames before an order process timer reset
            if (OrderProcessTimer::framesToNextReset(simulationFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // Avoid frames that could block commands needed for takeover, either for reset frame or takeover frame
            int orderTimerResetFrame = OrderProcessTimer::previousResetFrame(workerStatus.takeoverFrame);
            if (orderTimerResetFrame == workerStatus.takeoverFrame) orderTimerResetFrame -= 150;

            int commandFrameForTakeOver = workerStatus.takeoverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
            int commandFrameForReset = orderTimerResetFrame - BWAPI::Broodwar->getLatencyFrames();
            if ((commandFrameForTakeOver - commandFrameForReset) == BWAPI::Broodwar->getLatencyFrames()) commandFrameForReset++;

            if (simulationFrame == (commandFrameForTakeOver - BWAPI::Broodwar->getLatencyFrames()) ||
                simulationFrame == (commandFrameForReset - BWAPI::Broodwar->getLatencyFrames()))
            {
                return nextPositionsEvaluation;
            }

            // If the next positions' expected path has a position to try, return it
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // If there is an order process timer reset before the takeover frame, we can't use this position
            // Exception is if the order process timer reset happens on the frame the command kicks in
            // TODO: It is presumably also ok if we reach the patch before the reset, but we would have to consider Unit_Busy timings
            int nextResetFrame = OrderProcessTimer::nextResetFrame(simulationFrame);
            if (nextResetFrame < workerStatus.takeoverFrame && nextResetFrame != (simulationFrame + BWAPI::Broodwar->getLatencyFrames()))
            {
                return nextPositionsEvaluation;
            }

            // Check if this position should be tried
            if (WorkerMiningOptimization::isExploring() &&
                (observations.empty() || shouldExploreCollisions(observations.collisions, observations.nonCollisions)))
            {
                return PositionEvaluation::exploring(positionMetadata.pos, here);
            }

            // If this position hasn't been explored, mark this and return the evaluation for the next positions
            if (observations.empty())
            {
                nextPositionsEvaluation.hasUnexploredPositionOnExpectedPath = true;
                return nextPositionsEvaluation;
            }

            // Compute the expected delay for this position
            auto expectedDelay = computeExpectedDelay(workerStatus, simulationFrame, observations);
            if (!expectedDelay.has_value()) return nextPositionsEvaluation;

            auto expectedCollisionRate = expectedPatchCollisionRate(observations.collisions, observations.nonCollisions);

            // Use this position if the next ones have a potential patch switch or this one has a better delay
            if (nextPositionsEvaluation.potentialPatchSwitchFrame != INT_MAX
                || expectedDelay.value() < (nextPositionsEvaluation.expectedDelay - EPSILON)
                || (expectedDelay.value() < (nextPositionsEvaluation.expectedDelay + EPSILON)
                    && expectedCollisionRate < (nextPositionsEvaluation.expectedCollisionRate - EPSILON)))
            {
                return PositionEvaluation::resends(expectedDelay.value(),
                                                   expectedCollisionRate,
                                                   simulationFrame + 11 + BWAPI::Broodwar->getLatencyFrames() + observations.mostCommonArrivalDelay(),
                                                   positionMetadata.pos,
                                                   here,
                                                   nextPositionsEvaluation.hasUnexploredPositionOnExpectedPath);
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
                                            int simulationFrame,
                                            int workerOrderProcessTimer,
                                            const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &allPositionData,
                                            const GatherPositionObservations &positionMetadata,
                                            PathTraversalLoopGuard &loopGuard,
                                            uint16_t occurrencesHere = 0)
        {
            // Ensure we don't process a looping path or recurse too deep
            if (loopGuard.push(positionMetadata.pos)) return {};

            // Compute the order process timer for the next frame
            int nextWorkerOrderProcessTimer = nextOrderProcessTimer(simulationFrame, workerOrderProcessTimer);

            // Get data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;

            auto evaluateNextPosition = [&](const PositionAndVelocity &nextPosition, // NOLINT(*-no-recursion)
                                            uint16_t occurrences)->PositionEvaluation
            {
                if (isPatchSwitchPossible(workerStatus, simulationFrame, workerOrderProcessTimer, nextPosition))
                {
                    return PositionEvaluation::patchSwitch(simulationFrame + 1);
                }

                auto nextPositionDataIt = allPositionData.find(nextPosition);
                if (nextPositionDataIt == allPositionData.end())
                {
#if LOGGING_ENABLED
                    Log::Get() << "ERROR: No metadata found for next position " << nextPosition;
#endif
                    return {};
                }

                auto result = evaluatePosition(workerStatus,
                                               simulationFrame + 1,
                                               nextWorkerOrderProcessTimer,
                                               allPositionData,
                                               nextPositionDataIt->second,
                                               loopGuard,
                                               occurrences);
                loopGuard.pop(nextPosition);
                return result;
            };

            if (positionMetadata.nextPositionAndOccurrences.size() == 1)
            {
                auto nextIt = positionMetadata.nextPositionAndOccurrences.begin();
                nextPositionsEvaluation = evaluateNextPosition(nextIt->first, nextIt->second);
            }
            else
            {
                double delayAccumulator = 0.0;
                uint16_t occurrenceCount = 0;
                uint16_t bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : positionMetadata.nextPositionAndOccurrences)
                {
                    auto nextPositionEvaluation = evaluateNextPosition(nextPosition, occurrences);
                    if (nextPositionEvaluation.explored)
                    {
                        delayAccumulator += nextPositionEvaluation.expectedDelay * occurrences;
                        occurrenceCount += occurrences;
                    }
                    if (occurrences > bestOccurrences || (occurrences == bestOccurrences && less(nextPositionEvaluation, nextPositionsEvaluation)))
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelay = (delayAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), positionMetadata.pos);

            // If there is a potential patch switch, back off until the last safe frame
            if (nextPositionsEvaluation.potentialPatchSwitchFrame < (simulationFrame + BWAPI::Broodwar->getLatencyFrames()))
            {
                return nextPositionsEvaluation;
            }

            // When exploring, always explore the furthest position possible
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // We can't send a command LF+1 frames before an order process timer reset
            // Note that this is actually ok in cases where there is a second resend later, but we can't always trust that this will happen
            // if we discover a new path branch
            if (OrderProcessTimer::framesToNextReset(simulationFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // Now evaluate this position using the second resend metadata
            auto evaluationHere = evaluateSecondResendPositions(workerStatus,
                                                                simulationFrame,
                                                                simulationFrame,
                                                                workerOrderProcessTimer,
                                                                allPositionData,
                                                                positionMetadata,
                                                                positionMetadata.pos,
                                                                occurrencesHere,
                                                                0,
                                                                positionMetadata.noSecondResendArrivalObservations,
                                                                positionMetadata.nextPositionAndOccurrences,
                                                                loopGuard);

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

        workerStatus.hasPathData = true;
        workerStatus.resendsPlanned = true;

        auto shouldResend = [&](const PositionEvaluation &evaluation)
        {
            if (!evaluation.resendPosition)
            {
#if TAKEOVER_DEBUG
                CherryVis::log(workerStatus.worker->id) << "No path could be found";
#endif
                return false;
            }
            if (evaluation.positionToTryOnExpectedPath) return true;

            // If the evaluation has unexplored positions on it, only accept perfect solutions
            if (evaluation.hasUnexploredPositionOnExpectedPath && evaluation.expectedDelay > 0.5)
            {
#if TAKEOVER_DEBUG
                CherryVis::log(workerStatus.worker->id) << "Path has unexplored positions and is non-optimal";
#endif
                return false;
            }

            return true;
        };

        PathTraversalLoopGuard loopGuard;
        auto evaluation = evaluatePosition(workerStatus,
                                           BWAPI::Broodwar->getFrameCount(),
                                           workerStatus.worker->orderProcessTimer,
                                           optimalPositions,
                                           positionMetadata,
                                           loopGuard);
        if (shouldResend(evaluation))
        {
            workerStatus.plannedResendPosition = evaluation.resendPosition;
            workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
            if ((*workerStatus.plannedResendPosition) == (*workerStatus.plannedSecondResendPosition))
            {
                workerStatus.plannedSecondResendPosition = nullptr;
            }

            workerStatus.expectedPath = std::move(evaluation.expectedPath);
            workerStatus.expectedArrivalFrame = evaluation.expectedArrivalFrame;

#if TAKEOVER_DEBUG
            {
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
                    out << " expected delay " << evaluation.expectedDelay
                        << "; expected collision rate " << evaluation.expectedCollisionRate
                        << "; expected arrival frame " << evaluation.expectedArrivalFrame;
                }

                CherryVis::log(workerStatus.worker->id) << out.str();
            }

            {
                std::ostringstream out;
                out << "Expected path:";
                int frame = currentFrame;
                int orderProcessTimer = workerStatus.worker->orderProcessTimer;
                for (const auto &pos : workerStatus.expectedPath)
                {
                    if (frame == currentFrame)
                    {
                        frame++;
                        continue;
                    }

                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        pos.pos(),
                                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                                        workerStatus.resource->center);
                    out << "\n" << frame << ": " << pos << "; " << dist << "; " << orderProcessTimer;

                    frame++;
                    orderProcessTimer = nextOrderProcessTimer(frame, orderProcessTimer);
                }

                CherryVis::log(workerStatus.worker->id) << out.str();
            }
#endif
        }
    }

    bool validatePlannedGatherPathDouble(WorkerGatherStatus &workerStatus,
                                         const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (workerStatus.expectedPath.empty()) return true; // have no further resends planned
        if (workerStatus.expectedPath.front() == *currentPosition) return true; // path matches expectations

        // By default we second resend and path expectations
        workerStatus.plannedSecondResendPosition = nullptr;
        workerStatus.expectedPath.clear();
        workerStatus.expectedArrivalFrame = -1;

        // If we haven't passed the first resend position yet, then try to replan
        auto resentPosition = workerStatus.resentPosition();
        if (!resentPosition)
        {
#if TAKEOVER_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path; expected " << workerStatus.expectedPath.front()
                                                    << "; actual " << *currentPosition
                                                    << "; replanning";
#endif

            workerStatus.resendsPlanned = false;
            workerStatus.plannedResendPosition = nullptr;
            planGatherResendsDouble(workerStatus, optimalPositions, currentPosition);
            return workerStatus.resendsPlanned;
        }

        // We have sent the first resend, but hit a different path before reaching the second resend position
        auto resentPositionDataIt = optimalPositions.find(*resentPosition);
        if (resentPositionDataIt == optimalPositions.end())
        {
#if LOGGING_ENABLED
            Log::Get() << "ERROR: Didn't find resend position metadata: " << *resentPosition
                       << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
            return false;
        }

        auto &resentPositionData = resentPositionDataIt->second;

        // If we haven't observed the current position from this resend position before, we can't send a second resend
        auto secondGatherPositionIt = resentPositionData.secondResendObservations.find(*currentPosition);
        if (secondGatherPositionIt == resentPositionData.secondResendObservations.end())
        {
            return false;
        }

        // We have observed this path, so we can replan the second resend position

        // Evaluate second resends
        PathTraversalLoopGuard loopGuard;
        auto evaluation = evaluateSecondResendPositions(workerStatus,
                                                        currentFrame,
                                                        currentFrame,
                                                        workerStatus.worker->orderProcessTimer,
                                                        optimalPositions,
                                                        resentPositionData,
                                                        *currentPosition,
                                                        0,
                                                        secondGatherPositionIt->second.deltaToFirstResend,
                                                        secondGatherPositionIt->second.arrivalObservations,
                                                        secondGatherPositionIt->second.nextPositionAndOccurrences,
                                                        loopGuard);

        // Use it if we want to explore
        if (evaluation.positionToTryOnExpectedPath)
        {
            workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
            workerStatus.expectedPath = std::move(evaluation.expectedPath);
            workerStatus.expectedArrivalFrame = evaluation.expectedArrivalFrame;
            return true;
        }

        // If we don't know anything about the path, and aren't exploring, leave the worker alone
        if (!evaluation.explored) return false;

        // If the evaluation has unexplored positions on it, only accept perfect solutions
        if (evaluation.hasUnexploredPositionOnExpectedPath && evaluation.expectedDelay > 0.5)
        {
            return false;
        }

        workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
        workerStatus.expectedPath = std::move(evaluation.expectedPath);
        workerStatus.expectedArrivalFrame = evaluation.expectedArrivalFrame;

        return true;
    }
}
