// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the start of mining

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"
#include "Workers.h"
#include "Units.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        void sendPlannedResend(WorkerGatherStatus &workerStatus,
                               BWAPI::Unit &resourceBwapiUnit,
                               const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (!workerStatus.resendsPlanned) return;

            auto handlePlannedResend = [&](
                    const std::unique_ptr<GatherPositionObservationPtr> &plannedPosition,
                    int resend)
            {
                if (!plannedPosition) return; // nothing planned for this position
                if (workerStatus.resentPositions.size() >= resend) return; // already resent
                if ((plannedPosition->position()) != (*currentPosition)) return; // not at the position yet

#if OPTIMALPOSITIONS_DEBUG || TAKEOVER_DEBUG
                CherryVis::log(workerStatus.worker->id) << "Resending for " << *plannedPosition;
#endif

                // Ignoring return value here as it should only fail in edge conditions while exploring
                workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
            };

            handlePlannedResend(workerStatus.plannedResendPosition, 1);
            handlePlannedResend(workerStatus.plannedSecondResendPosition, 2);

            // Remove this position from the expected path
            if (!workerStatus.expectedPath.empty()) workerStatus.expectedPath.pop_front();
        }

        bool handleTakeover(WorkerGatherStatus &workerStatus,
                            const std::shared_ptr<PositionAndVelocity> &currentPosition,
                            BWAPI::Unit resourceBwapiUnit)
        {
            auto &resource = workerStatus.resource;
            auto &worker = workerStatus.worker;
            auto distToPatch = resource->getDistance(worker);

            // Keep track of whether the worker has passed a 10-distance position
            if (workerStatus.passed10DistancePosition == -1)
            {
                auto &tenDistancePositions = tenDistancePositionsFor(resource);
                if (tenDistancePositions.contains(*currentPosition))
                {
                    workerStatus.passed10DistancePosition = currentFrame;

#if TAKEOVER_DEBUG
                    CherryVis::log(worker->id) << "Will reach 10-distance position in LF+1 from here " << *currentPosition;
#endif
                }
                else if (distToPatch <= 10)
                {
                    workerStatus.passed10DistancePosition = (currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1);
                    workerStatus.passedUnregistered10DistancePosition = true;

#if TAKEOVER_DEBUG
                    CherryVis::log(worker->id) << "Worker passed unrecorded 10-distance position";
#endif
                }
            }

            // Try to find another worker assigned to the patch
            auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
            if (!otherWorker)
            {
                if (workerStatus.takeoverState != 0)
                {
                    workerStatus.takeoverState = 0;
                    workerStatus.takeoverFrame = -1;
                    workerStatus.resendsPlanned = false;
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

                // Compute the frame of the order timer reset prior to the takeover frame
                int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(workerStatus.takeoverFrame - addedFrame);
                if (previousOrderTimerReset == (workerStatus.takeoverFrame - addedFrame)) previousOrderTimerReset -= 150;

                // If the order timer reset during mining, adjust our take over frame
                // We always assume the worst-case scenario (needing to wait a full cycle after the mining timer expires)
                // Because the order timer is at 6 when mining ends without a reset, we only have to wait two extra frames
                int worstCaseExcluded = 0;
                if (previousOrderTimerReset >= otherWorker->lastStartedMining)
                {
                    // Determine if the order process timer reset happened on a frame that excludes the worst-case result
                    // We can calculate this since we know the order process timer will never be 8 after a reset, so it will also never
                    // be 8 on any multiples of 9 frames into the future
                    int earliestMiningEndFrame = otherWorker->lastStartedMining + 75;
                    if (previousOrderTimerReset <= earliestMiningEndFrame)
                    {
                        if ((earliestMiningEndFrame - previousOrderTimerReset) % 9 == 0)
                        {
                            worstCaseExcluded = 1;
                        }
                    }

                    workerStatus.takeoverFrame =
                            std::max(otherWorker->lastStartedMining + 83 - worstCaseExcluded, previousOrderTimerReset + 8) + addedFrame;
                }

                // If the order process timer will reset after the command frame for optimal takeover, adjust the takeover frame so the command takes
                // effect on the reset frame instead
                if (previousOrderTimerReset > (workerStatus.takeoverFrame - 11))
                {
                    workerStatus.takeoverFrame = previousOrderTimerReset + 11;
                }

#if TAKEOVER_DEBUG
                CherryVis::log(worker->id)
                        << "Initializing takeover from " << otherWorker->id << ": "
                        << "otherStarted=" << otherWorker->lastStartedMining << "; "
                        << "takeOverFrame=" << workerStatus.takeoverFrame << "; "
                        << "previousOrderTimerReset=" << previousOrderTimerReset << "; "
                        << "addedFrame=" << addedFrame << "; "
                        << "worstCaseExcluded=" << worstCaseExcluded;
#endif
            };

            // Make sure we aren't in a deadlock situation where both workers are waiting for the other
            auto otherWorkerStatus = WorkerMiningOptimization::gatherStatusFor(otherWorker);
            if (workerStatus.takeoverState >= 1 && workerStatus.takeoverState <= 3
                && otherWorkerStatus && otherWorkerStatus->takeoverState >= 1 && otherWorkerStatus->takeoverState <= 3)
            {
                otherWorkerStatus->takeoverState = 0;
                otherWorkerStatus->takeoverFrame = -1;
                otherWorkerStatus->switchedPatches = true; // not actually true but a shortcut to tell our optimizer not to trust observations
                otherWorkerStatus->resendsPlanned = false;
#if TAKEOVER_DEBUG
                CherryVis::log(worker->id) << "Clearing other worker state to avoid deadlock";
#endif
            }

            // Run the state machine
            // State 0: haven't detected a worker to take over from yet
            // State 1: have initialized takeover and may be resending commands up to the takeover frame
            // State 2: are following a path planned using our stored optimization data
            // State 3: have followed a path planned using our stored optimization data and are now waiting at the patch
            // State 10: issued last command while the worker was at the patch
            // State 11: issued last command after 2 or more prior resends
            // State 12: issued last command after 1 prior resend
            // State 13: issued last command after no prior resends
            // State 14: issued last command through path optimization achieving mining at or after takeover frame
            // State 15: issued last command through path optimization achieving patch locking
            // State 20: worker was already at patch after takeover frame, so nothing further was required
            while (true)
            {
                switch (workerStatus.takeoverState)
                {
                    case 0:
                    {
                        // If the other worker is not mining and is not expected to reach the patch before us, use normal approach optimization
                        if (otherWorker->carryingResource) return false;
                        if ((otherWorker->lastStartedMining == -1 || (currentFrame - otherWorker->lastStartedMining) >= 100)
                            && resource->getDistance(otherWorker) >= resource->getDistance(worker))
                        {
                            return false;
                        }

                        workerStatus.takeoverState = 1;

                        // Intentionally fall through to next case
                    }
                    case 1:
                    case 3:
                    {
                        // If the mining start frame of the other worker is not yet known, try to get it
                        // If we can't compute it yet, we just set the takeover frame a long time into the future
                        // This generally means that one or both workers were recently reassigned, so they both want to start mining at about
                        // the same time and there isn't anything to optimize in terms of takeover
                        computeTakeoverFrame();
                        int takeoverFrame = workerStatus.takeoverFrame;
                        bool takeoverFrameUnknown = (takeoverFrame == -1);
                        if (takeoverFrameUnknown)
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

                        // Plan a full approach if we have path information
                        if (!workerStatus.resendsPlanned && workerStatus.takeoverState == 1 && !workerStatus.switchedPatches && !takeoverFrameUnknown
                            && workerStatus.resentFrames.empty() && workerStatus.currentNode && workerStatus.currentNode->pos)
                        {
                            auto &positionMetadata = *workerStatus.currentNode->pos;

                            // Handle case where we haven't reached our observation horizon yet
                            if (positionMetadata.largestDeltaToBenchmark() < -GATHER_EXPLORE_BEFORE)
                            {
                                // If the other worker is finished mining, transition back to doing normal approach optimization
                                if (otherWorker->carryingResource)
                                {
#if TAKEOVER_DEBUG
                                    CherryVis::log(worker->id) << "Reverting to single-worker case, as other worker has finished mining";
#endif

                                    workerStatus.takeoverState = 0;
                                    workerStatus.takeoverFrame = -1;
                                    return false;
                                }

                                // Wait until the worker is closer to the patch
                                return true;
                            }

#if ENABLE_PATH_BASED_TAKEOVER
                            planGatherResendsDouble(workerStatus, positionMetadata);

                            if (workerStatus.resendsPlanned && workerStatus.plannedResendPosition)
                            {
                                workerStatus.takeoverState = 2;
                                sendPlannedResend(workerStatus, resourceBwapiUnit, currentPosition);
                                return true;
                            }
#endif
                        }

                        // We get here in the following conditions:
                        // - We never captured a known path
                        // - We followed a path, but lost it again before completing all resends
                        // - We have finished following a path, but will reach it early enough that we need to avoid patch switching

                        // Compute the frame of the order timer reset prior to the take over frame
                        int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(takeoverFrame);
                        if (previousOrderTimerReset == takeoverFrame) previousOrderTimerReset -= 150;

                        // Now compute when we need to issue mining commands
                        // Besides issuing a mining command for the takeover frame, we also want to issue a command if the order timer resets
                        int commandFrameForTakeOver = takeoverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
                        int commandFrameForReset = previousOrderTimerReset - BWAPI::Broodwar->getLatencyFrames();

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

                        auto willResendHereArriveOnTime = [&]()
                        {
                            // Always will if we are at the patch
                            if (distToPatch == 0)
                            {
#if TAKEOVER_DEBUG
                                CherryVis::log(worker->id) << "Sending final command; worker is at patch";
#endif
                                return std::make_pair(true, 10);
                            }

                            // Always won't if we are still at the depot
                            if (worker->getDistance(workerStatus.depot) == 0)
                            {
                                return std::make_pair(false, 0);
                            }

                            // If we've already resent twice, we assume resending again will always get to the patch on time
                            if (workerStatus.resentPositions.size() > 1)
                            {
#if TAKEOVER_DEBUG
                                CherryVis::log(worker->id) << "Sending final command; worker has already resent twice (or more)";
#endif
                                return std::make_pair(true, 11);
                            }

                            // The worker is still approaching the patch, so use our recorded data to figure out if we
                            // will get to the patch on time if we resend here

                            // Reference any observations we have for when we resend at this position
                            GatherResendArrivalObservations *observations = nullptr;
                            if (workerStatus.currentNode)
                            {
                                observations = &workerStatus.currentNode->resendArrivalObservations();
                            }

                            // Determine if we think a resend will succeed
                            bool send;
                            if (!observations || observations->empty())
                            {
                                send = false;

                                // If we are tracking a no-resend path, use the delta to benchmark as a guide
                                if (workerStatus.currentNode && workerStatus.currentNode->pos)
                                {
                                    send = !workerStatus.currentNode->pos->deltaToBenchmarkAndOccurrenceRate.empty()
                                            && (workerStatus.currentNode->pos->largestDeltaToBenchmark() >= 0)
                                            && (workerStatus.currentNode->pos->secondResendPositions.empty());
#if TAKEOVER_DEBUG
                                    if (send)
                                    {
                                        CherryVis::log(worker->id) << "Sending final command; delta to benchmark indicates probable success";
                                    }
#endif
                                }
                                else if (isExploring())
                                {
                                    send = true;
#if TAKEOVER_DEBUG
                                    CherryVis::log(worker->id) << "Sending final command; exploring arrival from here";
#endif
                                }
                                else if (workerStatus.passed10DistancePosition != -1)
                                {
                                    send = true;
#if TAKEOVER_DEBUG
                                    CherryVis::log(worker->id) << "Sending final command; have passed 10-distance position";
#endif
                                }
                            }
                            else
                            {
                                send = true;

                                // Count success rate
                                // We use this position if it succeeds two-thirds of the time (170/255)
                                unsigned int successRate = 0;
                                for (const auto &[packedArrivalDelayAndFacingPatch, occurrenceRate]
                                        : observations->packedArrivalDelayAndFacingPatchToOccurrenceRate)
                                {
                                    if (GatherResendArrivalObservations::unpackArrivalDelay(packedArrivalDelayAndFacingPatch) <= 0 &&
                                        GatherResendArrivalObservations::unpackFacingPatch(packedArrivalDelayAndFacingPatch))
                                    {
                                        successRate += occurrenceRate;
                                    }
                                }

                                // If exploring, ensure we try until we have three failures
                                bool explore = false;
                                if (successRate < 170 && WorkerMiningOptimization::isExploring())
                                {
                                    unsigned int failures = 0;
                                    for (const auto &[packedDelayAndFacingPatch, occurrences]
                                            : observations->packedArrivalDelayAndFacingPatchToOccurrences)
                                    {
                                        if (GatherResendArrivalObservations::unpackArrivalDelay(packedDelayAndFacingPatch) > 0) failures += occurrences;
                                    }
                                    explore = (failures < 3);
                                }

                                if (successRate < 170 && !explore)
                                {
                                    send = false;
#if TAKEOVER_DEBUG
                                    CherryVis::log(worker->id) << std::fixed << std::setprecision(1)
                                                               << "Not sending here; success rate is " << ((double)successRate * 100.0 / 255.0) << "%";
                                }
                                else if (explore)
                                {
                                    CherryVis::log(worker->id) << "Sending final command; exploring";
                                }
                                else
                                {
                                    CherryVis::log(worker->id) << std::fixed << std::setprecision(1)
                                                               << "Sending final command; success rate is "
                                                               << ((double)successRate * 100.0 / 255.0) << "%";
#endif
                                }
                            }

                            return std::make_pair(send, (workerStatus.resentPositions.size() > 1) ? 12 : 13);
                        };

                        // Logic for when the next command is in the future
                        if (framesToNextCommand > 0)
                        {
                            // Calculate if we think we can patch lock
                            // We don't do this if we have planned an exploratory path
                            if (!workerStatus.exploring &&
                                !workerStatus.resentFrames.contains(currentFrame) &&
                                !workerStatus.resentFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames()) &&
                                willResendHereArriveOnTime().first)
                            {
                                auto patchLock = checkForPatchLock(workerStatus, currentFrame);
                                if (patchLock.has_value())
                                {
                                    workerStatus.expectedPatchLockFrame = patchLock.value();
                                    workerStatus.takeoverState = 15;
                                    workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                                    return true;
                                }
                            }

                            // We need to resend gather commands once we get close to the patch to avoid the worker switching patches

                            // No need to do anything if the worker isn't close yet
                            if (workerStatus.passed10DistancePosition == -1) return true;

                            // Handle the initial send
                            // The important thing here is to resend as soon as possible but without blocking later resends with Unit_Busy
                            auto hasDoneInitialSend = [&]()
                            {
                                if (workerStatus.resentPositions.empty()) return false;
                                if (workerStatus.takeoverState == 1) return true;
                                return workerStatus.resentPositions.size() > workerStatus.plannedResendCount();
                            };
                            if (!hasDoneInitialSend())
                            {
                                // Compute the last frame we can resend on without potentially having the worker switch patches
                                auto computeLastSafeResendFrame = [&]()
                                {
                                    // If we've done a planned approach to the patch that already sent at least one resend, aim to resend before the
                                    // order process timer hits 0 again
                                    if (!workerStatus.resentFrames.empty())
                                    {
#if TAKEOVER_DEBUG
                                        CherryVis::log(worker->id) << "Last safe resend frame: " << (*workerStatus.resentFrames.rbegin() + 10);
#endif
                                        return *workerStatus.resentFrames.rbegin() + 10;
                                    }

                                    // By default this is given by when we detected the worker passing the appropriate position, but if we know the
                                    // worker's order process timer value we can be more specific
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
                                                                   << " (increased by " << additionalSafeFrames
                                                                   << " by known order process timer at arrival)"
                                                                   << " Predicted order process timer value here is " << worker->orderProcessTimer;
                                    }
                                    else
                                    {
                                        CherryVis::log(worker->id) << "Last safe resend frame: " << lastSafeResendFrame;
#endif
                                    }

                                    return lastSafeResendFrame;
                                };

                                int framesToLastSafeResendFrame = computeLastSafeResendFrame() - currentFrame;

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
                                                                   << " to avoid conflicts with future resends";
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
                                // Check if the current value is already not blocked by a previous resend
                                if (!workerStatus.resentFrames.contains(currentFrame + framesToNextResend - BWAPI::Broodwar->getLatencyFrames()))
                                {
                                    return true;
                                }

                                // Can't send in the past
                                if ((framesToNextResend + delta) < 0) return false;

                                // Adjusted frame is also blocked by a previous resend
                                if (workerStatus.resentFrames.contains(
                                        currentFrame + framesToNextResend + delta - BWAPI::Broodwar->getLatencyFrames()))
                                {
                                    return false;
                                }

                                // Adjusted frame would block next planned resend
                                if ((framesToNextCommand - framesToNextResend + delta) == BWAPI::Broodwar->getLatencyFrames()) return false;

                                // Adjusted frame would block takeover command
                                if ((framesToTakeoverCommand - framesToNextResend + delta) == BWAPI::Broodwar->getLatencyFrames()) return false;

                                // Found a value that works, so adjust it
                                framesToNextResend += delta;
#if TAKEOVER_DEBUG
                                CherryVis::log(worker->id) << "Shifted resend by " << delta << " to avoid conflicts";
#endif
                                return true;
                            };
                            realign(-1) || realign(1) || realign(-2) || realign(2) || realign(-3) || realign(3);

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

                        auto sendHere = willResendHereArriveOnTime();
                        if (sendHere.first && workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition))
                        {
                            workerStatus.takeoverState = sendHere.second;
                        }

                        return true;
                    }
                    case 2:
                    {
                        // Validate we are still following the expected path
                        if (!validatePlannedGatherPathDouble(workerStatus, currentPosition))
                        {
                            int newState = (workerStatus.resentPosition() ? 3 : 1);
#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Switching to state " << newState << " as planned resends could not be executed";
#endif

                            workerStatus.takeoverState = newState;
                            break; // execute state machine for state 1/3 immediately
                        }

                        sendPlannedResend(workerStatus, resourceBwapiUnit, currentPosition);

                        // Logic for when all planned resends have been sent
                        if (workerStatus.resentPositions.size() == workerStatus.plannedResendCount())
                        {
                            // If the last resend gets us patch locking, transition to a final state
                            if (workerStatus.expectedPatchLockFrame != -1)
                            {
                                workerStatus.takeoverState = 15;
                                return true;
                            }

                            // If the last resend gets us to the takeover frame (or beyond), transition to a final state
                            if (workerStatus.lastResendFrame() >= (workerStatus.takeoverFrame - 11 - BWAPI::Broodwar->getLatencyFrames()))
                            {
                                workerStatus.takeoverState = 14;
                                return true;
                            }

                            // If we are finished following the planned path, switch states for mineral locking
                            // TODO: Bug here when exploring and don't have an expected arrival frame
                            if ((workerStatus.expectedArrivalFrameAndOccurrenceRate.empty() && workerStatus.lastResendFrame() <= (currentFrame - 11))
                                || (!workerStatus.expectedArrivalFrameAndOccurrenceRate.empty() &&
                                    workerStatus.mostProbableArrivalFrame() <= (currentFrame + BWAPI::Broodwar->getLatencyFrames())))
                            {
#if TAKEOVER_DEBUG
                                if (workerStatus.expectedArrivalFrameAndOccurrenceRate.empty())
                                {
                                    CherryVis::log(worker->id) << "Last resend takes effect in LF, switching to state 3 for mineral locking";
                                }
                                else
                                {
                                    CherryVis::log(worker->id) << "Expected to arrive at patch in LF, switching to state 3 for mineral locking";
                                }
#endif
                                workerStatus.takeoverState = 3;

                                // If a command was sent this frame, return, otherwise process state 3 immediately
                                if (workerStatus.lastResendFrame() == currentFrame)
                                {
                                    return true;
                                }
                                break;
                            }
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
    }

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        // Don't touch the worker if it is transitioning to mine
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            // Exception is if another worker is currently mining the patch; we will mark as patch locked later if the resource still matches
            auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
            if (!otherWorker || otherWorker->bwapiUnit->getOrder() != BWAPI::Orders::MiningMinerals)
            {
                return;
            }
        }

        auto &workerStatus = gatherStatusFor(worker, depot, resource);

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            workerStatus.reset();
            return;
        }

        if (workerStatus.ignoreThisPosition()) return;

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
            // This runs before we append the position we are at now, since it may be changed by the patch switch
            if (!workerStatus.switchedPatches) handleGatherPatchSwitch(workerStatus);

#if LOGGING_ENABLED
            CherryVis::log(worker->id) << "targeting different patch; resending order";
            if (workerStatus.passed10DistancePosition != -1 && !workerStatus.passedUnregistered10DistancePosition && !isExploring())
            {
                std::ostringstream dbg;
                dbg << "WARNING: patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition()
                    << " patch switch";

                auto otherPatch = Units::resourceAt(worker->bwapiUnit->getOrderTarget()->getTilePosition());
                if (otherPatch)
                {
                    dbg << "; new patch @ " << otherPatch->tile;
                    if (resource->resourcesInSwitchPatchRange.contains(otherPatch))
                    {
                        dbg << "; in switch patch range";
                    }
                    else
                    {
                        dbg << "; OUTSIDE SWITCH PATCH RANGE";
                    }
                }

                dbg << "; passed10DistancePosition: " << workerStatus.passed10DistancePosition;
                Log::Get() << dbg.str();
            }
#endif

            auto currentPosition = workerStatus.appendCurrentPosition();

            // There could be a Unit_Busy failure here, but we will pick up next frame that the command hasn't been issued
            workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);

            // Set a fake 10 distance position frame if none is set
            if (workerStatus.passed10DistancePosition == -1)
            {
                workerStatus.passed10DistancePosition = currentFrame - BWAPI::Broodwar->getLatencyFrames();
            }

            workerStatus.switchedPatches = true;
            workerStatus.currentNode = nullptr;
            return;
        }

        // No need to do anything more if we are patch locked
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            workerStatus.lastProcessedFrame = currentFrame;
            if (workerStatus.actualPatchLockFrame == -1)
            {
                workerStatus.actualPatchLockFrame = currentFrame;
#if TAKEOVER_DEBUG
                // TODO: Log specific patch lock situations we want to test, like patch lock at reset frame, takeover frame, etc.
//                Log::Get() << "Patch lock; " << worker->id << " @ " << worker->getTilePosition();
                CherryVis::log(worker->id) << "Patch locked";
#endif
            }
            return;
        }

        // Track the worker's visited positions
        auto currentPosition = workerStatus.appendCurrentPosition();

