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
    namespace
    {
        std::multiset<int> orderProcessTimerAtTenDistanceFrame(const MyWorker &worker, const std::set<int> resendFrames, int tenDistanceFrame)
        {
            auto orderProcessTimerValues = OrderProcessTimer::atStartOfFrameAtDelta(
                currentFrame + 1,
                OrderProcessTimer::atStartOfNextFrame(currentFrame, worker->possibleOrderProcessTimerValues),
                resendFrames,
                {},
                tenDistanceFrame - currentFrame - 1);

            // If there was a recent resend, override the order process timer value to simulate the fact that a 0 value won't actually take
            // effect here
            if (orderProcessTimerValues.size() == 1 && orderProcessTimerValues.contains(0))
            {
                for (int i = 0; i <= 1; i++)
                {
                    if (resendFrames.contains(tenDistanceFrame - BWAPI::Broodwar->getLatencyFrames() - i - 1))
                    {
                        orderProcessTimerValues = {9 + i};
                    }
                }
            }

            return orderProcessTimerValues;
        }
    }

    template <>
    std::set<int> WorkerPathOptimizer<GatherArrivalData>::takeoverActionFrames(int latestTakeoverFrame, const PatchOccupiedForecast &forecast)
    {
        // We assume if the worker is within 10 pixels of the patch, it will arrive after a resend
        if (!expectedPath && resource->getDistance(worker) > ASSUME_RESEND_ALWAYS_ARRIVES_DISTANCE) return {};

        // The possible takeover frames comes directly from the already-compute action frames and resend always arrives frames
        // As we only return values we can guarantee are achievable (barring losing the path), we use the following logic:
        // - If we haven't started takeover and there is only one action frame, it is the earliest possible takeover frame
        // - If we have already issued a takeover resend, the currently-targeted takeover frame is achievable
        // - From here we start with the largest resend always arrives frame and generate possible takeover frames from it
        // - We skip any resend frames that would have an order process timer reset before the action frame
        // - We also skip a resend frame that has an order process timer reset immediately after the action frame, since this prolongs mining

        std::set<int> result;

        // Gather all of the resend frames we might use
        std::set<int> allResendFrames = executedResendFrames;
        if (expectedPath) allResendFrames.insert(expectedPath->resendFramesOnAllBranches.begin(), expectedPath->resendFramesOnAllBranches.end());

        // If there is a current takeover or pathing action frame, it almost always gives an achievable takeover frame
        if (hasFlag(StatusFlags::IssuedTakeoverResend))
        {
            if (expectedTakeoverFrame != -1) result.insert(expectedTakeoverFrame);
        }
        else if (expectedPath && expectedPath->actionFramesWithProbabilities.size() == 1)
        {
            auto takeoverFrame = expectedPath->actionFramesWithProbabilities.begin()->first - 1;

            // Check if there might be a patch switch
            auto patchSwitchWithArrivalData = [&](const SolverArrivalData &arrivalData)
            {
                // Consider the ten-distance frame, or the next frame if it has already passed
                auto frame = std::max(arrivalData.tenDistanceFrame, currentFrame + 1);
                if (hasFlag(StatusFlags::ReachedTenDistance)) frame = currentFrame + 1;

                // Get the worker's order process timer at the start of the frame
                auto orderProcessTimerValues = orderProcessTimerAtTenDistanceFrame(worker, allResendFrames, frame);

                // Check if a patch switch may happen when the worker's order timer reaches 0
                int switchPatchFrame = frame + *orderProcessTimerValues.begin();
                if (switchPatchFrame >= takeoverFrame) return false;
                if (forecast.atFrame(switchPatchFrame) > 0.001) return true;
                return false;
            };

            bool possiblePatchSwitch = false;
            if (takeoverFrame > (currentFrame + 8))
            {
                for (const auto &[arrivalData, _] : expectedPath->arrivalDataWithProbabilities)
                {
                    possiblePatchSwitch = possiblePatchSwitch || patchSwitchWithArrivalData(arrivalData);
                }
            }

            if (!possiblePatchSwitch)
            {
                result.insert(expectedPath->actionFramesWithProbabilities.begin()->first - 1);
            }
        }

        int startFrame = currentFrame + BWAPI::Broodwar->getLatencyFrames();
        if (expectedPath)
        {
            for (const auto &[arrivalData, _] : expectedPath->arrivalDataWithProbabilities)
            {
                startFrame = std::max(startFrame, arrivalData.resendAlwaysArrivesFrame);
            }
        }

        // Generate frames that don't have an unfortunate order process timer reset or collide with an already-planned resend
        int resendTakesEffectFrame = startFrame - 1;
        while (true)
        {
            ++resendTakesEffectFrame;

            // Don't use resends with an uncertain action frame due to order process timer resets
            int framesToNextReset = OrderProcessTimer::framesToNextReset(resendTakesEffectFrame);
            if (framesToNextReset > 0 && framesToNextReset < 12) continue;

            // Don't allow resends that would be blocked by a previously-issued resend
            if (allResendFrames.contains(resendTakesEffectFrame - BWAPI::Broodwar->getLatencyFrames() * 2)) continue;

            // Add the takeover frame and break if it is the first that meets the desired frame
            int takeoverFrame = resendTakesEffectFrame + 11;
            result.insert(takeoverFrame);
            if (takeoverFrame >= latestTakeoverFrame) break;
        }

        CherryVis::log(worker->id) << "Possible takeover frames: " << LogFormattingUtil::formatVectorlike(result);

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
            // If the takeover frame corresponds to the currently-planned action frame, don't plan for any additional resends
            if (!hasFlag(StatusFlags::IssuedTakeoverResend) && expectedPath->actionFramesWithProbabilities.size() == 1 &&
                expectedPath->actionFramesWithProbabilities.begin()->first == (takeoverFrame + 1))
            {
                expectedTakeoverFrame = takeoverFrame;
                takeoverResendFrames.clear();
#if LOGGING_ENABLED
                CherryVis::log(worker->id) << "Planned takeover frame of " << takeoverFrame
                                           << " using normal pathing arrival";
#endif

                return;
            }

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
        auto orderProcessTimerValues = orderProcessTimerAtTenDistanceFrame(worker, allResendFrames, earliestTenDistanceFrame);

        int switchPatchFrame = earliestTenDistanceFrame + *orderProcessTimerValues.begin();
        int switchPatchResendFrame = switchPatchFrame - BWAPI::Broodwar->getLatencyFrames() - 1; // One before to avoid transition to WaitForMinerals

        // Compute the resend frame for the next order process timer reset
        int resetResendFrame = OrderProcessTimer::nextResetFrame(switchPatchFrame) - BWAPI::Broodwar->getLatencyFrames();

        // Compute the resend frame to reach the desired takeover frame
        int takeoverResendFrame = takeoverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();

        // We now have the three frames we need to compute the resend strategy:
        // switchPatchResendFrame tells us where we need to resend to avoid having a switch patch at arrival to 10-distance
        // resetResendFrame tells us where we need to resend to avoid an order process timer reset causing a patch switch
        // takeoverResendFrame tells us where we need to issue the last resend to achieve the desired takeover
        // The third is of course always needed, but depending on the timing, the first two may not be needed

        // Start with the takeover resend frame
        takeoverResendFrames.clear();
        takeoverResendFrames.insert(takeoverResendFrame);

        // Helper to check if a given resend frame can be issued without being LF from an existing resend
        auto isResendFrameViable = [&](int frame)
        {
            if (takeoverResendFrames.contains(frame + BWAPI::Broodwar->getLatencyFrames())) return false;
            if (takeoverResendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames())) return false;
            if (allResendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames())) return false;
            return true;
        };

        // Helper to add the next viable resend frame from the given one
        auto addNextViableResendFrame = [&](int frame)
        {
            // Try four frames before giving up
            for (int f = frame; f <= (frame + 4); f++)
            {
                // If the frame has already been used as a resend frame, we don't need to handle this case
                if (allResendFrames.contains(f)) return;

                // Skip ahead to the current frame
                if (f < currentFrame) continue;

                // If the frame goes beyond the takeover resend frame, it won't be needed
                if (f >= takeoverResendFrame) return;

                // Add the frame if it is viable
                if (isResendFrameViable(f))
                {
                    takeoverResendFrames.insert(f);
                    return;
                }
            }
            Log::Get() << "WARNING: Worker " << *worker << " could not find a viable resend frame";
        };

        // If the reset resend frame is relevant, add it next
        if (resetResendFrame < takeoverResendFrame && resetResendFrame > switchPatchResendFrame)
        {
            addNextViableResendFrame(resetResendFrame);
        }

        // Finally add the patch switch resend frame
        if (switchPatchResendFrame < takeoverResendFrame)
        {
            addNextViableResendFrame(switchPatchResendFrame);
        }

        // Now check if there is a gap between any two of the resend frames long enough to cause a patch switch
        std::set<int> additionalNeededFrames;
        int previousFrame = *takeoverResendFrames.begin();
        for (int frame : takeoverResendFrames)
        {
            if (frame == previousFrame) continue; // first element

            int delta = (frame - previousFrame);
            if (delta > 7)
            {
                int steps = delta / 5;
                int step = delta / steps;
                for (int i = 1; i <= steps; i++)
                {
                    addNextViableResendFrame(previousFrame + i * step);
                }
            }

            previousFrame = frame;
        }
        takeoverResendFrames.insert(additionalNeededFrames.begin(), additionalNeededFrames.end());

        expectedTakeoverFrame = takeoverFrame;

