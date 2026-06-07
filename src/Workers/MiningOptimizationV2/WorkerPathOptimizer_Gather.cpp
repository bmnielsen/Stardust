#include "WorkerPathOptimizer.h"

#include "Workers.h"
#include "OrderProcessTimer.h"
#include "LogFormattingUtil.h"

#include "DebugFlag_MiningOptimization.h"
#include "Timer.h"

/*
 * Most of our takeover logic is in this file, with the rest related to coordination between workers in MiningOptimization.cpp.
 *
 * For takeover we basically do it in two phases.
 *
 * First, the takeoverActionFrames method is called to get the set of achievable takeover frames with 100% confidence. This looks at the combined
 * arrival data on the remaining branches focusing on the resend always arrives frame. This comes from our training data and tells us from what
 * frame a subsequent resend will always get us to the patch before the order process timer reaches 0 (if no reset occurs). Taking these values
 * and subtracting impossible resend frames and resends that get hit by an order process timer reset results in the set of achievable frames.
 *
 * Second, the useTakeoverFrame method is called with the takeover frame the coordinator wants this worker to target. This method then plans the
 * actual resends needed to achieve this takeover frame. At this point we need to consider the interaction of the 10 distance frame with the
 * order process timer, detecting frames where there is a possibility of the worker switching patches and planning additional resends to avoid
 * this. The result of this method is to update the set of resend frames needed to achieve the given takeover frame.
 *
 * The result of the takeoverActionFrames method will change over time, both as we become more confident of which specific path branch we are on,
 * and as certain takeover frames become impossible because of resends sent to target a different frame.
 */

namespace MiningOptimization
{
    template <>
    std::set<int> WorkerPathOptimizer<GatherArrivalData>::takeoverActionFrames(int latestTakeoverFrame)
    {
        // We assume if the worker is within 10 pixels of the patch, it will arrive after a resend
        // TODO: Analyze the resend always arrives data to validate this
        if (!expectedPath && resource->getDistance(worker) > 10) return {};

        // The possible takeover frames comes directly from the already-compute action frames and resend always arrives frames
        // As we only return values we can guarantee are achievable (barring losing the path), we use the following logic:
        // - If there is only one action frame, it is the earliest possible takeover frame
        // - From here we start with the largest resend always arrives frame and generate possible takeover frames from it
        // - We skip any resend frames that would have an order process timer reset before the action frame
        // - We also skip a resend frame that has an order process timer reset immediately after the action frame, since this prolongs mining

        std::set<int> result;

        if (expectedPath && expectedPath->actionFramesWithProbabilities.size() == 1)
        {
            // Subtract one since for takeover we are interested in the WaitForMinerals frame, whereas action frame is first mining frame
            result.insert(expectedPath->actionFramesWithProbabilities.begin()->first - 1);
        }

        int startFrame = currentFrame + BWAPI::Broodwar->getLatencyFrames();
        if (expectedPath)
        {
            for (const auto &[arrivalData, _] : expectedPath->arrivalDataWithProbabilities)
            {
                startFrame = std::max(startFrame, arrivalData.resendAlwaysArrivesFrame);
            }
        }

        if (!result.empty() && (*result.begin()) > (startFrame + 11))
        {
            Log::Get() << "ERROR: Start frame gives earlier than action frame";
        }

        // Generate frames that don't have an unfortunate order process timer reset
        for (int frame = startFrame; frame <= latestTakeoverFrame; ++frame)
        {
            if (OrderProcessTimer::framesToNextReset(frame) < 12) continue;
            result.insert(frame + 11);
        }

        return result;
    }

