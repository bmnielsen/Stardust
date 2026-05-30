#include "PatchOccupiedForecast.h"

#include "MyWorker.h"
#include "OrderProcessTimer.h"
#include "Workers.h"

namespace MiningOptimization
{
    PatchOccupiedForecast::PatchOccupiedForecast(Resource patch) : patch(std::move(patch)), startFrame(currentFrame)
    {
        // Initialize the forecast with the data for a currently mining worker
        // Get the mining worker and the next mining worker, either or both of which may be null
        MyWorker miningWorker;
        MyWorker nextMiningWorker;
        for (auto &worker : Workers::getWorkersAssignedTo(this->patch))
        {
            if (!worker->exists()) continue;

            // Don't consider workers returning, since we currently don't have the capability to simulate when they will get back to the patch
            // (and this is probably further into the future than we need to simulate anyway)
            if (worker->carryingResource) continue;

            if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
            {
                miningWorker = worker;
                continue;
            }

            // The next mining worker is assumed to be the one closest to the patch if there are two approaching
            // We also treat a worker transitioning to mining as the next mining worker
            if (!nextMiningWorker || worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals ||
                this->patch->getDistance(worker) < this->patch->getDistance(nextMiningWorker))
            {
                nextMiningWorker = worker;
            }
        }

        // Process some of the easy cases for a mining worker first
        if (miningWorker)
        {
            miningWorkerOrderProcessIndex = miningWorker->orderProcessIndex;

            // If the worker has transitioned to the mining order but hasn't actually started decrementing its mining timer yet, just mark
            // the patch as occupied for the entire forecast horizon and call it a day
            if (miningWorker->lastStartedMining < miningWorker->lastTransitionedToMiningOrder)
            {
                miningWorkerEarliestEndFrame = miningWorkerLatestEndFrame = currentFrame + 82;
                fullySaturated = true;
                return;
            }

            // Default values that get refined later
            miningWorkerEarliestEndFrame = miningWorkerLatestEndFrame = miningWorker->lastStartedMining + 81;
        }

        // If the next mining worker is in WaitForMinerals, we can usually deduce that the patch will be mined for the entire forecast horizon:
        // - If there is a worker currently mining, the next worker will patch lock and take over from it without any delay
        // - If there is not, the next worker will transition to mining on the next frame
        // The only exception to this is if there is a reset frame after the first WaitForMinerals frame, in which case we need to handle the
        // extra delay.
        std::optional<int> latestTakeoverWorkerFrame = std::nullopt;
        if (nextMiningWorker && nextMiningWorker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            if (nextMiningWorker->lastTransitionedToWaitForMineralsOrder >= (currentFrame - 6) &&
                OrderProcessTimer::isResetFrame(nextMiningWorker->lastTransitionedToWaitForMineralsOrder + 1))
            {
                int start = nextMiningWorker->lastTransitionedToWaitForMineralsOrder + 1;
                int end = start + 6;
                latestTakeoverWorkerFrame = end + 1;

                // Set an ascending probability here if there is no mining worker. If there is a mining worker, this will be handled later as
                // the logic becomes more complicated.
                if (!miningWorker)
                {
                    // Probability starts at 1/8 on the reset frame and increases by 1/8 each subsequent frame
                    double probability = 0.0;
                    for (int frame = start; frame <= end; ++frame)
                    {
                        probability += 0.125;
                        if (frame > currentFrame)
                        {
                            forecast[frame - currentFrame - 1] = probability;
                        }
                    }
                    std::fill(forecast.begin() + (end - currentFrame), forecast.end(), 1.0);
                    return;
                }
            }
            else
            {
                fullySaturated = true;
                return;
            }
        }

        if (!miningWorker) return;

        // Compute the mining end frame if there was no order timer reset
        int miningEndFrame = miningWorker->lastStartedMining + 81;

        // Check for an order process timer reset during mining
        int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(miningEndFrame);
        if (previousOrderTimerReset < miningWorker->lastStartedMining)
        {
            // There was no reset, so we can just write 1s until the end frame
            // We don't need to consider the takeover case since that would indicate there was a reset
            miningWorkerEarliestEndFrame = miningWorkerLatestEndFrame = miningEndFrame;

            int length = std::min(miningEndFrame - currentFrame - 1, PATCH_OCCUPIED_HORIZON);
            if (length == PATCH_OCCUPIED_HORIZON)
            {
                fullySaturated = true;
            }
            else
            {
                std::fill_n(forecast.begin(), std::min(miningEndFrame - currentFrame - 1, PATCH_OCCUPIED_HORIZON), 1.0);
            }
            return;
        }

        int earliestMiningEndFrame = miningWorker->lastStartedMining + 75;
        miningWorkerEarliestEndFrame = earliestMiningEndFrame;
        miningWorkerLatestEndFrame = earliestMiningEndFrame + 8;

        // If the reset happens after the mining timer expires, the earliest end frame is advanced to the reset point
        if (previousOrderTimerReset > earliestMiningEndFrame)
        {
            earliestMiningEndFrame = previousOrderTimerReset;
            miningWorkerEarliestEndFrame = earliestMiningEndFrame;
            miningWorkerLatestEndFrame = earliestMiningEndFrame + 7;
        }

        // If the earliest mining end frame is beyond the end of the horizon, flag as fully saturated and return
        if (earliestMiningEndFrame > (currentFrame + PATCH_OCCUPIED_HORIZON))
        {
            fullySaturated = true;
            return;
        }

        // If the worker taking over will definitely be patch locked by the earliest end frame, flag as fully saturated and return
        int extraTakeoverFrame = 0;
        if (nextMiningWorker && (*miningWorkerOrderProcessIndex > nextMiningWorker->orderProcessIndex)) extraTakeoverFrame = 1;
        if (latestTakeoverWorkerFrame && (*latestTakeoverWorkerFrame + extraTakeoverFrame) <= earliestMiningEndFrame)
        {
            fullySaturated = true;
            return;
        }

        // Fill the array up to the earliest end frame to indicate that the patch is definitely being mined until that point
        if (earliestMiningEndFrame > currentFrame)
        {
            std::fill_n(forecast.begin(), std::min(earliestMiningEndFrame - currentFrame - 1, PATCH_OCCUPIED_HORIZON), 1.0);
        }
        else
        {
            // Mining could have ended by now, but it hasn't, so move the earliest mining end frame to the next frame
            earliestMiningEndFrame = currentFrame + 1;
            miningWorkerEarliestEndFrame = earliestMiningEndFrame;
        }

        // Get the mining worker's possible order process timer values at the start of the next frame
        // This is equivalent to the values at the end of the current frame unless the next frame is a reset
        auto miningWorkerOrderProcessTimerStartOfNextFrame = miningWorker->possibleOrderProcessTimerValues;
        if (OrderProcessTimer::isResetFrame(currentFrame + 1))
        {
            miningWorkerOrderProcessTimerStartOfNextFrame = {0, 1, 2, 3, 4, 5, 6, 7};
        }

        // Determine the possible order process timer values at the earliest mining end frame
        auto orderProcessTimerValues = OrderProcessTimer::atStartOfFrameAtDelta(
            currentFrame + 1,
            miningWorkerOrderProcessTimerStartOfNextFrame,
            {},
            {},
            earliestMiningEndFrame - currentFrame - 1);

        int minOrderProcessTimerResetValue = INT_MAX;
        int maxOrderProcessTimerResetValue = 0;
        for (auto value : orderProcessTimerValues)
        {
            minOrderProcessTimerResetValue = std::min(minOrderProcessTimerResetValue, value);
            maxOrderProcessTimerResetValue = std::max(maxOrderProcessTimerResetValue, value);
        }
        miningWorkerEarliestEndFrame = earliestMiningEndFrame + minOrderProcessTimerResetValue;
        miningWorkerLatestEndFrame = earliestMiningEndFrame + maxOrderProcessTimerResetValue;

        // If there is no takeover worker also waiting, generate the decaying probability using the possible order process timer values
        if (!latestTakeoverWorkerFrame)
        {
            double step = 1.0 / (double)orderProcessTimerValues.size();
            double probabilityHere = 1.0;
            for (int frame = currentFrame + 1; frame < *miningWorkerLatestEndFrame; ++frame)
            {
                int arrayIdx = frame - currentFrame - 1;
                if (arrayIdx >= PATCH_OCCUPIED_HORIZON) break;

                if (orderProcessTimerValues.contains(frame - earliestMiningEndFrame))
                {
                    probabilityHere -= step;
                }

                forecast[arrayIdx] = probabilityHere;
            }
            return;
        }

        // We are in the rare case where the mining worker and the taking over worker are in an order process timer countdown race
        // If the taking over worker "wins" and patch locks before the mining worker is finished, there will be no delay

        // Start by advancing the takeover probability to its value on the next frame
        double takeoverProbability = 0.0;
        for (int frame = nextMiningWorker->lastTransitionedToWaitForMineralsOrder + 1; frame <= currentFrame; ++frame)
        {
            takeoverProbability += 0.125;
        }

        // Initialize the parameters for the mining end probability
        double miningEndStep = 1.0 / (double)orderProcessTimerValues.size();
        double miningEndProbability = 1.0;

        // Now loop the frames forward and compute the total probability at each step
        for (int frame = currentFrame + 1; frame < *latestTakeoverWorkerFrame; ++frame)
        {
            takeoverProbability += 0.125;
            if (frame < earliestMiningEndFrame)
            {
                forecast[frame - currentFrame - 1] = 1.0;
            }
            else
            {
                if (orderProcessTimerValues.contains(frame - earliestMiningEndFrame))
                {
                    miningEndProbability -= miningEndStep;
                }

                auto baseProbability = takeoverProbability - (0.125 * (double)extraTakeoverFrame);
                forecast[frame - currentFrame - 1] = baseProbability + ((1.0 - baseProbability) * miningEndProbability);
            }
        }

        std::fill(forecast.begin() + (*latestTakeoverWorkerFrame - currentFrame - 1), forecast.end(), 1.0);
    }

