// Worker mining optimization is split into multiple files
// This file contains the logic to find the optimal path from a position for a worker mining a patch together with another worker

#include "WorkerMiningOptimization.h"
#include "PathTraversalLoopGuard.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include <optional>
#include <ranges>

#include "Geo.h"

#define EPSILON 0.000001

/*
 * The algorithm implemented here is similar to the one for a single worker, but with the added constraint that it needs to consider patch locking
 * and switching.
 *
 * When patch locking is not possible, we optimize for being able to start mining as early as possible after the other worker is finished, in a
 * position that minimizes collision delays. While approaching and waiting at the patch, we ensure that patch switching does not occur, taking into
 * account any order process timer resets during this phase.
 *
 * Patch locking uses a forecast of how likely we think all other patches will be mined at any given frame. This forecast has been built in such a
 * way that we minimize false positives (flagging that all other patches will be mined when they in fact will not be), but as a result it does have
 * a higher rate of false negatives, especially as one looks further into the future, where the workers mining other patches may have not planned
 * their approach paths yet.
 *
 * In many cases, the algorithm will be able to find a patch locking path already at the initial path planning phase. But if it doesn't, we will
 * check during the approach if the forecast has become more favourable around the time the worker is expected to arrive at the patch. If the
 * forecast improves, we will check if replanning finds a better solution. Similarly, if we have planned a patch locking path and the forecast now
 * looks worse than before, we will replan for a normal approach.
 *
 * The logic for handling mineral locking while waiting at the patch also understands patch locking, so it is possible the worker will still be able
 * to patch lock after reaching the patch, if it has a lot of waiting time.
 *
 */
namespace WorkerMiningOptimization
{
    namespace
    {
        bool shouldExploreCollisions(uint32_t collisions, uint32_t nonCollisions)
        {
            uint32_t total = collisions + nonCollisions;

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

        double otherPatchesForecastAtFrame(const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast, int frame)
        {
            int frameIdx = frame - currentFrame - 1;
            if (frameIdx < 0 || frameIdx >= GATHER_FORECAST_FRAMES) return 0.0;
            return otherPatchesForecast[frameIdx];
        }

        bool isPatchSwitchPossible(const WorkerGatherStatus &workerStatus,
                                   const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast,
                                   int simulationFrame,
                                   int orderProcessTimer,
                                   const PositionAndVelocity &pos)
        {
            if (simulationFrame >= workerStatus.takeoverFrame) return false;
            if (orderProcessTimer > 0) return false;

            auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                pos.pos(),
                                                BWAPI::UnitTypes::Resource_Mineral_Field,
                                                workerStatus.resource->center);
            if (dist > 10) return false;

            if (otherPatchesForecastAtFrame(otherPatchesForecast, simulationFrame) < PATCH_LOCK_THRESHOLD)
            {
                return true;
            }

            return false;
        }

        double expectedPatchCollisionDelay(uint8_t collisionRate)
        {
            // A collision adds an extra order process timer cycle of delay
            return 9.0 * (double)collisionRate / 255.0;
        }

        struct PositionEvaluation
        {
            int patchLockFrameDelta = -1; // Relative to takeover frame, higher means further before
            double expectedDelay = 100.0; // Relative to takeover frame
            double expectedCollisionDelay = 0.0;
            std::vector<std::pair<int, int>> expectedArrivalFrameAndOccurrenceRate;
            int potentialPatchSwitchFrame = INT_MAX;
            bool positionToTryOnExpectedPath = false;
            bool hasUnexploredPositionOnExpectedPath = false;
            bool explored = false;
            std::deque<GatherPositionObservationPtr> expectedPath;
            std::unique_ptr<GatherPositionObservationPtr> resendPosition;