    template <>
    void WorkerPathOptimizer<GatherArrivalData>::useTakeoverFrame(int takeoverFrame)
    {
        // If the frame doesn't differ from our current plan, just return now
        if (expectedTakeoverFrame == takeoverFrame) return;

        // Wait to do detailed planning until the remaining resends are stable
        // If we don't have a path, we also don't have any planned resends, so this also counts as stable
        std::set<int> pathResendFrames;
        if (expectedPath)
        {
            auto aggregatedResendFrames = expectedPath->aggregatedResendFramesIfStable();
            if (!aggregatedResendFrames) return;
            pathResendFrames = std::move(*aggregatedResendFrames);
        }

        // Get the earliest 10-distance frame
        int earliestTenDistanceFrame = INT_MAX;
        if (expectedPath)
        {
            for (const auto &[arrivalData, _] : expectedPath->arrivalDataWithProbabilities)
            {
                earliestTenDistanceFrame = std::min(earliestTenDistanceFrame, arrivalData.tenDistanceFrame);
            }
        }
        if (earliestTenDistanceFrame == INT_MAX || earliestTenDistanceFrame <= currentFrame)
        {
            earliestTenDistanceFrame = currentFrame + 1;
        }

        // Simulate the worker's order process timer at the earliest ten distance frame to figure out when the worker might try to switch patches
        // without a resend
        std::set<int> allResendFrames = executedResendFrames;
        allResendFrames.insert(pathResendFrames.begin(), pathResendFrames.end());
        auto orderProcessTimerValues = OrderProcessTimer::atStartOfFrameAtDelta(
            currentFrame + 1,
            OrderProcessTimer::atStartOfNextFrame(currentFrame, worker->possibleOrderProcessTimerValues),
            allResendFrames,
            {},
            earliestTenDistanceFrame - currentFrame - 1);
        int switchPatchFrame = earliestTenDistanceFrame + *orderProcessTimerValues.begin();

        CherryVis::log(worker->id) << "Switch patch frame " << switchPatchFrame << "; " << orderProcessTimerValues.size() << "; " << pathResendFrames.size();
    }

    template <>
    bool WorkerPathOptimizer<GatherArrivalData>::isComplete()
    {
        return worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals
            || worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals;
    }

    template <>
    void WorkerPathOptimizer<GatherArrivalData>::setStartOfPathFlags()
    {
        // Started at the end of last path if the worker just delivered minerals
        if (!worker->carryingResource && worker->lastCarryingResourceChange == currentFrame)
        {
            setFlag(StatusFlags::StartedAtPreviousPathEnd);
        }

        // Is at the initial spawn position if this is the start of the game
        if (currentFrame <= 2 && worker->lastPosition == worker->spawnPosition)
        {
            setFlag(StatusFlags::StartedAtInitialSpawnPosition);
        }
    }

    template <>
    bool WorkerPathOptimizer<GatherArrivalData>::skipPathOptimization()
    {
        // Ensure the resource is visible
        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            return true;
        }

