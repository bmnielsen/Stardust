// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the start of mining

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"
#include "Workers.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        bool handleTakeover(WorkerGatherStatus &workerStatus,
                            const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
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

#if TAKEOVER_DEBUG
                    CherryVis::log(worker->id) << "Worker passed unrecorded 10-distance position";
#endif
                }
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
            // State 2: are following a path planned using our stored optimization data
            // State 10: issued last command while the worker was at the patch
            // State 11: issued last command after 2 or more prior resends
            // State 12: issued last command after 1 prior resend
            // State 13: issued last command after no prior resends
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
                    {
                        // Make sure we aren't in a deadlock situation where both workers are waiting for the other
                        auto otherWorkerStatus = WorkerMiningOptimization::gatherStatusFor(otherWorker);
                        if (otherWorkerStatus && otherWorkerStatus->takeoverState == 1)
                        {
                            otherWorkerStatus->takeoverState = 0;
                            otherWorkerStatus->takeoverFrame = -1;
                            otherWorkerStatus->switchedPatches = true; // not actually true but a shortcut to tell our optimizer not to trust observations
#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Clearing other worker state to avoid deadlock";
#endif
                        }

                        // If the mining start frame of the other worker is not yet known, try to get it
                        // If we can't compute it yet, we just set the takeover frame a long time into the future
                        // This generally means that one or both workers were recently reassigned, so they both want to start mining at about
                        // the same time and there isn't anything to optimize in terms of takeover
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

                        // Look up path data for this position
                        auto positionMetadataIt = optimalPositions.find(*currentPosition);

                        // If there is a recorded position, we might be able to plan our full approach
                        if (positionMetadataIt != optimalPositions.end() && !positionMetadataIt->second.deltaToBenchmarkAndOccurrences.empty())
                        {
                            // Handle case where we haven't reached our observation horizon yet
                            if (positionMetadataIt->second.largestDeltaToBenchmark() < -GATHER_EXPLORE_BEFORE)
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

                                // Otherwise continue the approach waiting until we get closer
                                return true;
                            }

                            planGatherResendsDouble(workerStatus, optimalPositions, currentPosition);

                            if (workerStatus.resendsPlanned)
                            {
                                workerStatus.takeoverState = 2;
                                return true;
                            }
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
                                                               << " (increased by " << additionalSafeFrames
                                                               << " by known order process timer at arrival)"
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

                        // Wait until we have left the depot
                        if (worker->getDistance(workerStatus.depot) == 0) return true;

                        // Reference the observations we have for when we resend at this position
                        const std::unordered_map<int8_t, uint16_t> *observations = nullptr;
                        auto resentPosition = workerStatus.resentPosition();
                        if (resentPosition)
                        {
                            auto resentPositionDataIt = optimalPositions.find(*resentPosition);
                            if (resentPositionDataIt != optimalPositions.end())
                            {
                                auto secondResendObservations = resentPositionDataIt->second.secondResendObservationsFor(currentPosition.get());
                                if (secondResendObservations)
                                {
                                    observations = &secondResendObservations->arrivalObservations.arrivalDelayAndOccurrences;
                                }
                            }
                        }
                        else
                        {
                            auto optimalPositionDataIt = optimalPositions.find(*currentPosition);
                            if (optimalPositionDataIt != optimalPositions.end())
                            {
                                observations = &optimalPositionDataIt->second.noSecondResendArrivalObservations.arrivalDelayAndOccurrences;
                            }
                        }

                        // Resend if we either don't have any data or think it will succeed
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
                    case 2:
                    {
                        // Validate we are still following the expected path. If the result is that we no longer have a planned path, kick it
                        // back to state 1
                        validatePlannedGatherPathDouble(workerStatus, optimalPositions, currentPosition);
                        if (!workerStatus.resendsPlanned)
                        {
                            workerStatus.takeoverState = 1;
                            break; // execute state machine for state 1 immediately
                        }

                        // Resend commands if required to maintain mineral locking
                        // TODO

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
            handleGatherPatchSwitch(workerStatus);

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

#if !ENABLE_GATHER_OPTIMIZATION
        return;
#endif

        auto &optimalPositions = optimalGatherPositionsFor(resource);

        // Handle case where another worker is assigned to the patch
        if (handleTakeover(workerStatus, optimalPositions, currentPosition, resourceBwapiUnit)) return;

        // Validate planned resends; may clear resend if a path change has occurred
        if (workerStatus.resendsPlanned)
        {
            validatePlannedGatherPathSingle(workerStatus, optimalPositions, currentPosition);
        }

        // Plan potential resends
        if (!workerStatus.resendsPlanned)
        {
            planGatherResendsSingle(workerStatus, optimalPositions, currentPosition);
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