#include "PatchOccupiedForecast.h"

#include "MyWorker.h"
#include "OrderProcessTimer.h"
#include "Workers.h"

namespace MiningOptimization
{
    namespace
    {
        enum class WorkerOrdering
        {
            MiningWorkerFirst,
            NextMiningWorkerFirst,
            Other
        };
    }

    PatchOccupiedForecast::PatchOccupiedForecast(Resource patch) : patch(std::move(patch)), startFrame(currentFrame)
    {
        // Initialize the forecast with the data for a currently mining worker
        // Get the mining worker and the next mining worker, either or both of which may be null
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

        if (nextMiningWorker) orderProcessIndices.insert(nextMiningWorker->orderProcessIndex);

        // Initialize the forecast with the data for the mining worker
        if (miningWorker)
        {
            orderProcessIndices.insert(miningWorker->orderProcessIndex);

            // If the worker has transitioned to the mining order but hasn't actually started decrementing its mining timer yet, just mark
            // the patch as occupied for the entire forecast horizon and call it a day
            if (miningWorker->lastStartedMining < miningWorker->lastTransitionedToMiningOrder)
            {
                miningWorkerLatestEndFrame = currentFrame + 82;
                fullySaturated = true;
                return;
            }

            // Compute the possible mining end frame range
            int earliestMiningEndFrame = miningWorker->lastStartedMining + 81;
            int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(earliestMiningEndFrame);
            if (previousOrderTimerReset < miningWorker->lastStartedMining)
            {
                miningWorkerLatestEndFrame = earliestMiningEndFrame;
            }
            else
            {
                earliestMiningEndFrame = miningWorker->lastStartedMining + 75;
                miningWorkerLatestEndFrame = earliestMiningEndFrame + 8;

                // If the reset happens after the mining timer expires, the earliest end frame is advanced to the reset point
                if (previousOrderTimerReset > earliestMiningEndFrame)
                {
                    earliestMiningEndFrame = previousOrderTimerReset;
                    miningWorkerLatestEndFrame = earliestMiningEndFrame + 7;
                }
            }

            // If the earliest mining end frame is in the past, we know it hasn't happened yet, so we can advance it to the next frame
            if (earliestMiningEndFrame <= currentFrame)
            {
                earliestMiningEndFrame = currentFrame + 1;
            }

            // If the earliest mining end frame is beyond the end of the horizon, flag as fully saturated and return
            if (earliestMiningEndFrame > (currentFrame + PATCH_OCCUPIED_HORIZON))
            {
                fullySaturated = true;
                return;
            }

            // Detect patch lock on the taking-over worker and mark the patch fully saturated
            // The only special case to consider is when the frame after WaitForMinerals was a reset frame and is in the recent past, which we
            // handle later
            if (nextMiningWorker && nextMiningWorker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals && (
                    nextMiningWorker->lastTransitionedToWaitForMineralsOrder < (currentFrame - 6) ||
                    !OrderProcessTimer::isResetFrame(nextMiningWorker->lastTransitionedToWaitForMineralsOrder + 1)))
            {
                fullySaturated = true;
                return;
            }

            auto initializeMiningWorker = [&]<WorkerOrdering workerOrdering>()
            {
                // Fill the arrays up to the earliest mining end frame
                std::fill_n(start.begin(), earliestMiningEndFrame - currentFrame, 1.0);
                std::fill_n(end.begin(), earliestMiningEndFrame - currentFrame - 1, 1.0);
                if constexpr (workerOrdering == WorkerOrdering::MiningWorkerFirst)
                {
                    std::fill_n(middle.begin(), earliestMiningEndFrame - currentFrame - 1, 1.0);
                }
                else if constexpr (workerOrdering == WorkerOrdering::NextMiningWorkerFirst)
                {
                    std::fill_n(middle.begin(), earliestMiningEndFrame - currentFrame, 1.0);
                }

                // If the mining end frame is known, nothing else is needed
                if (earliestMiningEndFrame == *miningWorkerLatestEndFrame)
                {
                    return;
                }

                // There was an order process timer reset, so we need to generate a decaying probability of the patch being mined between the earliest
                // and latest end frames

                // Get the mining worker's possible order process timer values at the start of the next frame
                auto miningWorkerOrderProcessTimerStartOfNextFrame =
                    OrderProcessTimer::atStartOfNextFrame(currentFrame, miningWorker->possibleOrderProcessTimerValues);

                // Determine the possible order process timer values at the earliest mining end frame
                auto orderProcessTimerValues = OrderProcessTimer::atStartOfFrameAtDelta(
                    currentFrame + 1,
                    miningWorkerOrderProcessTimerStartOfNextFrame,
                    {},
                    {},
                    earliestMiningEndFrame - currentFrame - 1);

                // Refine the latest end frame based on the possible order process timer values (this might shift it one frame earlier)
                miningWorkerLatestEndFrame = earliestMiningEndFrame + *orderProcessTimerValues.rbegin();

                // Generate the decaying probability of mining ending at each frame using the possible order process timer values
                double step = 1.0 / (double)orderProcessTimerValues.size();
                double probabilityStart = 1.0;
                double probabilityEnd = 1.0;
                for (int frame = currentFrame + 1; frame <= *miningWorkerLatestEndFrame; ++frame)
                {
                    int arrayIdx = frame - currentFrame - 1;
                    if (arrayIdx >= PATCH_OCCUPIED_HORIZON) break;

                    if (orderProcessTimerValues.contains(frame - earliestMiningEndFrame))
                    {
                        probabilityEnd -= step;
                    }

                    start[arrayIdx] = probabilityStart;
                    end[arrayIdx] = probabilityEnd;
                    if constexpr (workerOrdering == WorkerOrdering::MiningWorkerFirst)
                    {
                        middle[arrayIdx] = probabilityEnd;
                    }
                    else if constexpr (workerOrdering == WorkerOrdering::NextMiningWorkerFirst)
                    {
                        middle[arrayIdx] = probabilityStart;
                    }

                    probabilityStart = probabilityEnd;
                }
            };

            if (nextMiningWorker)
            {
                if (miningWorker->orderProcessIndex > nextMiningWorker->orderProcessIndex)
                {
                    initializeMiningWorker.operator()<WorkerOrdering::MiningWorkerFirst>();
                }
                else if (miningWorker->orderProcessIndex < nextMiningWorker->orderProcessIndex)
                {
                    initializeMiningWorker.operator()<WorkerOrdering::NextMiningWorkerFirst>();
                }
                else
                {
                    initializeMiningWorker.operator()<WorkerOrdering::Other>();
                }
            }
            else
            {
                initializeMiningWorker.operator()<WorkerOrdering::Other>();
            }
        }

        // Handle the next mining worker transitioning to mining or patch lock
        if (nextMiningWorker && nextMiningWorker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            usePatchLockFrame(nextMiningWorker->lastTransitionedToWaitForMineralsOrder);
        }
    }