        // Ensure the worker is targeting the correct resource
        // Our logic ensures mineral locking automatically except in some specific cases:
        // - worker has been released from combat, which can leave it with a gather order to a random patch used for kiting
        // - workers have been avoiding a no-go area and returning to mining as a group, so the timing gets messed up
        // - both workers reach the patch at approximately the same time after one or both are (re)assigned
        // - we get a diversion from our observed path and are unlucky with the order timer
        // - we mispredict a patch locking
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
#if LOGGING_ENABLED
            CherryVis::log(worker->id) << "targeting different patch; resending order";
#endif
            // There could be a Unit_Busy failure here, but we will pick up next frame that the command hasn't been issued
            if (worker->gather(resourceBwapiUnit))
            {
                executedResendFrames.insert(currentFrame);
            }
            setFlag(StatusFlags::SwitchedPatch);
            resetPath();
            return true;
        }

        // If the worker is transitioning to mine, nothing more is needed
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            // If another worker is mining the patch, ensure we have marked patch locking
            if (actualPatchLockFrame == -1)
            {
                auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
                if (otherWorker && otherWorker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    actualPatchLockFrame = currentFrame;

#if VERBOSE_PATH_LOGGING
                    CherryVis::log(worker->id) << "Patch locked";
#endif
                }
            }

            return true;
        }

        return false;
    }

    template <>
    void WorkerPathOptimizer<GatherArrivalData>::initializeGatherTakeover()
    {
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (!otherWorker)
        {
            // There might have been another worker earlier, if so clear all of our state
            // TODO: Replan if possible
            if (hasFlag(StatusFlags::GatherTakeover))
            {
                reset();
            }
            return;
        }

        setFlag(StatusFlags::GatherTakeover);

        // If the other worker is not mining, clear the takeover frames, as the patch is now available
        if (otherWorker->bwapiUnit->getOrder() != BWAPI::Orders::MiningMinerals || otherWorker->lastStartedMining == -1)
        {
            takeoverFrames.clear();
            return;
        }

        // Compute the takeover frame probabilities, i.e. the probability at any given frame that we will be able to take over mining from the other
        // worker

        // Nothing is needed if they have already been computed
        if (!takeoverFrames.empty()) return;

        // Take into account the relative order process index of the workers: if this worker's orders are processed before the other worker's,
        // we need an extra frame before the patch is available
        int extraFrame = (worker->orderProcessIndex <= otherWorker->orderProcessIndex) ? 1 : 0;

        // With no order timer resets, mining will end 81 frames after it starts
        int miningEndFrame = otherWorker->lastStartedMining + 81;

        // Check for an order process timer reset before mining end
        int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(miningEndFrame);
        if (previousOrderTimerReset < otherWorker->lastStartedMining)
        {
            // There was no reset, so the patch will be available at the end frame computed earlier
            takeoverFrames[miningEndFrame + extraFrame] = 1.0;

#if VERBOSE_TAKEOVER_LOGGING
            CherryVis::log(otherWorker->id) << "Extra frame: " << extraFrame
                                            << "\nTakeover frame: " << (miningEndFrame + extraFrame);
#endif
            return;
        }

        // There was an order process timer reset, so mining could end at the latest of the following frames:
        // - Mining timer expiry
        // - Order process timer reset
        // - Next frame
        int earliestMiningEndFrame = std::max({otherWorker->lastStartedMining + 75, previousOrderTimerReset, currentFrame + 1});

        // Get the possible order process timer values at the earliest end frame
        std::multiset<int> orderProcessTimerValuesAtEarliestMiningEndFrame;
        if (OrderProcessTimer::isResetFrame(currentFrame + 1))
        {
            orderProcessTimerValuesAtEarliestMiningEndFrame = {0, 1, 2, 3, 4, 5, 6, 7};
        }
        else
        {
            orderProcessTimerValuesAtEarliestMiningEndFrame =
                    OrderProcessTimer::atStartOfFrameAtDelta(currentFrame + 1,
                                                             otherWorker->possibleOrderProcessTimerValues,
                                                             {},
                                                             {},
                                                             (earliestMiningEndFrame - (currentFrame + 1)));
        }

        // Now generate the probabilities of each end frame
        for (auto orderProcessTimerValue : orderProcessTimerValuesAtEarliestMiningEndFrame)
        {
            takeoverFrames[earliestMiningEndFrame + orderProcessTimerValue + extraFrame]
                = 1.0 / (double)orderProcessTimerValuesAtEarliestMiningEndFrame.size();
        }

#if VERBOSE_TAKEOVER_LOGGING
        CherryVis::log(otherWorker->id) << "Extra frame: " << extraFrame
                                        << "\nTakeover frames: " << LogFormattingUtil::formatProbabilityMap(takeoverFrames, INT_MAX);
#endif
    }

    template <>
    bool WorkerPathOptimizer<GatherArrivalData>::issueResend()
    {
        auto bwapiUnit = resource->getBwapiUnitIfVisible();
        if (!bwapiUnit)
        {
            BWAPI::Broodwar->setLastError(BWAPI::Errors::Unit_Not_Visible);
            return false;
        }

        return worker->gather(bwapiUnit);
    }

    template class WorkerPathOptimizer<GatherArrivalData>;
}
