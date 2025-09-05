// Worker mining optimization is split into multiple files
// This file contains the logic to find the optimal path from a position for a single worker mining a patch

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"
#include "Geo.h"

#include <optional>

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
        bool shouldExploreCollisions(uint32_t collisions, uint32_t nonCollisions)
        {
            if (!WorkerMiningOptimization::isExploring()) return false;

            uint32_t total = collisions + nonCollisions;

            // Always explore until 2 observations and stop exploring after 5
            if (total < 2) return true;
            if (total >= 5) return false;

            // In the in-between period, explore if there is disagreement
            return collisions != total && nonCollisions != total;
        }

        double expectedPatchCollisionDelay(uint8_t collisionRate)
        {
            // A collision adds an extra order process timer cycle of delay
            return 9.0 * (double)collisionRate / 255.0;
        }

        struct PositionEvaluation
        {
            bool explored = false;
            double expectedDelta = 100.0;
            std::deque<GatherPositionObservationPtr> expectedPath;
            std::unique_ptr<GatherPositionObservationPtr> resendPosition;
            bool positionToTryOnExpectedPath = false;
            int positionToTryDelta = 0;

            static PositionEvaluation exploring(GatherPositionObservations &firstResend,
                                                GatherPositionObservationPtr secondResend,
                                                int delta)
            {
                return {false,
                        100,
                        {secondResend},
                        std::make_unique<GatherPositionObservationPtr>(&firstResend),
                        true,
                        delta};
            }

            static PositionEvaluation resends(double delta,
                                              GatherPositionObservations &firstResend,
                                              GatherPositionObservationPtr secondResend)
            {
                return {true,
                        delta,
                        {secondResend},
                        std::make_unique<GatherPositionObservationPtr>(&firstResend)};
            }
        };

        bool less(const PositionEvaluation &first, const PositionEvaluation &second)
        {
            if (first.expectedPath.empty() && second.expectedPath.empty()) return first.expectedDelta < second.expectedDelta;
            if (first.expectedPath.empty()) return true;
            if (second.expectedPath.empty()) return false;
            return first.expectedPath.begin()->position() < second.expectedPath.begin()->position();
        }

        std::optional<double> computeExpectedDelta(int commandFrame,
                                                   const GatherPositionObservations &positionMetadata,
                                                   int deltaToFirstResend,
                                                   const GatherResendArrivalObservations &observations)
        {
            // If we don't know the normal delta to benchmark, or haven't observed this position, return nothing
            if (positionMetadata.deltaToBenchmarkAndOccurrenceRate.empty() || observations.arrivalDelayAndOccurrenceRate.empty())
            {
                return std::nullopt;
            }

            // Ignore positions with unstable paths where all deltas are below -2
            if ((positionMetadata.deltaToBenchmarkAndOccurrenceRate.size() > 1 ||
                 positionMetadata.nextPositions.size() > 1 ||
                 positionMetadata.secondResendPositions.size() > 1) &&
                positionMetadata.largestDeltaToBenchmark() < -2)
            {
                return std::nullopt;
            }

            double expectedMiningDelay = observations.expectedMiningDelay(commandFrame);
            auto collisionDelay = expectedPatchCollisionDelay(observations.collisionRate);
            return positionMetadata.averageDeltaToBenchmark() + deltaToFirstResend + expectedMiningDelay + collisionDelay;
        }

        PositionEvaluation evaluateSecondResendPositions(int commandFrame, // NOLINT(*-no-recursion)
                                                         GatherPositionObservations &firstResend,
                                                         GatherPositionObservationPtr here,
                                                         uint8_t deltaToFirstResend,
                                                         const Resource &resource)
        {
            // Reference the observations and next positions
            auto &observations = (here.pos ? here.pos->noSecondResendArrivalObservations : here.secondResendPos->arrivalObservations);
            auto &nextPositions = here.nextSecondResendPositions();

            // Do not resend from positions that are at the patch, unless this is a stable path moving parallel with the patch
            if (Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                        here.position().pos(),
                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                        resource->center) == 0)
            {
                // Require there to be at least one next position, and no next positions equal to this one
                if (nextPositions.empty()) return {};
                for (const auto &nextPos : nextPositions)
                {
                    if (nextPos.pos.pos() == here.position().pos()) return {};
                }
            }

            // Start by getting the data for doing a second resend at all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            if (nextPositions.size() == 1)
            {
                nextPositionsEvaluation = evaluateSecondResendPositions(commandFrame + 1,
                                                                        firstResend,
                                                                        GatherPositionObservationPtr(&*nextPositions.begin()),
                                                                        deltaToFirstResend + 1,
                                                                        resource);
            }
            else if (nextPositions.size() > 1)
            {
                double deltaAccumulator = 0.0;
                uint8_t bestOccurrenceRate = 0;
                for (auto &nextPos : nextPositions)
                {
                    auto nextPositionEvaluation = evaluateSecondResendPositions(commandFrame + 1,
                                                                                firstResend,
                                                                                GatherPositionObservationPtr(&nextPos),
                                                                                deltaToFirstResend + 1,
                                                                                resource);
                    if (nextPositionEvaluation.explored)
                    {
                        deltaAccumulator += nextPositionEvaluation.expectedDelta * ((double)nextPos.occurrenceRate / 255.0);
                    }
                    if (nextPos.occurrenceRate > bestOccurrenceRate ||
                        (nextPos.occurrenceRate == bestOccurrenceRate && less(nextPositionEvaluation, nextPositionsEvaluation)))
                    {
                        bestOccurrenceRate = nextPos.occurrenceRate;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                nextPositionsEvaluation.expectedDelta = deltaAccumulator;
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), here);

            // We can't send another command at LF after previous command
            if (deltaToFirstResend == BWAPI::Broodwar->getLatencyFrames()) return nextPositionsEvaluation;

            // We can't send a command LF+1 frames before an order process timer reset
            if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // If we want to try this position and it is better than the current best, return this
            int probableDeltaToBenchmark = firstResend.probableDeltaToBenchmark();
            if (WorkerMiningOptimization::isExploring()
                && (observations.empty() || shouldExploreCollisions(observations.collisions, observations.nonCollisions))
                && probableDeltaToBenchmark >= -GATHER_EXPLORE_BEFORE
                && probableDeltaToBenchmark <= GATHER_EXPLORE_AFTER)
            {
                int positionToTryDelta = std::abs(probableDeltaToBenchmark + deltaToFirstResend);
                if (!nextPositionsEvaluation.positionToTryOnExpectedPath || positionToTryDelta < nextPositionsEvaluation.positionToTryDelta)
                {
                    return PositionEvaluation::exploring(firstResend, here, positionToTryDelta);
                }
            }

            // If the next positions' expected path has a position to try, return it
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // Compute the expected delta for this position
            auto expectedDelta = computeExpectedDelta(commandFrame, firstResend, deltaToFirstResend, observations);
            if (!expectedDelta.has_value()) return nextPositionsEvaluation;

            if (expectedDelta.value() < (nextPositionsEvaluation.expectedDelta - EPSILON))
            {
                return PositionEvaluation::resends(expectedDelta.value(), firstResend, here);
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(int commandFrame, // NOLINT(*-no-recursion)
                                            GatherPositionObservations &positionMetadata,
                                            const Resource &resource)
        {
            // Jump out of the recursion when we've exceeded the exploration horizon
            if (positionMetadata.deltaToBenchmarkAndOccurrenceRate.size() == 1 &&
                positionMetadata.deltaToBenchmarkAndOccurrenceRate.begin()->first > GATHER_EXPLORE_AFTER)
            {
                return {};
            }

            // Start by getting the data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            if (positionMetadata.nextPositions.size() == 1)
            {
                nextPositionsEvaluation = evaluatePosition(commandFrame + 1,
                                                           *positionMetadata.nextPositions.begin(),
                                                           resource);
            }
            else if (positionMetadata.nextPositions.size() > 1)
            {
                double deltaAccumulator = 0.0;
                uint8_t bestOccurrenceRate = 0;
                for (auto &nextPositionMetadata : positionMetadata.nextPositions)
                {
                    auto nextPositionEvaluation = evaluatePosition(commandFrame + 1,
                                                                   nextPositionMetadata,
                                                                   resource);
                    if (nextPositionEvaluation.explored)
                    {
                        deltaAccumulator += nextPositionEvaluation.expectedDelta * ((double)nextPositionMetadata.occurrenceRate / 255.0);
                    }
                    if (nextPositionMetadata.occurrenceRate > bestOccurrenceRate ||
                            (nextPositionMetadata.occurrenceRate == bestOccurrenceRate && less(nextPositionEvaluation, nextPositionsEvaluation)))
                    {
                        bestOccurrenceRate = nextPositionMetadata.occurrenceRate;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                nextPositionsEvaluation.expectedDelta = deltaAccumulator;
            }
            nextPositionsEvaluation.expectedPath.emplace(nextPositionsEvaluation.expectedPath.begin(), &positionMetadata);

            // We can't send a command LF+1 frames before an order process timer reset
            // Note that this is actually ok in cases where there is a second resend later, but we can't always trust that this will happen
            // if we discover a new path branch
            if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // Now evaluate this position using the second resend metadata
            auto evaluationHere = evaluateSecondResendPositions(commandFrame,
                                                                positionMetadata,
                                                                GatherPositionObservationPtr(&positionMetadata),
                                                                0,
                                                                resource);

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

    void planGatherResendsSingle(WorkerGatherStatus &workerStatus)
    {
        // Require a path node that isn't to a second resend position
        if (!workerStatus.currentNode || workerStatus.currentNode->secondResendPos) return;

        // Don't plan anything until we have left the depot
        if (!workerStatus.hasLeftDepot) return;

        auto &positionMetadata = *workerStatus.currentNode->pos;

        // Wait to start planning until we reach a position that is usable
        if (!positionMetadata.usableForPathPlanning()) return;

        // We are now sure that we will plan something, though we may choose not to perform a resend
        workerStatus.resendsPlanned = true;
        workerStatus.hasPathData = true;

        // Check if we need to "explore" the no resend case, in which case we plan to send no resends
        if (positionMetadata.deltaToBenchmarkAndOccurrenceRate.empty()) return;
        if (shouldExploreCollisions(positionMetadata.noResendCollisions, positionMetadata.noResendNonCollisions)) return;

        auto shouldResend = [&](const PositionEvaluation &evaluation)
        {
            if (!evaluation.resendPosition) return false;
            if (evaluation.positionToTryOnExpectedPath) return true;

            // Ensure the path gets us to the patch better than the worst case of letting the worker be
            auto normalPathCollisionDelay = expectedPatchCollisionDelay(positionMetadata.noResendCollisionRate);
            if (evaluation.expectedDelta > (9 + normalPathCollisionDelay)) return false;

            // If we can predict the worker's order process timer at normal arrival, check if it is better than the evaluated result
            double orderProcessTimerDelay = 4.5;
            int framesToNormalPathArrival = BWAPI::Broodwar->getLatencyFrames() + 10 - positionMetadata.probableDeltaToBenchmark();
            int orderProcessTimerAtArrival =
                    OrderProcessTimer::unitOrderProcessTimerAtDelta(workerStatus.worker->orderProcessTimer, framesToNormalPathArrival);
            if (orderProcessTimerAtArrival != -1)
            {
                // The order timer might reset between arrival and mining start, so adjust for this
                if (OrderProcessTimer::previousResetFrame(currentFrame + framesToNormalPathArrival + 1) > currentFrame)
                {
                    orderProcessTimerDelay = (double)(orderProcessTimerAtArrival + 4);
                }
                else
                {
                    orderProcessTimerDelay = (double)orderProcessTimerAtArrival;
                }
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

        auto evaluation = evaluatePosition(currentFrame, positionMetadata, workerStatus.resource);
        if (shouldResend(evaluation))
        {
            workerStatus.plannedResendPosition = std::move(evaluation.resendPosition);
            workerStatus.plannedSecondResendPosition = std::make_unique<GatherPositionObservationPtr>(evaluation.expectedPath.back());
            if ((*workerStatus.plannedResendPosition) == (*workerStatus.plannedSecondResendPosition))
            {
                workerStatus.plannedSecondResendPosition = nullptr;
            }

            workerStatus.expectedPath = std::move(evaluation.expectedPath);

#if OPTIMALPOSITIONS_DEBUG
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
                    out << " expected delta " << evaluation.expectedDelta;
                }

                CherryVis::log(workerStatus.worker->id) << out.str();
            }

            {
                std::ostringstream out;
                out << "Expected path:";
                int frame = currentFrame;
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
                    out << "\n" << frame << ": " << pos << "; " << dist;

                    frame++;
                }

                CherryVis::log(workerStatus.worker->id) << out.str();
            }
#endif
        }
    }

    void validatePlannedGatherPathSingle(WorkerGatherStatus &workerStatus,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (workerStatus.expectedPath.empty()) return; // have no further resends planned
        if (workerStatus.expectedPath.front().position() == *currentPosition) return; // path matches expectations

        // We need to clear second resend and expected path no matter what
        workerStatus.plannedSecondResendPosition = nullptr;
        workerStatus.expectedPath.clear();

        // If we haven't passed the first resend position yet, then just clear the planned data so we can replan
        if (!workerStatus.resentPosition())
        {
#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path; replanning";
#endif

            workerStatus.resendsPlanned = false;
            workerStatus.plannedResendPosition = nullptr;
            return;
        }

        // Guard against having sent multiple resends
        if (workerStatus.resentPositions.size() != 1)
        {
#if OPTIMALPOSITIONS_DEBUG
            Log::Get() << "ERROR: Worker has more than one resent positions while still tracking path"
                    << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
            return;
        }

        // We have sent the first resend, but hit a different path before reaching the second resend position
        auto &firstResend = *workerStatus.plannedResendPosition->pos;

        // If we haven't observed this path, leave the worker alone to get data about this new path
        if (!workerStatus.currentNode)
        {
#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path and unexplored path discovered; aborting second resend";
#endif
            return;
        }

        // We have observed this path, so we can replan the second resend position
        // First we need to figure out the delta between the first resend and the current position
        int deltaFromFirstResend = currentFrame - workerStatus.lastResendFrame();
        int firstResendCommandFrame = currentFrame - deltaFromFirstResend;

        // Evaluate second resends
        auto evaluation = evaluateSecondResendPositions(currentFrame,
                                                        firstResend,
                                                        *workerStatus.currentNode,
                                                        deltaFromFirstResend,
                                                        workerStatus.resource);

        // Use it if we want to explore
        if (evaluation.positionToTryOnExpectedPath)
        {
            workerStatus.plannedSecondResendPosition = std::make_unique<GatherPositionObservationPtr>(evaluation.expectedPath.back());
            workerStatus.expectedPath = std::move(evaluation.expectedPath);
            return;
        }

        // If we don't know anything about the path, and aren't exploring, leave the worker alone
        // TODO: Check if it is usually better to resend at the same delta as what we originally planned
        if (!evaluation.explored) return;

        // Evaluate no second resend
        auto expectedDelta = computeExpectedDelta(firstResendCommandFrame,
                                                  firstResend,
                                                  0,
                                                  firstResend.noSecondResendArrivalObservations);

        // Resend if the result is better than the no resend delta
        if (!expectedDelta.has_value() || evaluation.expectedDelta < (expectedDelta.value() + EPSILON))
        {
            workerStatus.plannedSecondResendPosition = std::make_unique<GatherPositionObservationPtr>(*evaluation.expectedPath.rbegin());
            workerStatus.expectedPath = std::move(evaluation.expectedPath);
        }
    }
}
