// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the start of mining

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"
#include "Workers.h"

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
            if (positionMetadata.deltaToNormalPathOptimalPosition == 100) return 100.0;

            double expectedMiningDelay = observations.expectedMiningDelay(false, commandFrame);
            auto collisionDelay = expectedPatchCollisionDelay(observations.collisions, observations.nonCollisions);
            return positionMetadata.deltaToNormalPathOptimalPosition + deltaToFirstResend + expectedMiningDelay + collisionDelay;
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
                        Log::Get() << "ERROR: No second resend metadata found for next position " << nextPosition;
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
            if (observations.empty()
                && positionMetadata.deltaToNormalPathOptimalPosition >= -EXPLORE_BEFORE
                && positionMetadata.deltaToNormalPathOptimalPosition <= -EXPLORE_AFTER
                && (WorkerMiningOptimization::isExploring() || (positionMetadata.deltaToNormalPathOptimalPosition == 0 && deltaToFirstResend == 0)))
            {
                int positionToTryDelta = std::abs(positionMetadata.deltaToNormalPathOptimalPosition + deltaToFirstResend);
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

        void validatePlannedPath(WorkerGatherStatus &workerStatus,
                                 BWAPI::Unit resourceBwapiUnit,
                                 const std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &optimalPositions,
                                 const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (!workerStatus.resendsPlanned) return; // haven't planned yet
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
                Log::Get() << "ERROR: Didn't find resend position metadata: " << *resentPosition;
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
                    Log::Get() << "ERROR: Didn't find resend position in positions history: " << *resentPosition;
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
                    Log::Get() << "ERROR: Didn't find resend position in positions history: " << *resentPosition;
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

        bool handleTakeover(WorkerGatherStatus &workerStatus,
                            const std::shared_ptr<PositionAndVelocity> &currentPosition,
                            BWAPI::Unit resourceBwapiUnit)
        {
            auto &resource = workerStatus.resource;
            auto &worker = workerStatus.worker;
            auto distToPatch = resource->getDistance(worker);

            // Keep track of whether the worker has passed a 10-distance position
            auto &tenDistancePositions = tenDistancePositionsFor(resource);
            if (workerStatus.passed10DistancePosition == -1 && tenDistancePositions.contains(*currentPosition))
            {
                workerStatus.passed10DistancePosition = currentFrame;

#if TAKEOVER_DEBUG
                CherryVis::log(worker->id) << "Will reach 10-distance position in LF+1 from here " << *currentPosition;
#endif
            }
            if (workerStatus.passed10DistancePosition == -1 && distToPatch == 0)
            {
                workerStatus.passed10DistancePosition = (currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1);

#if TAKEOVER_DEBUG
                CherryVis::log(worker->id) << "Worker arrived at patch without passing recorded 10-distance position";
#endif
            }

            // Try to find another worker assigned to the patch
            auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
            if (!otherWorker || !otherWorker->exists())
            {
                if (workerStatus.takeoverState != 0)
                {
                    workerStatus.takeoverState = 0;
                    workerStatus.takeoverFrame = -1;
#if TAKEOVER_DEBUG
                    CherryVis::log(worker->id)
                            << "Clearing takeover from other worker no longer assigned to this patch";
#endif
                }

                return false;
            }

            auto computeTakeoverFrame = [&workerStatus, &worker, &otherWorker]()
            {
                // Nothing needed if we've already done it
                if (workerStatus.takeoverFrame != -1) return;

                // Can't compute the frame yet if the other worker hasn't started mining
                if (otherWorker->lastStartedMining == -1 || (currentFrame - otherWorker->lastStartedMining) >= 100) return;

                // We need to add an extra frame if the worker taking over might have its orders processed first
                int addedFrame = 1;
                if (otherWorker->orderProcessIndex > worker->orderProcessIndex)
                {
                    addedFrame = 0;
                }

                // Without order timer resets, we can compute the exact takeover frame
                workerStatus.takeoverFrame = otherWorker->lastStartedMining + 81 + addedFrame;

                // Compute the frame of the order timer reset prior to the take over frame
                int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(workerStatus.takeoverFrame);
                if (previousOrderTimerReset == workerStatus.takeoverFrame) previousOrderTimerReset -= 150;

                // If the order timer reset during mining, adjust our take over frame
                // We always assume the worst-case scenario (needing to wait a full cycle after the mining timer expires)
                // Because the order timer is at 6 when mining ends without a reset, we only have to wait two extra frames
                if (previousOrderTimerReset >= otherWorker->lastStartedMining)
                {
                    workerStatus.takeoverFrame = std::max(otherWorker->lastStartedMining + 83, previousOrderTimerReset + 8) + addedFrame;
                }

#if TAKEOVER_DEBUG
                CherryVis::log(worker->id)
                        << "Initializing takeover from " << otherWorker->id << ": "
                        << "otherStarted=" << otherWorker->lastStartedMining << "; "
                        << "takeOverFrame=" << workerStatus.takeoverFrame << "; "
                        << "previousOrderTimerReset=" << previousOrderTimerReset << "; "
                        << "addedFrame=" << addedFrame;
#endif
            };

            // Run the state machine
            // State 0: haven't detected a worker to take over from yet
            // State 1: have initialized takeover and may be resending commands up to the takeover frame
            // State 2: are following a planned path using our normal approach optimization
            // State 10: issued last command while the worker was at the patch
            // State 11: issued last command after 2 or more prior resends
            // State 12: issued last command after 1 prior resend
            // State 13: issued last command after no prior resends
            // State 20: worker was already at patch after takeover frame, so nothing further was required
            switch (workerStatus.takeoverState)
            {
                case 0:
                {
                    // If the other worker is not mining and is not expected to reach the patch before us, use normal approach optimization
                    if ((otherWorker->lastStartedMining == -1 || (currentFrame - otherWorker->lastStartedMining) >= 100)
                            && resource->getDistance(otherWorker) >= resource->getDistance(worker))
                    {
                        return false;
                    }

                    workerStatus.takeoverState = 1;

                    // Intentionally fall through to next case
                }
                case 1:
                {
                    // Make sure we aren't in a deadlock situation where both workers are waiting for the other
                    auto otherWorkerStatus = WorkerMiningOptimization::gatherStatusFor(otherWorker);
                    if (otherWorkerStatus && otherWorkerStatus->takeoverState == 1)
                    {
                        otherWorkerStatus->takeoverState = 0;
                        otherWorkerStatus->takeoverFrame = -1;
                        otherWorkerStatus->switchedPatches = true; // not actually true but a good way to tell our optimizer not to trust observations
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Clearing other worker state to avoid deadlock";
#endif
                    }

                    // If the mining start frame of the other worker is not yet known, try to get it
                    computeTakeoverFrame();
                    int takeoverFrame = workerStatus.takeoverFrame;
                    if (takeoverFrame == -1)
                    {
                        takeoverFrame = currentFrame + 80 - (currentFrame % 7);
                    }

                    // If the worker is at the patch and the takeover frame has passed, don't touch it
                    // We normally won't reach this but it might happen if workers are reassigned
                    if (distToPatch == 0 && currentFrame >= takeoverFrame)
                    {
                        workerStatus.takeoverState = 20;
                        return true;
                    }

                    // Compute the frame of the order timer reset prior to the take over frame
                    int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(takeoverFrame);
                    if (previousOrderTimerReset == takeoverFrame) previousOrderTimerReset -= 150;

                    // Now compute when we need to issue mining commands
                    // Besides issuing a mining command for the takeover frame, we also want to issue a command if the order timer resets
                    int commandFrameForTakeOver = takeoverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
                    int commandFrameForReset = previousOrderTimerReset - BWAPI::Broodwar->getLatencyFrames();

                    // If the takeover frame comes first, delay sending the order so it takes effect when the order timer resets instead
                    // This is to avoid situations where the second worker's command takes effect too soon,
                    // causing it to switch to a different patch
                    if (commandFrameForReset > commandFrameForTakeOver)
                    {
                        commandFrameForTakeOver = commandFrameForReset;
                    }

                    // If the takeover and reset command frames are LF apart, we can't send both of them
                    // Time it instead to send the reset command one frame late - this may cause the worker to switch patches for one frame,
                    // but this isn't long enough to be a problem
                    if ((commandFrameForTakeOver - commandFrameForReset) == BWAPI::Broodwar->getLatencyFrames())
                    {
                        commandFrameForReset++;
                    }

                    int framesToTakeoverCommand = commandFrameForTakeOver - currentFrame;

                    // Compute the number of frames until the next command we have to send
                    // We ignore order process timer resets if we are far enough away from the patch that it will not affect us
                    int framesToNextCommand = framesToTakeoverCommand;
                    if (currentFrame <= commandFrameForReset && commandFrameForTakeOver > commandFrameForReset
                        && workerStatus.passed10DistancePosition != -1)
                    {
                        framesToNextCommand = commandFrameForReset - currentFrame;
                        if (framesToNextCommand == 0)
                        {
#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Resending to ensure mineral locking (reset frame)";
#endif
                            workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                            return true;
                        }
                    }

                    // Logic for when the next command is in the future
                    if (framesToNextCommand > 0)
                    {
                        // We need to resend gather commands once we get close to the patch to avoid the worker switching patches

                        // No need to do anything if the worker isn't close yet
                        if (workerStatus.passed10DistancePosition == -1) return true;

                        // Handle the initial send
                        // The important thing here is to resend as soon as possible but without blocking later resends with Unit_Busy
                        if (workerStatus.resentPositions.empty())
                        {
                            // Compute the last frame we can resend on without potentially having the worker switch patches
                            // By default this is given by when we detected the worker passing the appropriate position, but if we know the worker's
                            // order process timer value we can be more specific
                            int lastSafeResendFrame = workerStatus.passed10DistancePosition;
                            int atRangeFrame = lastSafeResendFrame + BWAPI::Broodwar->getLatencyFrames();
                            int additionalSafeFrames = std::min(
                                    OrderProcessTimer::unitOrderProcessTimerAtDelta(worker->orderProcessTimer, atRangeFrame - currentFrame),
                                    OrderProcessTimer::nextResetFrame(atRangeFrame));
                            if (additionalSafeFrames != -1)
                            {
                                lastSafeResendFrame += additionalSafeFrames;
#if TAKEOVER_DEBUG
                                CherryVis::log(worker->id) << "Last safe resend frame: " << lastSafeResendFrame
                                    << " (increased by " << additionalSafeFrames << " by known order process timer at arrival)"
                                    << " Predicted order process timer value here is " << worker->orderProcessTimer;
                            }
                            else
                            {
                                CherryVis::log(worker->id) << "Last safe resend frame: " << lastSafeResendFrame;
#endif
                            }

                            int framesToLastSafeResendFrame = lastSafeResendFrame - currentFrame;

                            // Don't need to do anything if the next normal resend frame comes first
                            if (framesToNextCommand <= framesToLastSafeResendFrame)
                            {
#if TAKEOVER_DEBUG
                                CherryVis::log(worker->id) << "Don't need to resend as next normal resend frame comes first";
#endif
                                return true;
                            }

                            // Might be in the past if we didn't have observations or couldn't send a command earlier
                            if (framesToLastSafeResendFrame < 0) framesToLastSafeResendFrame = 0;

                            // Resolve conflicts with the next and last commands: we can't send a command LF frames before either
                            auto resolveConflict = [&](int delta)
                            {
                                int f = framesToLastSafeResendFrame + delta;
                                if (f < 0) return false;
                                if ((framesToNextCommand - f) == BWAPI::Broodwar->getLatencyFrames()) return false;
                                if ((framesToTakeoverCommand - f) == BWAPI::Broodwar->getLatencyFrames()) return false;
                                framesToLastSafeResendFrame = f;

#if TAKEOVER_DEBUG
                                if (delta != 0)
                                {
                                    CherryVis::log(worker->id) << "Adjusting initial mineral locking resend frame by " << delta
                                                               << "to avoid conflicts with future resends";
                                }
#endif
                                return true;
                            };
                            resolveConflict(0) || resolveConflict(-1) || resolveConflict(-2) || resolveConflict(1) || resolveConflict(2);

                            // Wait if the result is in the future
                            if (framesToLastSafeResendFrame > 0) return true;

#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Initial resend to ensure mineral locking";
#endif
                            workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                            return true;
                        }

                        // We resend every 7 frames, since this is within the order process timer cycle and doesn't conflict with LF3-LF6
                        int framesToNextResend = framesToNextCommand % 7;

                        // The planned resend may be blocked by a previous one
                        // If this occurs, find a nearby frame where we can resend
                        auto realign = [&](int delta)
                        {
                            if (!workerStatus.resentFrames.contains(currentFrame + framesToNextResend - BWAPI::Broodwar->getLatencyFrames())) return;

                            if ((framesToNextResend + delta) < 0) return;
                            if (workerStatus.resentFrames.contains(currentFrame + framesToNextResend + delta - BWAPI::Broodwar->getLatencyFrames()))
                            {
                                return;
                            }
                            if ((framesToNextCommand - framesToNextResend + delta) == BWAPI::Broodwar->getLatencyFrames()) return;
                            if ((framesToTakeoverCommand - framesToNextResend + delta) == BWAPI::Broodwar->getLatencyFrames()) return;

                            framesToNextResend += delta;
#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Shifted resend by " << delta << " to avoid conflicts";
#endif
                        };
                        realign(-1);
                        realign(1);
                        realign(-2);
                        realign(2);
                        realign(-3);
                        realign(3);

                        if (framesToNextResend == 0)
                        {
#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Resending to ensure mineral locking";
#endif

                            workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        }

                        return true;
                    }

                    // Bail out if we can't send a command here
                    if (workerStatus.resentFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames()))
                    {
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Skipping this frame as we can't send a command here";
#endif

                        return true;
                    }

                    // If the worker is at the patch, just send the command
                    if (distToPatch == 0)
                    {
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Sending final command; worker is at patch";
#endif

                        workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        workerStatus.takeoverState = 10;
                        return true;
                    }

                    // The worker is still approaching the patch, so use our recorded data to figure out if we
                    // will get to the patch on time if we resend here

                    // If we've already resent twice, we assume resending again will always get to the patch on time
                    if (workerStatus.resentPositions.size() > 1)
                    {
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Sending final command; worker has already resent twice (or more)";
#endif

                        workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        workerStatus.takeoverState = 11;
                        return true;
                    }

                    // Reference the observations we have for when we resend at this position
                    auto &optimalPositions = optimalGatherPositionsFor(resource);
                    const std::map<int, int>* observations = nullptr;
                    auto resentPosition = workerStatus.resentPosition();
                    if (resentPosition)
                    {
                        auto resentPositionDataIt = optimalPositions.find(*resentPosition);
                        if (resentPositionDataIt != optimalPositions.end())
                        {
                            auto secondResendMetadata = resentPositionDataIt->second.secondResendMetadataFor(currentPosition.get());
                            if (secondResendMetadata)
                            {
                                observations = &secondResendMetadata->observations.arrivalDelayAndOccurrences;
                            }
                        }
                    }
                    else
                    {
                        auto optimalPositionDataIt = optimalPositions.find(*currentPosition);
                        if (optimalPositionDataIt != optimalPositions.end())
                        {
                            observations = &optimalPositionDataIt->second.noSecondResendObservations.arrivalDelayAndOccurrences;
                        }
                    }

                    // Resend if we either don't have any data or only see success from this position
                    bool send = true;
                    if (!observations || observations->empty())
                    {
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Sending final command; no observations for this position";
#endif
                    }
                    else
                    {
                        // Count number of successes and failures
                        int successes = 0;
                        int failures = 0;
                        for (const auto &[delay, occurrences] : *observations)
                        {
                            ((delay > 0) ? failures : successes)++;
                        }

                        // Our current heuristic is to take any positions that succeed more than twice as often as they fail
                        // While exploring we wait until we have experienced at least three failures
                        if (successes < (failures * 2) && (!WorkerMiningOptimization::isExploring() || failures >= 3))
                        {
                            send = false;
#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Not sending here; successes " << successes << " vs. failures " << failures;
                        }
                        else
                        {
                            CherryVis::log(worker->id) << "Sending final command; successes " << successes << " vs. failures " << failures;
#endif
                        }
                    }

                    if (send)
                    {
                        workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        workerStatus.takeoverState = (resentPosition ? 12 : 13);
                    }

                    return true;
                }
                default:
                {
                    // Handles all final states where we have sent the last command needed
                    return true;
                }
            }
        }
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        // Don't touch the worker if it is transitioning to mine
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) return;

        auto &workerStatus = gatherStatusFor(worker, depot, resource);

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            workerStatus.reset();
            return;
        }

        // Track the worker's visited positions
        auto currentPosition = workerStatus.appendCurrentPosition();

        // Resend the gather command if it has been scheduled for this frame
        if (workerStatus.resentOnSchedule())
        {
            if (workerStatus.resendCommandOnFrame == currentFrame)
            {
                workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending gather command on schedule";
#endif
            }
            return;
        }

        // Our logic ensures mineral locking automatically except in some specific cases:
        // - worker has been released from combat, which can leave it with a gather order to a random patch used for kiting
        // - workers have been avoiding a no-go area and returning to mining as a group, so the timing gets messed up
        // - both workers reach the patch at approximately the same time after one or both are (re)assigned
        // - we don't have enough observed resend positions and get unlucky on the order timer
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            // Hook to update our observations based on this potential failure of mineral locking
            handleStartOfMiningPatchSwitch(workerStatus);

            CherryVis::log(worker->id) << "targeting different patch; resending order";
            Log::Get() << "ERROR: patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition() << " switched patch"
                << "; passed10DistancePosition: " << workerStatus.passed10DistancePosition;

            workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
            if (workerStatus.passed10DistancePosition == -1)
            {
                workerStatus.passed10DistancePosition = currentFrame - BWAPI::Broodwar->getLatencyFrames();
            }
            workerStatus.switchedPatches = true;
            return;
        }

        // Handle case where another worker is assigned to the patch
        if (handleTakeover(workerStatus, currentPosition, resourceBwapiUnit)) return;

        auto &optimalPositions = optimalGatherPositionsFor(resource);

        // If we have a path planned, validate that we are following it
        validatePlannedPath(workerStatus, resourceBwapiUnit, optimalPositions, currentPosition);

        // Plan resends if we haven't done so already
        if (!workerStatus.resendsPlanned)
        {
            auto metadataIt = optimalPositions.find(*currentPosition);
            if (metadataIt == optimalPositions.end()) return; // haven't reached an observed position yet
            if (metadataIt->second.deltaToNormalPathOptimalPosition == 100) return; // haven't observed this position's normal path yet

            workerStatus.resendsPlanned = true;

            auto shouldResend = [&](const PositionEvaluation &evaluation)
            {
                if (!evaluation.resendPosition) return false;
                if (evaluation.positionToTryOnExpectedPath) return true;

                // Ensure the path gets us to the patch better than the worst case of letting the worker be
                auto normalPathCollisionDelay = expectedPatchCollisionDelay(metadataIt->second.noResendCollisions,
                                                                            metadataIt->second.noResendNonCollisions);
                if (evaluation.expectedDelta > (9 + normalPathCollisionDelay)) return false;

                // If we can predict the worker's order process timer at normal arrival, check if it is better than the evaluated result
                double orderProcessTimerDelay = 4.5;
                int framesToNormalPathArrival = BWAPI::Broodwar->getLatencyFrames() + 10 - metadataIt->second.deltaToNormalPathOptimalPosition;
                int orderProcessTimerAtArrival =
                        OrderProcessTimer::unitOrderProcessTimerAtDelta(worker->orderProcessTimer, framesToNormalPathArrival);
                if (orderProcessTimerAtArrival != -1)
                {
                    orderProcessTimerDelay = (double)orderProcessTimerAtArrival;
                }

                if ((normalPathCollisionDelay + orderProcessTimerDelay) < evaluation.expectedDelta)
                {
#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << std::fixed << std::setprecision(1)
                            << "Not resending as expected order timer delay " << orderProcessTimerDelay
                            << " and collision delay " << normalPathCollisionDelay
                            << " is better than expected delta " << evaluation.expectedDelta;
#endif

                    return false;
                }

                return true;
            };

            auto evaluation = evaluatePosition(BWAPI::Broodwar->getFrameCount(), optimalPositions, metadataIt->second);
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

                CherryVis::log(worker->id) << out.str();
#endif
            }
        }

        // Send commands we have pre-planned
        if (workerStatus.resendsPlanned)
        {
            auto handlePlannedResend = [&](
                    const std::shared_ptr<const PositionAndVelocity> &plannedPosition,
                    int resend)
            {
                if (!plannedPosition) return; // nothing planned for this position
                if (workerStatus.resentPositions.size() >= resend) return; // already resent
                if ((*plannedPosition) != (*currentPosition)) return; // not at the position yet

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending for " << *plannedPosition;
#endif

                workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
            };

            handlePlannedResend(workerStatus.plannedResendPosition, 1);
            handlePlannedResend(workerStatus.plannedSecondResendPosition, 2);

            // Remove this position from the expected path
            if (!workerStatus.expectedPath.empty()) workerStatus.expectedPath.pop_front();
        }
    }
}