            // Combines the logic we use to determine whether one evaluation is better than another
            // Note that this is not intended to be reversible - rather it is intended to be called on one position's evaluation with the next
            // positions provided as an argument
            [[nodiscard]] bool isBetterThan(const PositionEvaluation &other) const
            {
                // Use this if the other is unusable for whatever reason
                if (other.potentialPatchSwitchFrame != INT_MAX) return true;
                if (!other.explored) return true;

                // Possibility of patch lock is most important
                if (other.patchLockFrameDelta >= 0 && patchLockFrameDelta < 0) return false;
                if (patchLockFrameDelta >= 0 && other.patchLockFrameDelta < 0) return true;

                // If both have a possible patch lock, consider collision delay in the weighting
                if (other.patchLockFrameDelta >= 0 && patchLockFrameDelta >= 0)
                {
                    // We subtract the collision delay from each frame delta to balance locking early with avoiding collisions
                    double otherPatchLockScore = (double)other.patchLockFrameDelta - other.expectedCollisionDelay;
                    double thisPatchLockScore = (double)patchLockFrameDelta - expectedCollisionDelay;
                    return thisPatchLockScore > otherPatchLockScore;
                }

                // Compare the delays
                // Since we have started doing patch lock optimization, we are now including the collision delay here to optimize for fewer collisions
                // The reason for this is that it is now much more important to get back to the patch quickly, to give the largest window of
                // opportunity for patch locking
                auto thisDelay = expectedDelay + expectedCollisionDelay;
                auto otherDelay = other.expectedDelay + other.expectedCollisionDelay;

                if (thisDelay < (otherDelay - EPSILON)) return true;
                return false;
            }

            static PositionEvaluation patchSwitch(int frame)
            {
                return {-1, 0.0, 0.0, {}, frame};
            }

            static PositionEvaluation exploring(GatherPositionObservations &firstResend, GatherPositionObservationPtr secondResend)
            {
                return {-1, 0.0, 0.0, {}, INT_MAX, true, false, false, {secondResend}, std::make_unique<GatherPositionObservationPtr>(&firstResend)};
            }

            static PositionEvaluation resends(int patchLockFrameDelta,
                                              double delay,
                                              double collisionDelay,
                                              int simulationFrame,
                                              const GatherResendArrivalObservations &arrivalObservations,
                                              GatherPositionObservations &firstResend,
                                              GatherPositionObservationPtr secondResend,
                                              bool unexploredPositionOnExpectedPath)
            {
                // Generate the possible arrival frames with their weighting
                std::vector<std::pair<int, int>> _expectedArrivalFrameAndOccurrenceRate;
                _expectedArrivalFrameAndOccurrenceRate.reserve(arrivalObservations.packedArrivalDelayAndFacingPatchToOccurrenceRate.size());
                for (const auto &[packedArrivalDelayAndFacingPatch, occurrenceRate]
                    : arrivalObservations.packedArrivalDelayAndFacingPatchToOccurrenceRate)
                {
                    _expectedArrivalFrameAndOccurrenceRate.emplace_back(
                        simulationFrame + 11 + BWAPI::Broodwar->getLatencyFrames()
                            + GatherResendArrivalObservations::unpackArrivalDelay(packedArrivalDelayAndFacingPatch),
                        (int)occurrenceRate);
                }

                return {patchLockFrameDelta,
                        delay,
                        collisionDelay,
                        std::move(_expectedArrivalFrameAndOccurrenceRate),
                        INT_MAX,
                        false,
                        unexploredPositionOnExpectedPath,
                        true,
                        {secondResend},
                        std::make_unique<GatherPositionObservationPtr>(&firstResend)};
            }
        };

        bool less(const PositionEvaluation &first, const PositionEvaluation &second)
        {
            if (first.expectedPath.empty() && second.expectedPath.empty()) return first.expectedDelay < second.expectedDelay;
            if (first.expectedPath.empty()) return true;
            if (second.expectedPath.empty()) return false;
            return first.expectedPath.begin()->position() < second.expectedPath.begin()->position();
        }

