// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the start of mining

#include "WorkerMiningOptimization.h"

#include "Workers.h"
#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        unsigned int expectedGatherDelay(const PositionObservationMetadata &metadata)
        {
            // Gather the aggregated delays across all nonoptimal resends
            unsigned int count = metadata.successes;
            unsigned int delays = 0;
            for (const auto &[_, delayAndMetadata] : metadata.failurePositionMetadata)
            {
                for (const auto &[delay, secondMetadata] : delayAndMetadata)
                {
                    count += secondMetadata.successes + secondMetadata.failures;

                    // If the delay is exactly LF after, the unit will be busy and this will incur an extra frame of delay
                    int effectiveDelay = delay + ((delay == BWAPI::Broodwar->getFrameCount()) ? 1 : 0);
                    delays += secondMetadata.successes * effectiveDelay + secondMetadata.failures * (effectiveDelay + 5);
                }
            }

            if (count == 0) return 0;

            // Integer division with ceiling
            return (delays + count - 1) / count;
        }

        bool shouldResendGatherCommand(const MyWorker &worker,
                                       const PositionObservationMetadata &positionMetadata,
                                       unsigned int &expectedDelay)
        {
            expectedDelay = expectedGatherDelay(positionMetadata);
            if (expectedDelay == 0) return true;

            // If we can predict the order timer value at arrival, check if it is better or worse than the observed results on this patch
            if (worker->orderProcessTimer != -1)
            {
                int orderProcessTimerAtArrival = worker->orderProcessTimer - BWAPI::Broodwar->getLatencyFrames() - 11 + 1;
                while (orderProcessTimerAtArrival < 0)
                {
                    orderProcessTimerAtArrival += 9;
                }

#if OPTIMALPOSITIONS_DEBUG
                if (expectedDelay >= orderProcessTimerAtArrival)
                {
                    CherryVis::log(worker->id) << "Not resending at " << positionMetadata << " as expected delay " << expectedDelay
                                               << " is no better than expected order process timer at arrival " << orderProcessTimerAtArrival;
                }
#endif

                return expectedDelay < orderProcessTimerAtArrival;
            }

            // The order timer will be randomized at arrival, so resend if the metadata indicates we on average would benefit
#if OPTIMALPOSITIONS_DEBUG
            if (expectedDelay >= 5)
            {
                CherryVis::log(worker->id) << "Not resending at " << positionMetadata << " as expected delay " << expectedDelay
                                           << " is no better than average delay";
            }
#endif

            return expectedDelay < 5;
        }

        bool shouldResendGatherCommand(const MyWorker &worker,
                                       const std::map<int, PositionObservationMetadata> &positionMetadata,
                                       bool &mayGetUnitBusy)
        {
            // The logic is just checking if successes outweigh failures
            // In reality we don't really see failures so this doesn't matter too much
            unsigned int successes = 0;
            unsigned int failures = 0;
            for (const auto &[_, metadata] : positionMetadata)
            {
                successes += metadata.successes;
                failures += metadata.failures;
            }

            mayGetUnitBusy = positionMetadata.contains(BWAPI::Broodwar->getLatencyFrames()) &&
                    !positionMetadata.contains(BWAPI::Broodwar->getLatencyFrames() + 1);

            return successes >= failures;
        }

        void optimizeArrival(const MyWorker &worker,
                             const Resource &resource,
                             WorkerGatherStatus &workerStatus,
                             std::map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositions,
                             const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (workerStatus.secondResentPosition || workerStatus.resentOnSchedule()) return;

            auto handleOrderProcessTimerReset = [&](unsigned int expectedDelay)
            {
                int framesFromCommandToReset = OrderProcessTimer::framesToNextReset() - BWAPI::Broodwar->getLatencyFrames();
                if (framesFromCommandToReset > 0 && framesFromCommandToReset < (12 + expectedDelay))
                {
                    // Send a command to take effect on the reset frame if it is coming soon
                    // Otherwise just let it take its course
                    if (framesFromCommandToReset < 5)
                    {
                        workerStatus.resendCommandOnFrame = currentFrame + framesFromCommandToReset;
#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Scheduled gather command for approach optimization on frame "
                                                   << workerStatus.resendCommandOnFrame;
                        CherryVis::log(resource->id) << "Scheduled gather command for approach optimization on frame "
                                                     << workerStatus.resendCommandOnFrame;
#endif

                    }
                    return true;
                }

                return false;
            };

            if (!workerStatus.resentPosition)
            {
                auto optimalGatherPositionIt = optimalGatherPositions.find(*currentPosition);
                unsigned int expectedDelay = 0;
                if (optimalGatherPositionIt != optimalGatherPositions.end() &&
                    shouldResendGatherCommand(worker, optimalGatherPositionIt->second, expectedDelay))
                {
                    if (handleOrderProcessTimerReset(expectedDelay))
                    {
                    }
                    else if (worker->gather(resource->getBwapiUnitIfVisible()))
                    {
                        workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Resending gather command for approach optimization at position "
                                                   << optimalGatherPositionIt->second;
                        CherryVis::log(resource->id) << "Resending gather command for approach optimization at position "
                                                     << optimalGatherPositionIt->second;
#endif
                    }
                    else
                    {
                        workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                                   << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
                    }
                }

                return;
            }

            auto resendPositionDataIt = optimalGatherPositions.find(*workerStatus.resentPosition);
            if (resendPositionDataIt == optimalGatherPositions.end()) return;

            auto &resendPositionData = resendPositionDataIt->second;

            auto optimalGatherPositionIt = resendPositionData.failurePositionMetadata.find(*currentPosition);
            bool mayGetUnitBusy = false;
            if (optimalGatherPositionIt != resendPositionData.failurePositionMetadata.end() &&
                shouldResendGatherCommand(worker, optimalGatherPositionIt->second, mayGetUnitBusy))
            {
                if (handleOrderProcessTimerReset(0))
                {
                }
                else if (mayGetUnitBusy)
                {
                    // Sending the command now will result in Unit_Busy, so schedule it for the next frame
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);
                    workerStatus.secondResentPosition = currentPosition;
                }
                else if (worker->gather(resource->getBwapiUnitIfVisible()))
                {
                    workerStatus.secondResentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Resending second gather command for approach optimization";
                    CherryVis::log(resource->id) << "Resending second gather command for approach optimization";
#endif

                }
                else
                {
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "Failed to send second gather command for approach optimization for " << worker->id << " @ "
                               << worker->getTilePosition() << ": " << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send second gather command for approach optimization; last error "
                                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(resource->id) << "Failed to send second gather command for approach optimization; last error "
                                                 << BWAPI::Broodwar->getLastError();
#endif
                }
            }
        }
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const Resource &resource)
    {
        auto &optimalGatherPositions = optimalGatherPositionsFor(resource);
        auto &tenDistancePositions = tenDistancePositionsFor(resource);
        auto &takeoverResendPositions = takeoverPositionsFor(resource);
        auto &workerStatus = gatherStatusFor(worker, resource);

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            workerStatus.reset();
            return;
        }

        auto currentPosition = std::make_shared<PositionAndVelocity>(worker);

        // Clear worker status if it wasn't processed last frame
        if (workerStatus.lastProcessedFrame != (currentFrame - 1))
        {
            workerStatus.reset();
        }
        workerStatus.lastProcessedFrame = currentFrame;

        // Track the worker's visited positions
        workerStatus.positionHistory.emplace_back(currentPosition);

        // Don't touch the worker if it is transitioning to mine
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) return;

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
            handleStartOfMiningPatchSwitch(workerStatus, resource, tenDistancePositions, takeoverResendPositions);

            CherryVis::log(worker->id) << "targeting different patch; resending order";
            Log::Get() << "patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition() << " switched patch";

            worker->gather(resourceBwapiUnit);
            workerStatus.positionHistory.clear();
            workerStatus.takeoverMode = 2;
            workerStatus.lastProcessedFrame = currentFrame;
            return;
        }

        // Resend the gather command if it has been scheduled for this frame
        if (workerStatus.resendCommandOnFrame == currentFrame)
        {
            worker->gather(resourceBwapiUnit);

#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(worker->id) << "Resending gather command on schedule";
            CherryVis::log(resource->id) << "Resending gather command on schedule";
#endif
            return;
        }

        // Handle case where another worker is assigned to the patch
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && (currentFrame - otherWorker->lastStartedMining) < 100)
        {
            // Keep track of whether the worker has passed a 10-distance position
            if (!workerStatus.passed10DistancePosition && tenDistancePositions.contains(*currentPosition))
            {
                workerStatus.passed10DistancePosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Will reach 10-distance position in LF+1 from here";
                CherryVis::log(resource->id) << "Will reach 10-distance position in LF+1 from here";
#endif
            }

            // Compute the optimal frame to take over from the other worker

            // We need to add an extra frame if the worker taking over might have its orders processed first
            int addedFrame = 1;
            if (otherWorker->orderProcessIndex > worker->orderProcessIndex)
            {
                addedFrame = 0;
            }

            // Without order timer resets, we can compute the exact takeover frame
            int takeOverFrame = otherWorker->lastStartedMining + 81 + addedFrame;

            // Compute the frame of the order timer reset prior to the take over frame
            int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(takeOverFrame);
            if (previousOrderTimerReset == takeOverFrame) previousOrderTimerReset -= 150;

            // If the order timer reset during mining, adjust our take over frame
            // We always assume the worst-case scenario (needing to wait a full cycle after the mining timer expires)
            // Because the order timer is at 6 when mining ends without a reset, we only have to wait two extra frames
            if (previousOrderTimerReset >= otherWorker->lastStartedMining)
            {
                takeOverFrame = std::max(otherWorker->lastStartedMining + 83, previousOrderTimerReset + 8) + addedFrame;
            }

            // Now compute when we need to issue mining commands
            // Besides issuing a mining command for the takeover frame, we also want to issue a command if the order timer resets
            int commandFrameForTakeOver = takeOverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
            int commandFrameForReset = previousOrderTimerReset - BWAPI::Broodwar->getLatencyFrames();

            // If the takeover frame comes first, delay sending the order so it takes effect when the order timer resets instead
            // This is to avoid situations where the second worker's command takes effect too soon, causing it to switch to a different patch
            if (commandFrameForReset > commandFrameForTakeOver)
            {
                commandFrameForTakeOver = commandFrameForReset;
            }

            // Compute the number of frames until the next command we have to send
            // We ignore order process timer resets if we are far enough away from the patch that it will not affect us
            auto distToPatch = resource->getDistance(worker);
            int framesToNextCommand;
            if (currentFrame <= commandFrameForReset && (commandFrameForTakeOver - commandFrameForReset) > 3
                && (workerStatus.passed10DistancePosition || distToPatch == 0))
            {
                framesToNextCommand = std::min(commandFrameForReset, commandFrameForTakeOver) - currentFrame;
            }
            else
            {
                framesToNextCommand = commandFrameForTakeOver - currentFrame;
            }

#if TAKEOVER_DEBUG
            CherryVis::log(worker->id)
                    << "Timing for takeover from " << otherWorker->id << ": "
                    << "otherStarted=" << otherWorker->lastStartedMining << "; "
                    << "takeOverFrame=" << takeOverFrame << "; "
                    << "previousOrderTimerReset=" << previousOrderTimerReset << "; "
                    << "commandFrameForTakeOver=" << commandFrameForTakeOver << "; "
                    << "commandFrameForReset=" << commandFrameForReset << "; "
                    << "framesToNextCommand=" << framesToNextCommand << "; "
                    << "addedFrame=" << addedFrame << "; "
                    << "distToPatch=" << distToPatch << "; "
                    << "passed10Distance=" << (workerStatus.passed10DistancePosition != nullptr) << "; "
                    << "resentPosition=" << (workerStatus.resentPosition != nullptr) << "; "
                    << "takeoverMode=" << workerStatus.takeoverMode;
#endif

            // Logic for when the next command is in the future
            if (framesToNextCommand > 0)
            {
                // Reset some state in case we needed to resend because of the order timer
                workerStatus.resentPosition = nullptr;
                workerStatus.secondResentPosition = nullptr;
                workerStatus.takeoverMode = 0;

                // Once the worker is close to the patch, resend commands every 2 frames to avoid the worker switching patches
                if (framesToNextCommand % 2 == 0 && (workerStatus.passed10DistancePosition || distToPatch == 0))
                {
                    worker->gather(resourceBwapiUnit);
                }

                return;
            }

            switch (workerStatus.takeoverMode)
            {
                case 0:
                {
                    // Try to use normal approach optimization, which applies if the worker is not able to reach the patch by the takeover frame
                    if (distToPatch > 0)
                    {
                        optimizeArrival(worker, resource, workerStatus, optimalGatherPositions, currentPosition);
                        if (workerStatus.resentPosition || workerStatus.resentOnSchedule()) return;
                    }

                    // intentional fall-through
                }
                case 1:
                {
                    if (distToPatch == 0 && !workerStatus.resentPosition && !workerStatus.resentOnSchedule())
                    {
                        workerStatus.takeoverMode = 2;
                        worker->gather(resource->getBwapiUnitIfVisible());

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Resending gather command for takeover optimization, as have arrived at patch";
                        CherryVis::log(resource->id) << "Resending gather command for takeover optimization, as have arrived at patch";
#endif
                        break;
                    }

                    auto takeoverPositionValid = [&](const PositionObservationMetadata &takeoverPositionData)
                    {
#if OPTIMALPOSITIONS_DEBUG
                        if (takeoverPositionData.failures > 0)
                        {
                            CherryVis::log(worker->id) << "Rejecting for takeover optimization: " << takeoverPositionData;
                            CherryVis::log(resource->id) << "Rejecting for takeover optimization: " << takeoverPositionData;
                        }
#endif

                        // TODO: Revisit the logic after performing some analysis
                        return takeoverPositionData.failures == 0;
                    };

                    // Try to use takeover optimization
                    // This differs from approach optimization as it just tries to ensure the worker will reach the patch on time, not what the order
                    // process timer is
                    if (!workerStatus.resentPosition && !workerStatus.resentOnSchedule())
                    {
                        // Use the first valid position we come to
                        auto takeoverPositionIt = takeoverResendPositions.find(*currentPosition);
                        if (takeoverPositionIt != takeoverResendPositions.end() && takeoverPositionValid(takeoverPositionIt->second))
                        {
                            workerStatus.takeoverMode = 1;

                            if (worker->gather(resource->getBwapiUnitIfVisible()))
                            {
                                workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                                CherryVis::log(worker->id) << "Resending gather command for takeover optimization at position " << takeoverPositionIt->second;
                                CherryVis::log(resource->id) << "Resending gather command for takeover optimization at position "
                                                             << takeoverPositionIt->second;
#endif
                            }
                            else
                            {
                                workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                                Log::Get() << "Failed to send gather command for takeover optimization for " << worker->id << " @ "
                                           << worker->getTilePosition() << ": " << BWAPI::Broodwar->getLastError();
                                CherryVis::log(worker->id) << "Failed to send gather command for takeover optimization; last error "
                                                           << BWAPI::Broodwar->getLastError();
                                CherryVis::log(resource->id) << "Failed to send gather command for takeover optimization; last error "
                                                             << BWAPI::Broodwar->getLastError();
#endif
                            }
                        }
                        else if (workerStatus.passed10DistancePosition)
                        {
                            // As we've passed the 10-distance position, we had better send a command here
                            // We probably just lack the data needed to know which position to send from
                            workerStatus.takeoverMode = 1;
                            worker->gather(resource->getBwapiUnitIfVisible());
                            workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                            CherryVis::log(worker->id) << "Resending as we have reached 10 distance and haven't observed a good position yet";
                            CherryVis::log(resource->id) << "Resending as we have reached 10 distance and haven't observed a good position yet";
#endif
                        }
                    }
                    break;
                }
                case 2:
                {
                    // Worker is at patch and a command has been sent, so no further orders needed
                    return;
                }
            }

            return;
        }

        // Single worker approach optimization
        optimizeArrival(worker, resource, workerStatus, optimalGatherPositions, currentPosition);
    }
}