    void PatchOccupiedForecast::useTakeoverFrame(int frame)
    {
        if (!nextMiningWorker)
        {
            Log::Get() << "ERROR: Called useTakeoverFrame without a next mining worker being defined";
            return;
        }

        // Validate bounds
        int frameIdx = frame - startFrame; // Index of the action frame
        if (frameIdx >= GATHER_FORECAST_FRAMES) return;
        if (frameIdx < 0)
        {
            fullySaturated = true;
            return;
        }

        // This method is used to indicate that a worker will at the latest take over at the given frame
        // This means we can just set 1s in the appropriate arrays from that point onwards, taking into consideration that the action will happen one
        // frame later
        std::fill(end.begin() + frameIdx, end.end(), 1.0);
        std::fill(start.begin() + frameIdx + 1, start.end(), 1.0);
        if (miningWorker)
        {
            if (nextMiningWorker->orderProcessIndex > miningWorker->orderProcessIndex)
            {
                std::fill(middle.begin() + frameIdx, middle.end(), 1.0);
            }
            else if (nextMiningWorker->orderProcessIndex < miningWorker->orderProcessIndex)
            {
                std::fill(middle.begin() + frameIdx + 1, middle.end(), 1.0);
            }
        }
    }

    void PatchOccupiedForecast::usePatchLockFrame(int frame)
    {
        if (!nextMiningWorker)
        {
            Log::Get() << "ERROR: Called usePatchLockFrame without a next mining worker being defined";
            return;
        }

        // This method is used to indicate that a worker will patch lock at the given frame
        // The currently-mining worker may however already be finished with mining, in which case the worker will take over instead

        // Nothing is needed if we already know the patch is fully saturated
        if (fullySaturated) return;

        // An order process timer reset immediately after the patch lock will cause the worker to remain in the WaitForMinerals state until the new
        // order process timer cycle gets back to 0
        // Handle the non-reset case first since it is much simpler
        if (!OrderProcessTimer::isResetFrame(frame + 1))
        {
            // Calidating the bounds
            int frameIdx = frame - startFrame; // Index of the action frame
            if (frameIdx > GATHER_FORECAST_FRAMES) return;
            if (frameIdx < 0)
            {
                fullySaturated = true;
                return;
            }

            // This logic is pretty much the same as for takeover, but we check if the mining worker is definitely mining at the patch lock moment
            // and flag fully saturated if this is the case
            if (miningWorker)
            {
                if (nextMiningWorker->orderProcessIndex > miningWorker->orderProcessIndex)
                {
                    if (start[frameIdx] > 0.999)
                    {
                        fullySaturated = true;
                        return;
                    }

                    std::fill(middle.begin() + frameIdx, middle.end(), 1.0);
                }
                else if (nextMiningWorker->orderProcessIndex < miningWorker->orderProcessIndex)
                {
                    if (middle[frameIdx] > 0.999)
                    {
                        fullySaturated = true;
                        return;
                    }

                    std::fill(middle.begin() + frameIdx + 1, middle.end(), 1.0);
                }
                else
                {
                    if (end[frameIdx] > 0.999)
                    {
                        fullySaturated = true;
                        return;
                    }
                }
            }
            std::fill(end.begin() + frameIdx, end.end(), 1.0);
            std::fill(start.begin() + frameIdx + 1, start.end(), 1.0);
            return;
        }

        // There is a reset, so we generate an increasing probability of reaching the action frame

        double probability = 0.0;
        fullySaturated = true; // reset in the loop if we find a frame where it isn't true
        for (int f = frame + 1; f < (frame + 8); ++f)
        {
            probability += 0.125;

            int frameIdx = f - startFrame - 1;
            if (frameIdx < 0) continue;
            if (frameIdx >= PATCH_OCCUPIED_HORIZON) break;

            if (miningWorker && nextMiningWorker->orderProcessIndex > miningWorker->orderProcessIndex)
            {
                if (end[frameIdx] > 0.999) continue;

                middle[frameIdx] = probability + ((1.0 - probability) * middle[frameIdx]);
            }
            else if (miningWorker && nextMiningWorker->orderProcessIndex < miningWorker->orderProcessIndex)
            {
                if (middle[frameIdx] > 0.999) continue;

                if ((frameIdx + 1) < PATCH_OCCUPIED_HORIZON)
                {
                    middle[frameIdx + 1] = probability + ((1.0 - probability) * middle[frameIdx + 1]);
                }
            }
            else if (miningWorker && nextMiningWorker->orderProcessIndex == miningWorker->orderProcessIndex)
            {
                if (end[frameIdx] > 0.999) continue;
            }

            end[frameIdx] = probability + ((1.0 - probability) * end[frameIdx]);
            if ((frameIdx + 1) < PATCH_OCCUPIED_HORIZON)
            {
                start[frameIdx + 1] = probability + ((1.0 - probability) * start[frameIdx + 1]);
            }

            fullySaturated = false;
        }

        if (fullySaturated) return;

        // Fill the rest of the forecast from the frame where the worker is definitely going to have taken over
        useTakeoverFrame(frame + 8);
    }
}