        std::optional<double> computeExpectedDelay(const WorkerGatherStatus &workerStatus,
                                                   int simulationFrame,
                                                   const GatherResendArrivalObservations &observations)
        {
            // Given an arrival delay, figures out how long after the takeover frame mining will start
            // Returns nullopt if this arrival delay is unusable
            auto packedArrivalDelayAndFacingPatchToMiningDelay =
                    [&](int8_t packedArrivalDelayAndFacingPatch)->std::optional<double>
            {
                int arrivalDelay = GatherResendArrivalObservations::unpackArrivalDelay(packedArrivalDelayAndFacingPatch);
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
                double miningDelay =
                        GatherResendArrivalObservations::packedArrivalDelayAndFacingPatchToMiningDelay(
                                packedArrivalDelayAndFacingPatch, simulationFrame);

                // Adjust it to be relative to the takeover frame
                return miningDelay + (miningStartFrame - workerStatus.takeoverFrame);
            };

            if (observations.packedArrivalDelayAndFacingPatchToOccurrenceRate.size() == 1)
            {
                return packedArrivalDelayAndFacingPatchToMiningDelay(observations.packedArrivalDelayAndFacingPatchToOccurrenceRate.begin()->first);
            }

            double totalMiningDelay = 0.0;
            for (const auto &[packedArrivalDelayAndFacingPatch, occurrenceRate]
                : observations.packedArrivalDelayAndFacingPatchToOccurrenceRate)
            {
                auto miningDelay = packedArrivalDelayAndFacingPatchToMiningDelay(packedArrivalDelayAndFacingPatch);
                if (!miningDelay.has_value()) return std::nullopt;

                totalMiningDelay += (miningDelay.value() * ((double)occurrenceRate / 255.0));
            }

            return totalMiningDelay;
        }

        std::optional<std::pair<int, double>> evaluatePatchLock(const MyWorker &worker,
                                                                const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast,
                                                                int lastResendFrame,
                                                                int arrivalFrame)
        {
            // Compute the worker's order process timer at the start of the arrival frame
            int orderProcessTimerAtArrival;
            if (lastResendFrame == -1)
            {
                // Not sure if we will ever evaluate a no-resend approach, but including this in case we want to at some point
                orderProcessTimerAtArrival =
                        OrderProcessTimer::unitOrderProcessTimerAtDelta(worker->orderProcessTimer, arrivalFrame - currentFrame - 1);
            }
            else
            {
                int commandFrame = lastResendFrame + BWAPI::Broodwar->getLatencyFrames();
                orderProcessTimerAtArrival =
                        OrderProcessTimer::unitOrderProcessTimerAtDelta(commandFrame, 10, arrivalFrame - commandFrame - 1);
            }

            // There are four possibilities we need to consider:
            // - We know the order process timer at arrival, and there is no reset before it reaches 0
            //   - Here we only need to consider the frame where it reaches 0
            // - We know the order process timer at arrival, but there is a reset before it reaches 0
            //   - Here the order process timer can reach 0 from the reset frame and 7 frames ahead
            // - We don't know the order process timer at arrival, and there is no reset in the following 8 frames
            //   - Here there is an equal probability of the order process timer reaching 0 the next 8 frames ahead
            // - We don't know the order process timer at arrival, and there is a reset in the following 8 frames
            //   - Here there is a chance the order process timer reaches 0 before the reset, then an equal probability of it reaching
            //     0 during the following 7 frames

            // To make things as simple as possible, we start by building a list of frames with the probability of the order process
            // timer reaching 0 at each
            std::vector<std::pair<int, double>> possibleFramesAndProbabilities;
            if (orderProcessTimerAtArrival != -1)
            {
                if (OrderProcessTimer::unitOrderProcessTimerAtDelta(arrivalFrame - 1, orderProcessTimerAtArrival, orderProcessTimerAtArrival) == 0)
                {
                    possibleFramesAndProbabilities.emplace_back(arrivalFrame + orderProcessTimerAtArrival, 1.0);
                }
                else
                {
                    int resetFrame = OrderProcessTimer::nextResetFrame(arrivalFrame);
                    for (int frame = resetFrame; frame <= (resetFrame + 7); frame++)
                    {
                        possibleFramesAndProbabilities.emplace_back(frame, 1.0 / 8.0);
                    }
                }
            }
            else
            {
                int resetFrame = OrderProcessTimer::nextResetFrame(arrivalFrame);
                for (int frame = arrivalFrame; frame <= (arrivalFrame + 8); frame++)
                {
                    if (frame == resetFrame)
                    {
                        double probabilityOfReachingHere = 1.0 - (double)(frame - arrivalFrame) / 9.0;
                        for (int frameAfterReset = resetFrame; frameAfterReset <= (resetFrame + 7); frameAfterReset++)
                        {
                            possibleFramesAndProbabilities.emplace_back(frameAfterReset, probabilityOfReachingHere / 8.0);
                        }
                        break;
                    }
                    possibleFramesAndProbabilities.emplace_back(frame, 1.0 / 9.0);
                }
            }

            // Now evaluate whether we expect to patch lock
            double probabilityAccumulator = 0.0;
            for (const auto &[frame, probability] : possibleFramesAndProbabilities)
            {
                probabilityAccumulator += probability * otherPatchesForecastAtFrame(otherPatchesForecast, frame);
            }
            if (probabilityAccumulator < PATCH_LOCK_THRESHOLD)
            {
                return std::nullopt;
            }
            return std::make_pair(possibleFramesAndProbabilities.rbegin()->first, probabilityAccumulator);
        }

