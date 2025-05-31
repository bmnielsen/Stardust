// Worker mining optimization is split into multiple files
// This file contains the logic to find the optimal path from a position for a worker mining a patch together with another worker

#include "WorkerMiningOptimization.h"
#include "PathTraversalLoopGuard.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include <optional>

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

        double expectedPatchCollisionDelay(uint8_t collisionRate)
        {
            // A collision adds an extra order process timer cycle of delay
            return 9.0 * (double)collisionRate / 255.0;
        }

        struct PositionEvaluation
        {
            double expectedDelay = 100.0; // Relative to takeover frame
            double expectedCollisionDelay = 0.0;
            int expectedArrivalFrame = -1;
            int potentialPatchSwitchFrame = INT_MAX;
            bool positionToTryOnExpectedPath = false;
            bool hasUnexploredPositionOnExpectedPath = false;
            bool explored = false;
            std::deque<GatherPositionObservationPtr> expectedPath;
            std::unique_ptr<GatherPositionObservationPtr> resendPosition;

            static PositionEvaluation patchSwitch(int frame)
            {
                return {0.0, 0.0, -1, frame};
            }

            static PositionEvaluation exploring(GatherPositionObservations &firstResend, GatherPositionObservationPtr secondResend)
            {
                return {0.0, 0.0, -1, INT_MAX, true, false, false, {secondResend}, std::make_unique<GatherPositionObservationPtr>(&firstResend)};
            }

            static PositionEvaluation resends(double delay,
                                              double collisionDelay,
                                              int expectedArrivalFrame,
                                              GatherPositionObservations &firstResend,
                                              GatherPositionObservationPtr secondResend,
                                              bool unexploredPositionOnExpectedPath)
            {
                return {delay,
                        collisionDelay,
                        expectedArrivalFrame,
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

            if (observations.arrivalDelayAndOccurrenceRate.size() == 1)
            {
                return arrivalDelayToMiningDelay(observations.arrivalDelayAndOccurrenceRate.begin()->first);
            }

            double totalMiningDelay = 0.0;
            for (const auto &[arrivalDelay, occurrenceRate] : observations.arrivalDelayAndOccurrenceRate)
            {
                auto miningDelay = arrivalDelayToMiningDelay(arrivalDelay);
                if (!miningDelay.has_value()) return std::nullopt;

                totalMiningDelay += (miningDelay.value() * ((double)occurrenceRate / 255.0));
            }

            return totalMiningDelay;
        }

        PositionEvaluation evaluateSecondResendPositions(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
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
                if (isPatchSwitchPossible(workerStatus, simulationFrame, workerOrderProcessTimer, nextPosition.pos))
                {
                    return PositionEvaluation::patchSwitch(simulationFrame + 1);
                }

                return evaluateSecondResendPositions(workerStatus,
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
                }
                nextPositionsEvaluation.expectedDelay = delayAccumulator;
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
            if (!expectedDelay.has_value()) return nextPositionsEvaluation;

            auto expectedCollisionDelay = expectedPatchCollisionDelay(observations.collisionRate);

            // Use this position if the next ones have a potential patch switch or this one has a better delay
            if (nextPositionsEvaluation.potentialPatchSwitchFrame != INT_MAX
                || expectedDelay.value() < (nextPositionsEvaluation.expectedDelay - EPSILON)
                || (expectedDelay.value() < (nextPositionsEvaluation.expectedDelay + EPSILON)
                    && expectedCollisionDelay < (nextPositionsEvaluation.expectedCollisionDelay - EPSILON)))
            {
                return PositionEvaluation::resends(expectedDelay.value(),
                                                   expectedCollisionDelay,
                                                   simulationFrame + 11 + BWAPI::Broodwar->getLatencyFrames() + observations.mostCommonArrivalDelay(),
                                                   firstResend,
                                                   here,
                                                   nextPositionsEvaluation.hasUnexploredPositionOnExpectedPath);
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(const WorkerGatherStatus &workerStatus, // NOLINT(*-no-recursion)
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
                if (isPatchSwitchPossible(workerStatus, simulationFrame, workerOrderProcessTimer, nextPosition.pos))
                {
                    return PositionEvaluation::patchSwitch(simulationFrame + 1);
                }

                return evaluatePosition(workerStatus,
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
                }
                nextPositionsEvaluation.expectedDelay = delayAccumulator;
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
                                                                simulationFrame,
                                                                simulationFrame,
                                                                workerOrderProcessTimer,
                                                                positionMetadata,
                                                                GatherPositionObservationPtr(&positionMetadata),
                                                                0);

            // If exploring, return now
            if (evaluationHere.positionToTryOnExpectedPath) return evaluationHere;

            // If the next positions have a potential patch switch, use this evaluation
            if (nextPositionsEvaluation.potentialPatchSwitchFrame != INT_MAX)
            {
                return evaluationHere;
            }

            // Return the best branch
            if (!nextPositionsEvaluation.explored
                || evaluationHere.expectedDelay < (nextPositionsEvaluation.expectedDelay - EPSILON)
                || (evaluationHere.expectedDelay < (nextPositionsEvaluation.expectedDelay + EPSILON)
                    && evaluationHere.expectedCollisionDelay < (nextPositionsEvaluation.expectedCollisionDelay - EPSILON)))
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
                                           BWAPI::Broodwar->getFrameCount(),
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
                        << "; expected collision delay " << evaluation.expectedCollisionDelay
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
        if (workerStatus.expectedPath.empty()) return true; // have no further resends planned
        if (workerStatus.expectedPath.front().position() == *currentPosition) return true; // path matches expectations

        // We always need to clear second resend and path expectations
        workerStatus.plannedSecondResendPosition = nullptr;
        workerStatus.expectedPath.clear();
        workerStatus.expectedArrivalFrame = -1;

        // If we haven't passed the first resend position yet, then try to replan
        if (!workerStatus.resentPosition())
        {
#if TAKEOVER_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path; replanning";
#endif

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
                                                        currentFrame,
                                                        currentFrame,
                                                        workerStatus.worker->orderProcessTimer,
                                                        firstResend,
                                                        *workerStatus.currentNode,
                                                        deltaFromFirstResend);

        // Don't use the position if we aren't exploring and:
        // - It hasn't been explored
        // - It has unexplored positions and is imperfect
        if (!evaluation.positionToTryOnExpectedPath
            && (!evaluation.explored || (evaluation.hasUnexploredPositionOnExpectedPath && evaluation.expectedDelay > 0.5)))
        {
            return false;
        }

        workerStatus.plannedSecondResendPosition = std::make_unique<GatherPositionObservationPtr>(evaluation.expectedPath.back());
        workerStatus.expectedPath = std::move(evaluation.expectedPath);
        workerStatus.expectedArrivalFrame = evaluation.expectedArrivalFrame;
        return true;
    }
}