#if !ENABLE_GATHER_OPTIMIZATION
        return;
#endif

        // If we are following a path, try to advance the current path node
        if (workerStatus.currentNode)
        {
            workerStatus.currentNode = workerStatus.currentNode->nextPositionIfExists(*currentPosition, workerStatus.resentPosition());
            if (!workerStatus.currentNode)
            {
#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Lost path at " << *currentPosition;
#endif
            }
        }

        // If we don't have a current node, check if this position is a root node
        if (!workerStatus.currentNode && !workerStatus.resendsPlanned)
        {
            auto rootNode = findGatherPositionObservations(resource, *currentPosition, false, &workerStatus.rootNode);
            if (rootNode)
            {
                workerStatus.currentNode = std::make_unique<GatherPositionObservationPtr>(rootNode);
#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Picked up path at " << *currentPosition;
#endif
            }
        }

        // Handle case where another worker is assigned to the patch
        if (handleTakeover(workerStatus, currentPosition, resourceBwapiUnit)) return;

        // Validate planned resends; may clear resend if a path change has occurred
        if (workerStatus.resendsPlanned)
        {
            validatePlannedGatherPathSingle(workerStatus, currentPosition);
        }

        // Plan potential resends
        if (!workerStatus.resendsPlanned)
        {
            planGatherResendsSingle(workerStatus);
        }

        // Send a command we have pre-planned
        sendPlannedResend(workerStatus, resourceBwapiUnit, currentPosition);
    }
}