        // Checks whether the worker is expected to patch lock, returning the last frame if it will
        template<typename T>
        std::optional<int> checkForPatchLock(const WorkerGatherStatus &workerStatus,
                                             const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast,
                                             int resendFrame,
                                             const T &arrivalFramesAndOccurrenceRates)
        {
            double probabilityAccumulator = 0.0;
            int bestOccurrenceRate = 0;
            int mostCommonLockFrame = 0;
            for (const auto &[arrivalFrame, occurrenceRate] : arrivalFramesAndOccurrenceRates)
            {
                auto result = evaluatePatchLock(
                        workerStatus.worker,
                        otherPatchesForecast,
                        resendFrame,
                        arrivalFrame);
                if (!result.has_value()) return std::nullopt;

                probabilityAccumulator += (result.value().second) * ((double)occurrenceRate / 255.0);
                if (occurrenceRate > bestOccurrenceRate)
                {
                    bestOccurrenceRate = occurrenceRate;
                    mostCommonLockFrame = result.value().first;
                }
            }

            if (probabilityAccumulator < PATCH_LOCK_THRESHOLD)
            {
                return std::nullopt;
            }
            return mostCommonLockFrame;
        }

        std::optional<int> checkForPatchLock(const WorkerGatherStatus &workerStatus,
                                             const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast,
                                             int simulationFrame,
                                             const GatherResendArrivalObservations &observations)
        {
            // Transforms the arrival delays into arrival frames
            auto arrivalFrames =
                    std::ranges::views::transform(observations.packedArrivalDelayAndFacingPatchToOccurrenceRate, [&](auto a)
            {
                return std::make_pair(
                        simulationFrame + BWAPI::Broodwar->getLatencyFrames() + 11 + GatherResendArrivalObservations::unpackArrivalDelay(a.first),
                        (int)a.second);
            });
            return checkForPatchLock(workerStatus, otherPatchesForecast, simulationFrame, arrivalFrames);
        }