    void PatchOccupiedForecast::useTakeoverFrame(int frame)
    {
        // This method is used to indicate that a worker will at the latest take over at the given frame
        // This means we can just set 1s from that point onwards, taking into consideration that the action will happen one frame later
        std::fill(forecast.begin() + std::max(0, frame - startFrame - 1), forecast.end(), 1.0);
    }

    void PatchOccupiedForecast::usePatchLockFrame(int frame, int takingOverWorkerOrderProcessIndex)
    {
        // This method is used to indicate that a worker will patch lock at the given frame
        // The currently-mining worker may however already be finished with mining, in which case the worker will take over instead

        // This method does not take the situation into account where the frame after patch lock is an order process timer reset frame
        // We intentionally avoid planning paths that hit this timing, but warn here if we miss one
        if (OrderProcessTimer::isResetFrame(frame + 1))
        {
            Log::Get() << "WARNING: Patch lock frame immediately before a reset; " << *patch;
        }

        // Start by validating the bounds, and if we know the patch is still being mined at the given frame, mark the forecast as
        // fully saturated immediately and return
        int frameIdx = frame - startFrame - 1;
        if (frameIdx >= GATHER_FORECAST_FRAMES) return;
        if (frameIdx < 0 || forecast[frameIdx] > 0.9999)
        {
            fullySaturated = true;
            return;
        }

        // When we get here, we are in a situation where the mining worker may or may not still be mining at the given frame
        // If the mining worker has finished by the time the taking over worker begins, the patch will be occupied from the next frame
        // The probability of this happening is the probability at the frame if the mining worker's orders are processed first, or the
        // probability from the previous frame otherwise
        if (frameIdx > 0 && miningWorkerOrderProcessIndex && takingOverWorkerOrderProcessIndex > *miningWorkerOrderProcessIndex)
        {
            forecast[frameIdx] = forecast[frameIdx - 1];
        }

        std::fill(forecast.begin() + frameIdx, forecast.end(), 1.0);
    }
}
