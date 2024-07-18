#include "WorkerMiningOptimization.h"

#include "Workers.h"
#include "WorkerOrderTimer.h"
#include "OrderProcessTimer.h"

#define USE_OLD_LOGIC false

#if INSTRUMENTATION_ENABLED
#define LOG_DEBUG_INFO true
#endif

namespace WorkerMiningOptimization
{
    void initialize()
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::initialize();
        return;
#endif

        // TODO
    }

    void write()
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::write();
        return;
#endif

        // TODO
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const Resource &resource)
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::optimizeStartOfMining(worker, resource);
        return;
#endif

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            return;
        }

        // Our logic ensures mineral locking automatically except in some specific cases:
        // - worker has been released from combat, which can leave it with a gather order to a random patch used for kiting
        // - workers have been avoiding a no-go area and returning to mining as a group, so the timing gets messed up
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            CherryVis::log(worker->id) << "targeting different patch; resending order";
            worker->gather(resourceBwapiUnit);
            return;
        }

        // Handle case where another worker is assigned to the patch
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && (currentFrame - otherWorker->lastStartedMining) < 100)
        {
            // Compute the optimal frame to take over from the other worker

            // We need to add an extra frame if the worker taking over has its orders processed first
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
            if (previousOrderTimerReset > otherWorker->lastStartedMining)
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
            // We send regular commands to avoid having the worker switch patches
            int framesToNextCommand;
            if (currentFrame <= commandFrameForReset && (commandFrameForTakeOver - commandFrameForReset) > 3)
            {
                framesToNextCommand = std::min(commandFrameForReset, commandFrameForTakeOver) - currentFrame;
            }
            else
            {
                framesToNextCommand = commandFrameForTakeOver - currentFrame;
            }

#if LOG_DEBUG_INFO
            CherryVis::log(worker->id)
                << "Timing for takeover from " << otherWorker->id << ": "
                << "otherStarted=" << otherWorker->lastStartedMining << "; "
                << "takeOverFrame=" << takeOverFrame << "; "
                << "previousOrderTimerReset=" << previousOrderTimerReset << "; "
                << "commandFrameForTakeOver=" << commandFrameForTakeOver << "; "
                << "commandFrameForReset=" << commandFrameForReset << "; "
                << "framesToNextCommand=" << framesToNextCommand << "; "
                << "addedFrame=" << addedFrame << "; "
                << "distToPatch=" << resource->getDistance(worker);
#endif

            // Logic for when the next command is in the future
            if (framesToNextCommand >= 0)
            {
                // Issue commands every 4 frames
                if (framesToNextCommand % 4 == 0)
                {
                    worker->gather(resourceBwapiUnit);
                }
                return;
            }

            // Fall through to normal optimization - if we are still approaching the patch it may send a new command to optimize mining at arrival
            // There is one specific case that is not handled - if the approach optimization tries to send a gather command exactly LF after the
            // previous one, BWAPI will give a Unit_Busy error
            // However if our optimization is working correctly, workers should never arrive too late to the patch anyway, so I'm not going to spend
            // effort on this now
        }

        // TODO: Approach optimization

    }

    // Optimizes returning a resource, returning whether an order was sent to the worker.
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot)
    {
        // TODO
    }
}