        PositionEvaluation evaluateSecondResendPositions(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
                                                         const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast,
                                                         int firstResendFrame,
                                                         int simulationFrame,
                                                         int workerOrderProcessTimer,
                                                         GatherPositionObservations &firstResend,
                                                         GatherPositionObservationPtr here,
                                                         uint8_t deltaToFirstResend)
        {
            // Reference the observations and next positions
            auto &observations = (here.pos ? here.pos->noSecondResendArrivalObservations : here.secondResendPos->arrivalObservations);
            auto &nextPositions = here.nextSecondResendPositions();

            // If we have no further next positions, we are at the end of our recorded path
            // We return a patch switch to indicate that we don't want to resend commands close to the end
            if (nextPositions.empty())
            {
                return PositionEvaluation::patchSwitch(simulationFrame + 1);
            }

            // Do not resend from positions that are at the patch, unless this is a stable path moving parallel with the patch
            if (Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                        here.position().pos(),
                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                        workerStatus.resource->center) == 0)
            {
                // Require there to be at least one next position, and no next positions equal to this one
                if (nextPositions.empty()) return {};
                for (const auto &nextPos : nextPositions)
                {
                    if (nextPos.pos.pos() == here.position().pos()) return {};
                }
            }

            // Compute the order process timer for the next frame
            int nextWorkerOrderProcessTimer = nextOrderProcessTimer(simulationFrame, workerOrderProcessTimer, firstResendFrame);

            // Get the data for doing a second resend at all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            auto evaluateNextPosition = [&](SecondResendGatherPositionObservations &nextPosition)->PositionEvaluation // NOLINT(*-no-recursion)
            {
                if (isPatchSwitchPossible(workerStatus,
                                          otherPatchesForecast,
                                          simulationFrame,
                                          workerOrderProcessTimer,
                                          nextPosition.pos))
                {
                    return PositionEvaluation::patchSwitch(simulationFrame + 1);
                }

                return evaluateSecondResendPositions(workerStatus,
                                                     otherPatchesForecast,
                                                     firstResendFrame,
                                                     simulationFrame + 1,
                                                     nextWorkerOrderProcessTimer,
                                                     firstResend,
                                                     GatherPositionObservationPtr(&nextPosition),
                                                     deltaToFirstResend + 1);
            };