#if LOGGING_ENABLED
        CherryVis::log(worker->id) << "Planned takeover frame of " << takeoverFrame
                                   << ": takeoverResendFrame=" << takeoverResendFrame
                                   << "; switchPatchResendFrame=" << switchPatchResendFrame
                                   << "; resetResendFrame=" << resetResendFrame
                                   << "; resends=" << LogFormattingUtil::formatVectorlike(takeoverResendFrames);
#endif
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

        // Handle a special case where the worker has transitioned to WaitForMinerals while a resend was pending
        // This can happen in cases where we get different path arrival than expected and didn't realize we would patch lock, so sent another resend
        // ahead of time
        // The result is that the worker spends an extra frame in ResetHarvestCollision that bumps the timing one frame into the future
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::ResetCollision &&
            hasFlag(StatusFlags::IssuedTakeoverResend) && expectedTakeoverFrame != -1 &&
            executedResendFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1))
        {
            ++expectedTakeoverFrame;
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
#if ENABLE_TAKEOVER_LOGIC
            Log::Get() << "Patch switch; " << *worker;
#endif
#endif

            setFlag(StatusFlags::SwitchedPatch);
            resetPath();
            lastProcessedFrame = currentFrame;

            // There could be a Unit_Busy failure here, but we will pick up next frame that the command hasn't been issued
            if (worker->gather(resourceBwapiUnit))
            {
                executedResendFrames.insert(currentFrame);
                expectedTakeoverFrame = currentFrame + 11 + BWAPI::Broodwar->getLatencyFrames();
            }
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