            if (nextPositions.size() == 1)
            {
                nextPositionsEvaluation = evaluateNextPosition(nextPositions.front());
            }
            else
            {
                double patchLockAccumulator = 0.0;
                double delayAccumulator = 0.0;
                uint8_t bestOccurrenceRate = 0;
                for (auto &nextPos : nextPositions)
                {
                    auto nextPositionEvaluation = evaluateNextPosition(nextPos);
                    if (nextPositionEvaluation.explored)
                    {
                        delayAccumulator += nextPositionEvaluation.expectedDelay * ((double)nextPos.occurrenceRate / 255.0);
                    }
                    if (nextPos.occurrenceRate > bestOccurrenceRate ||
                        (nextPos.occurrenceRate == bestOccurrenceRate && less(nextPositionEvaluation, nextPositionsEvaluation)))
                    {
                        bestOccurrenceRate = nextPos.occurrenceRate;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                    if (nextPositionEvaluation.patchLockFrameDelta >= 0)
                    {
                        patchLockAccumulator += nextPos.occurrenceRate;
                    }
                }
                nextPositionsEvaluation.expectedDelay = delayAccumulator;

                // If patch locking is not guaranteed at high enough probability, clear it
                if (nextPositionsEvaluation.patchLockFrameDelta >= 0 && (patchLockAccumulator / 255.0) < PATCH_LOCK_THRESHOLD)
                {
                    nextPositionsEvaluation.patchLockFrameDelta = -1;
                }
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
            // TODO: Figure out if any of this needs to be adjusted to take patch locking into consideration
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

            // Compute whether we expect a patch lock from resending in this position
            int patchLockFrameDelta = -1;
            auto patchLock = checkForPatchLock(workerStatus, otherPatchesForecast, simulationFrame, observations);
            if (patchLock.has_value())
            {
                patchLockFrameDelta = workerStatus.takeoverFrame - patchLock.value();
            }

            // If there is an order process timer reset before the takeover frame, we can't use this position
            // Exception is if the order process timer reset happens on the frame the command kicks in or we expect to patch lock
            // TODO: It is presumably also ok if we reach the patch before the reset, but we would have to consider Unit_Busy timings
            if (patchLockFrameDelta < 0)
            {
                int nextResetFrame = OrderProcessTimer::nextResetFrame(simulationFrame);
                if (nextResetFrame < workerStatus.takeoverFrame && nextResetFrame != (simulationFrame + BWAPI::Broodwar->getLatencyFrames()))
                {
                    return nextPositionsEvaluation;
                }
            }

            // Check if this position should be tried
            if (WorkerMiningOptimization::isExploring() &&
                (observations.empty() || shouldExploreCollisions(observations.collisions, observations.nonCollisions)))
            {
                return PositionEvaluation::exploring(firstResend, here);
            }

            // If this position hasn't been explored, mark this and return the evaluation for the next positions
            if (observations.empty())
            {
                nextPositionsEvaluation.hasUnexploredPositionOnExpectedPath = true;
                return nextPositionsEvaluation;
            }

            // Compute the expected delay for this position
            auto expectedDelay = computeExpectedDelay(workerStatus, simulationFrame, observations);
            auto expectedCollisionDelay = expectedPatchCollisionDelay(observations.collisionRate);
            auto evaluationHere = PositionEvaluation::resends(patchLockFrameDelta,
                                                              expectedDelay.has_value() ? expectedDelay.value() : 100.0,
                                                              expectedCollisionDelay,
                                                              simulationFrame,
                                                              observations,
                                                              firstResend,
                                                              here,
                                                              nextPositionsEvaluation.hasUnexploredPositionOnExpectedPath);

            if (evaluationHere.isBetterThan(nextPositionsEvaluation))
            {
                return evaluationHere;
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
                                            const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast,
                                            int simulationFrame,
                                            int workerOrderProcessTimer,
                                            GatherPositionObservations &positionMetadata)
        {
            // Compute the order process timer for the next frame
            int nextWorkerOrderProcessTimer = nextOrderProcessTimer(simulationFrame, workerOrderProcessTimer);

            // Get data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            auto evaluateNextPosition = [&](GatherPositionObservations &nextPosition)->PositionEvaluation // NOLINT(*-no-recursion)
            {
                if (isPatchSwitchPossible(workerStatus,
                                          otherPatchesForecast,
                                          simulationFrame,
                                          workerOrderProcessTimer,
                                          nextPosition.pos))
                {
                    return PositionEvaluation::patchSwitch(simulationFrame + 1);
                }

                return evaluatePosition(workerStatus,
                                        otherPatchesForecast,
                                        simulationFrame + 1,
                                        nextWorkerOrderProcessTimer,
                                        nextPosition);
            };

            if (positionMetadata.nextPositions.size() == 1)
            {
                nextPositionsEvaluation = evaluateNextPosition(positionMetadata.nextPositions.front());
            }
            else if (positionMetadata.nextPositions.size() > 1)
            {
                double patchLockAccumulator = 0.0;
                double delayAccumulator = 0.0;
                uint8_t bestOccurrenceRate = 0;
                for (auto &nextPositionMetadata : positionMetadata.nextPositions)
                {
                    auto nextPositionEvaluation = evaluateNextPosition(nextPositionMetadata);
                    if (nextPositionEvaluation.explored)
                    {
                        delayAccumulator += nextPositionEvaluation.expectedDelay * ((double)nextPositionMetadata.occurrenceRate / 255.0);
                    }
                    if (nextPositionMetadata.occurrenceRate > bestOccurrenceRate ||
                        (nextPositionMetadata.occurrenceRate == bestOccurrenceRate && less(nextPositionEvaluation, nextPositionsEvaluation)))
                    {
                        bestOccurrenceRate = nextPositionMetadata.occurrenceRate;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                    if (nextPositionEvaluation.patchLockFrameDelta >= 0)
                    {
                        patchLockAccumulator += nextPositionMetadata.occurrenceRate;
                    }
                }
                nextPositionsEvaluation.expectedDelay = delayAccumulator;

                // If patch locking is not guaranteed at high enough probability, clear it
                if (nextPositionsEvaluation.patchLockFrameDelta >= 0 && (patchLockAccumulator / 255.0) < PATCH_LOCK_THRESHOLD)
                {
                    nextPositionsEvaluation.patchLockFrameDelta = -1;
                }
            }
            nextPositionsEvaluation.expectedPath.emplace(nextPositionsEvaluation.expectedPath.begin(), &positionMetadata);

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
                                                                otherPatchesForecast,
                                                                simulationFrame,
                                                                simulationFrame,
                                                                workerOrderProcessTimer,
                                                                positionMetadata,
                                                                GatherPositionObservationPtr(&positionMetadata),
                                                                0);

            // If exploring, return now
            if (evaluationHere.positionToTryOnExpectedPath) return evaluationHere;

            if (evaluationHere.isBetterThan(nextPositionsEvaluation))
            {
                return evaluationHere;
            }

            return nextPositionsEvaluation;
        }
    }

    void planGatherResendsDouble(WorkerGatherStatus &workerStatus, GatherPositionObservations &positionMetadata)
    {
        // Don't plan anything until we have left the depot
        if (!workerStatus.hasLeftDepot) return;

        // Wait to start planning until we reach a position that is usable
        if (!positionMetadata.usableForPathPlanning()) return;

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

            // Always use a patch lock solution
            if (evaluation.patchLockFrameDelta >= 0) return true;

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

        auto evaluation = evaluatePosition(workerStatus,
                                           workerStatus.resource->getAllOtherPatchesGatheredProbabilityForecast(),
                                           currentFrame,
                                           workerStatus.worker->orderProcessTimer,
                                           positionMetadata);
        if (shouldResend(evaluation))
        {
            workerStatus.plannedResendPosition = std::move(evaluation.resendPosition);
            workerStatus.plannedSecondResendPosition = std::make_unique<GatherPositionObservationPtr>(evaluation.expectedPath.back());
            if ((*workerStatus.plannedResendPosition) == (*workerStatus.plannedSecondResendPosition))
            {
                workerStatus.plannedSecondResendPosition = nullptr;
            }

            workerStatus.expectedPath = std::move(evaluation.expectedPath);
            workerStatus.expectedArrivalFrameAndOccurrenceRate = evaluation.expectedArrivalFrameAndOccurrenceRate;
            if (evaluation.patchLockFrameDelta >= 0)
            {
                workerStatus.expectedPatchLockFrame = workerStatus.takeoverFrame - evaluation.patchLockFrameDelta;
                workerStatus.expectedMiningStartFrame = -1;
            }
            else
            {
                workerStatus.expectedPatchLockFrame = -1;
                workerStatus.expectedMiningStartFrame = workerStatus.takeoverFrame + (int)std::round(evaluation.expectedDelay) + 1;
            }

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
                        << "; expected collision delay " << evaluation.expectedCollisionDelay
                        << "; expected arrival frame(s) " << workerStatus.expectedArrivalFramesDebug()
                        << "; expected patch lock frame " << workerStatus.expectedPatchLockFrame
                        << "; expected mining start frame " << workerStatus.expectedMiningStartFrame;
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
                                                        pos.position().pos(),
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
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        // Performs replanning from the current state
        auto replan = [&]()
        {
            // We always need to clear second resend and path expectations
            workerStatus.plannedSecondResendPosition = nullptr;
            workerStatus.expectedPath.clear();
            workerStatus.expectedArrivalFrameAndOccurrenceRate.clear();
            workerStatus.expectedMiningStartFrame = -1;
            workerStatus.expectedPatchLockFrame = -1;

            // If we haven't passed the first resend position yet, then replan from scratch
            if (!workerStatus.resentPosition())
            {
                workerStatus.resendsPlanned = false;
                workerStatus.plannedResendPosition = nullptr;
                if (workerStatus.currentNode && workerStatus.currentNode->pos)
                {
                    planGatherResendsDouble(workerStatus, *workerStatus.currentNode->pos);
                }
                return workerStatus.resendsPlanned;
            }

            // Guard against having sent multiple resends
            if (workerStatus.resentPositions.size() != 1)
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "ERROR: Worker has more than one resent positions while still tracking path"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }

            // We have sent the first resend, but hit a different path before reaching the second resend position
            auto &firstResend = *workerStatus.plannedResendPosition->pos;

            // If we haven't observed this path, abandon the plan
            if (!workerStatus.currentNode)
            {
#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path and unexplored path discovered; aborting second resend";
#endif
                return false;
            }

            // We have observed this path, so we can replan the second resend position
            // First we need to figure out the delta between the first resend and the current position
            int deltaFromFirstResend = currentFrame - workerStatus.lastResendFrame();

            // Evaluate second resends
            auto evaluation = evaluateSecondResendPositions(workerStatus,
                                                            workerStatus.resource->getAllOtherPatchesGatheredProbabilityForecast(),
                                                            currentFrame,
                                                            currentFrame,
                                                            workerStatus.worker->orderProcessTimer,
                                                            firstResend,
                                                            *workerStatus.currentNode,
                                                            deltaFromFirstResend);

            // Don't use the position if we aren't exploring and:
            // - It hasn't been explored
            // - It doesn't patch lock
            // - It has unexplored positions and is imperfect
            if (!evaluation.positionToTryOnExpectedPath
                && evaluation.patchLockFrameDelta < 0
                && (!evaluation.explored || (evaluation.hasUnexploredPositionOnExpectedPath && evaluation.expectedDelay > 0.5)))
            {
                return false;
            }

            workerStatus.plannedSecondResendPosition = std::make_unique<GatherPositionObservationPtr>(evaluation.expectedPath.back());
            workerStatus.expectedPath = std::move(evaluation.expectedPath);
            workerStatus.expectedArrivalFrameAndOccurrenceRate = evaluation.expectedArrivalFrameAndOccurrenceRate;
            if (evaluation.patchLockFrameDelta >= 0)
            {
                workerStatus.expectedPatchLockFrame = workerStatus.takeoverFrame - evaluation.patchLockFrameDelta;
            }
            else
            {
                workerStatus.expectedMiningStartFrame = workerStatus.takeoverFrame + (int)std::round(evaluation.expectedDelay) + 1;
            }
            return true;
        };

        // If we have no further resends planned, return
        if (workerStatus.expectedPath.empty()) return true;

        // If path does not match expectations, replan
        if (workerStatus.expectedPath.front().position() != *currentPosition)
        {
#if TAKEOVER_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path; replanning";
#endif

            return replan();
        }

        // Re-evaluate patch locking
        // If we are now evaluating something different, replan
        auto patchLock = checkForPatchLock(workerStatus,
                                           workerStatus.resource->getAllOtherPatchesGatheredProbabilityForecast(),
                                           workerStatus.lastResendFrameIncludingPlanned(),
                                           workerStatus.expectedArrivalFrameAndOccurrenceRate);
        if ((workerStatus.expectedPatchLockFrame != -1 && (!patchLock.has_value() || patchLock.value() > workerStatus.takeoverFrame)) ||
            (workerStatus.expectedPatchLockFrame == -1 && patchLock.has_value() && patchLock.value() <= workerStatus.takeoverFrame))
        {
#if TAKEOVER_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Changed potential for patch locking; replanning";
#endif
            return replan();
        }

        return true;
    }

    std::optional<int> checkForPatchLock(const WorkerGatherStatus &workerStatus, int resendFrame)
    {
        std::vector<std::pair<int, int>> arrivalData;
        arrivalData.emplace_back(resendFrame + BWAPI::Broodwar->getLatencyFrames() + 10, 255);
        return checkForPatchLock(workerStatus, workerStatus.resource->getAllOtherPatchesGatheredProbabilityForecast(), resendFrame, arrivalData);
    }
}
