#include "Resource.h"

#include "Geo.h"
#include "Workers.h"
#include "OrderProcessTimer.h"
#include "MiningOptimization/WorkerMiningOptimization.h"

#define EPSILON 0.000001

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_SATURATION_DATA false
#endif

ResourceImpl::ResourceImpl(BWAPI::Unit unit)
    : id(unit->getID())
    , isMinerals(unit->getType().isMineralField())
    , tile(unit->getTilePosition())
    , center(unit->getPosition())
    , initialAmount(unit->getResources())
    , currentAmount(unit->getResources())
    , seenLastFrame(false)
    , destroyed(false)
    , bwapiUnit(unit)
    , gatherProbabilityForecast({0.0})
    , gatherProbabilityForecastUpdated(-2)
    , allOtherPatchesGatheredProbabilityForecast({0.0})
    , allOtherPatchesGatheredProbabilityForecastUpdated(-2)
{}

bool ResourceImpl::hasMyCompletedRefinery() const
{
    if (isMinerals) return false;
    if (!refinery) return false;
    if (!refinery->completed) return false;
    if (refinery->player != BWAPI::Broodwar->self()) return false;

    return true;
}

BWAPI::Unit ResourceImpl::getBwapiUnitIfVisible() const
{
    if (refinery && refinery->bwapiUnit && refinery->bwapiUnit->isVisible())
    {
        return refinery->bwapiUnit;
    }

    if (bwapiUnit && bwapiUnit->exists() && bwapiUnit->isVisible())
    {
        return bwapiUnit;
    }

    bwapiUnit = nullptr;
    for (auto unit : BWAPI::Broodwar->getNeutralUnits())
    {
        if (unit->getTilePosition() != tile) continue;
        if (isMinerals && !unit->getType().isMineralField()) continue;
        if (!isMinerals && unit->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;
        if (!unit->isVisible()) continue;

        bwapiUnit = unit;
        break;
    }

    return bwapiUnit;
}

int ResourceImpl::getDistance(const Unit &unit) const
{
    return Geo::EdgeToEdgeDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            unit->type,
            unit->lastPosition);
}

int ResourceImpl::getDistance(BWAPI::Position pos) const
{
    return Geo::EdgeToPointDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            pos);
}

int ResourceImpl::getDistance(const Resource &other) const
{
    return Geo::EdgeToEdgeDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            other->isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            other->center);
}

int ResourceImpl::getDistance(BWAPI::UnitType otherType, BWAPI::Position otherCenter) const
{
    return Geo::EdgeToEdgeDistance(
            isMinerals ? BWAPI::UnitTypes::Resource_Mineral_Field : BWAPI::UnitTypes::Resource_Vespene_Geyser,
            center,
            otherType,
            otherCenter);
}

std::array<double, GATHER_FORECAST_FRAMES> &ResourceImpl::getAllOtherPatchesGatheredProbabilityForecast()
{
    if (allOtherPatchesGatheredProbabilityForecastUpdated == currentFrame)
    {
        return allOtherPatchesGatheredProbabilityForecast;
    }

    // The probability of all other patches being mined at the start of a given frame is found by multiplying all of the other vectors together
    std::fill(allOtherPatchesGatheredProbabilityForecast.begin(), allOtherPatchesGatheredProbabilityForecast.end(), 1.0);
    for (auto &patch : resourcesInSwitchPatchRange)
    {
        if (patch->destroyed) continue;

        std::transform(allOtherPatchesGatheredProbabilityForecast.begin(),
                       allOtherPatchesGatheredProbabilityForecast.end(),
                       patch->getGatherProbabilityForecast().begin(),
                       allOtherPatchesGatheredProbabilityForecast.begin(),
                       std::multiplies<>{});
    }

    // The above only gives us the probability at the end of the frame, but if we want to be sure all other patches are mined when a worker's
    // orders are processed, we need to consider the fact that other workers' orders are likely to have been processed before it.
    // The simplest way to fix this is to include the probability of the other patches also being mined on the previous frame. This takes into account
    // any patches that are forecasted to start being mined on the frame when our worker's orders are processed.
    // A more accurate solution to this would actually consider the order process index, but this would require tracking a lot of additional data
    // and would likely make very little difference in practice.
    // The first frame in the forecast can not be multiplied by the next, so we leave it as-is, but as a frame that early would never be usable
    // for planning (because of latency), this shouldn't be an issue.
    for (int i = 1; i < GATHER_FORECAST_FRAMES; i++)
    {
        allOtherPatchesGatheredProbabilityForecast[i] *= allOtherPatchesGatheredProbabilityForecast[i - 1];
    }

#if DEBUG_SATURATION_DATA
    std::ostringstream debug;
    debug << std::fixed << std::setprecision(2) << "other patches forecast: ";
    std::string sep;
    for (int i = 0; i < std::min(10, GATHER_FORECAST_FRAMES); i++)
    {
        debug << sep << allOtherPatchesGatheredProbabilityForecast[i];
        sep = ", ";
    }
    CherryVis::log(id) << debug.str();
#endif

    allOtherPatchesGatheredProbabilityForecastUpdated = currentFrame;
    return allOtherPatchesGatheredProbabilityForecast;
}

std::array<double, GATHER_FORECAST_FRAMES> &ResourceImpl::getGatherProbabilityForecast()
{
    if (gatherProbabilityForecastUpdated == currentFrame)
    {
        return gatherProbabilityForecast;
    }

    auto returner = [&]() -> std::array<double, GATHER_FORECAST_FRAMES>&
    {
#if DEBUG_SATURATION_DATA
        std::ostringstream debug;
        debug << std::fixed << std::setprecision(2) << "this patch forecast: ";
        std::string sep;
        for (int i = 0; i < std::min(10, GATHER_FORECAST_FRAMES); i++)
        {
            debug << sep << gatherProbabilityForecast[i];
            sep = ", ";
        }
        CherryVis::log(id) << debug.str();
#endif

        gatherProbabilityForecastUpdated = currentFrame;
        return gatherProbabilityForecast;
    };

    // Get the mining worker and the next mining worker, either or both of which may be null
    MyWorker miningWorker;
    MyWorker nextMiningWorker;
    for (auto &worker : Workers::getWorkersAssignedTo(shared_from_this()))
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
        if (!nextMiningWorker || getDistance(worker) < getDistance(nextMiningWorker))
        {
            nextMiningWorker = worker;
        }
    }

    // Overview of logic:
    // No workers assigned:
    // - The patch will not be mined over the entire forecast horizon
    // A worker is mining:
    // - The patch is mined until between 75 and 82 frames after starting depending on whether there was an order process timer reset
    // - If an order process timer reset affects the timing, we generate the decaying probability at end of mining
    // A worker is approaching:
    // - The patch will be mined from the worker's expected mining start frame, if we have predicted that with path data in our optimizer
    // - In the case of patch locking on takeover, the patch will be mined for the entire forecast horizon

    // If the next mining worker is actively transitioning to mining, we can just write 1s for the entire forecast horizon
    if (nextMiningWorker && nextMiningWorker->lastTransitionedToWaitForMineralsOrder == currentFrame
        && !OrderProcessTimer::isResetFrame())
    {
        std::fill(gatherProbabilityForecast.begin(), gatherProbabilityForecast.end(), 1.0);
        return returner();
    }

    // Start with zeroes
    std::fill(gatherProbabilityForecast.begin(), gatherProbabilityForecast.end(), 0.0);

    // If there is a mining worker, fill in its data
    if (miningWorker)
    {
        // If the worker has transitioned to the mining order but hasn't actually started decrementing its mining timer yet, just mark
        // the patch as occupied for the entire forecast horizon and call it a day
        if (miningWorker->lastStartedMining < miningWorker->lastTransitionedToMiningOrder)
        {
            std::fill(gatherProbabilityForecast.begin(), gatherProbabilityForecast.end(), 1.0);
            return returner();
        }

        // Compute the mining end frame if there was no order timer reset
        int miningEndFrame = miningWorker->lastStartedMining + 81;

        // If there was an order timer reset after the start of mining, the worker may end mining between frame 74 and 81
        int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(miningEndFrame - 1);
        if (previousOrderTimerReset >= miningWorker->lastStartedMining)
        {
            int earliestMiningEndFrame = miningWorker->lastStartedMining + 75;
            int possibleOrderProcessTimerValues = 9;

            // If the reset happens after the mining timer expires, the earliest end frame is advanced to the reset point
            if (previousOrderTimerReset > earliestMiningEndFrame)
            {
                earliestMiningEndFrame = previousOrderTimerReset + 1;
                possibleOrderProcessTimerValues = 8; // At reset it will get a value of 0-7 inclusive
            }

            // Fill the array up to the earliest end frame to indicate that the patch is definitely being mined until that point
            if (earliestMiningEndFrame > currentFrame)
            {
                std::fill_n(gatherProbabilityForecast.begin(), std::min(earliestMiningEndFrame - currentFrame - 1, GATHER_FORECAST_FRAMES), 1.0);
            }
            else
            {
                // Mining could have ended by now, but it hasn't, which limits the possible order process timer values
                possibleOrderProcessTimerValues -= (currentFrame - earliestMiningEndFrame + 1);
                earliestMiningEndFrame = currentFrame + 1;
            }

            // Set a decaying probability from here
            for (int i = 0; i < possibleOrderProcessTimerValues; i++)
            {
                int arrayIdx = earliestMiningEndFrame + i - currentFrame - 1;
                if (arrayIdx >= GATHER_FORECAST_FRAMES) break;

                gatherProbabilityForecast[arrayIdx] = 1.0 - ((double)(i + 1) / (double)possibleOrderProcessTimerValues);
            }
        }
        else
        {
            std::fill_n(gatherProbabilityForecast.begin(), std::min(miningEndFrame - currentFrame - 1, GATHER_FORECAST_FRAMES), 1.0);
        }
    }

    // Jump out if there is no next mining worker
    if (!nextMiningWorker) return returner();

    // We can only predict if we have gather status data
    auto gatherStatus = WorkerMiningOptimization::gatherStatusFor(nextMiningWorker);
    if (!gatherStatus || gatherStatus->resource.get() != this)
    {
#if DEBUG_SATURATION_DATA
        CherryVis::log(nextMiningWorker->id) << "No gather status; not using this for mining forecast";
#endif
        return returner();
    }

    // Save the probability for the next frame from the currently mining worker
    double miningWorkerNextFrameProbability = gatherProbabilityForecast[0];

    // Two worker takeover case
    if (gatherStatus->takeoverFrame != -1)
    {
        // Special case when the takeover worker is expected to patch lock on a given frame
        int patchLockFrame = gatherStatus->actualPatchLockFrame;
        if (patchLockFrame == -1) patchLockFrame = gatherStatus->expectedPatchLockFrame;
        if (patchLockFrame != -1)
        {
            // There are three high-level scenarios:
            // - Patch lock has already happened -> mining will continue with no delay
            // - Patch lock will happen before the mining worker finishes mining -> mining will continue with no delay
            // - "Patch lock" may be after previous worker finishes mining -> mining will start as on usual takeover (with one WaitForMinerals frame)
            // The first two cases are easy, as the patch lock frame already has a 1 from the mining worker (or is before the forecast window), so
            // we can write 1s from the frame after the patch lock frame, which will overwrite any data already there from the mining worker.
            // In the last case we can also always write 1s from the frame after the patch lock frame, as we are sure mining will happen by then.
            // However, it gets more complicated if the mining worker might finish mining on the same frame as the patch lock. If the new worker has
            // its orders processed first, it may lock before the other worker is processed and therefore continue mining immediately.

            // Start by filling the 1s from the frame after the patch lock frame
            int frameAfterLockIndex = std::max(patchLockFrame - currentFrame, 0);
            std::fill_n(gatherProbabilityForecast.begin() + frameAfterLockIndex, GATHER_FORECAST_FRAMES - frameAfterLockIndex, 1.0);

            // If we are in the situation where the mining worker might finish on the patch lock frame, and the next worker has its orders processed
            // first, the probability for that frame becomes the probability that the mining worker was mining on the previous frame
            if (frameAfterLockIndex > 0 && miningWorker && nextMiningWorker->orderProcessIndex > miningWorker->orderProcessIndex)
            {
                gatherProbabilityForecast[frameAfterLockIndex - 1] = (frameAfterLockIndex == 1)
                                                                     ? 1.0
                                                                     : gatherProbabilityForecast[frameAfterLockIndex - 2];
            }
            return returner();
        }

        // We require the expected mining start frame to be in the future, otherwise we guessed wrong and leave the forecast with 0s
        if (gatherStatus->expectedMiningStartFrame > currentFrame)
        {
            // If the expected mining start frame is at or before the current frame, we guessed wrong and don't write any data
            int miningStartIndex = gatherStatus->expectedMiningStartFrame - currentFrame - 1;

            std::fill_n(gatherProbabilityForecast.begin() + miningStartIndex, GATHER_FORECAST_FRAMES - miningStartIndex, 1.0);
        }

        return returner();
    }

    // Single worker, so we can only predict anything if we have at least one expected arrival frame
    if (gatherStatus->expectedArrivalFrameAndOccurrenceRate.empty()) return returner();

    // We process each potential arrival frame separately and aggregate to get the final frame probabilities
    auto processArrivalFrame = [&gatherStatus, &nextMiningWorker](
            int arrivalFrame, double probability, std::array<double, GATHER_FORECAST_FRAMES> &forecast)
    {
        // Try to predict the value of the order process timer at the arrival frame in order to compute the mining start frame
        int frameWithKnownOrderProcessTimer;
        int knownOrderProcessTimerValue;
        int deltaToArrivalFrame;
        int lastResendFrame = gatherStatus->lastResendFrameIncludingPlanned();
        if (lastResendFrame != -1)
        {
            frameWithKnownOrderProcessTimer = lastResendFrame + BWAPI::Broodwar->getLatencyFrames();
            knownOrderProcessTimerValue = 10;
            deltaToArrivalFrame = arrivalFrame - frameWithKnownOrderProcessTimer;

#if DEBUG_SATURATION_DATA
            if (gatherStatus->expectedPath.empty())
            {
                CherryVis::log(nextMiningWorker->id) << "After last resend"
                                                     << "; frameWithKnownOrderProcessTimer=" << frameWithKnownOrderProcessTimer
                                                     << "; deltaToArrivalFrame=" << deltaToArrivalFrame;
            }
            else
            {
                CherryVis::log(nextMiningWorker->id) << "On path"
                                                     << "; frameWithKnownOrderProcessTimer=" << frameWithKnownOrderProcessTimer
                                                     << "; deltaToArrivalFrame=" << deltaToArrivalFrame;
            }
#endif
        }
        else
        {
            // No resends, order process timer is used if known
            frameWithKnownOrderProcessTimer = currentFrame;
            knownOrderProcessTimerValue = nextMiningWorker->orderProcessTimer;
            deltaToArrivalFrame = arrivalFrame - currentFrame;
#if DEBUG_SATURATION_DATA
            CherryVis::log(nextMiningWorker->id) << "No resend";
#endif
        }

        auto orderProcessTimerAtArrival = OrderProcessTimer::unitOrderProcessTimerAtDelta(
                frameWithKnownOrderProcessTimer,
                knownOrderProcessTimerValue,
                deltaToArrivalFrame - 1);

#if DEBUG_SATURATION_DATA
        CherryVis::log(nextMiningWorker->id) << "Predicted order process timer at arrival: " << orderProcessTimerAtArrival;
#endif

        // There are two sets of scenarios that happen here in parallel:
        // - The order process timer at arrival may be known or unknown
        // - The order process timer may reset after arrival, changing the timings further

        // We start by setting the probabilities under the assumption that there is no order process timer reset after arrival
        if (orderProcessTimerAtArrival != -1)
        {
            // We know the order process timer at arrival, so fill the array from the mining start frame, unless it is in the past
            // (in which case we were wrong about something)
            auto expectedMiningStartFrame = arrivalFrame + orderProcessTimerAtArrival + 1;
            if (expectedMiningStartFrame > currentFrame)
            {
                auto startIndex = expectedMiningStartFrame - currentFrame - 1;
                if (startIndex < GATHER_FORECAST_FRAMES)
                {
                    std::fill_n(forecast.begin() + startIndex, GATHER_FORECAST_FRAMES - startIndex, probability);
                }
            }
        }
        else
        {
            // We don't know the order process timer at arrival, so we generate an increasing probability
            int earliestMiningStartFrame = arrivalFrame + 1;
            int possibleOrderProcessTimerValues = 9;

            // If we have arrived, the number of possible order process timer values left is lower
            if (earliestMiningStartFrame <= currentFrame)
            {
                possibleOrderProcessTimerValues -= (currentFrame - earliestMiningStartFrame + 1);
                earliestMiningStartFrame = currentFrame + 1;
                if (possibleOrderProcessTimerValues < 0)
                {
                    Log::Get() << "ERROR: earliest mining start frame is way before current frame, something is wrong here"
                               << "; worker " << nextMiningWorker->id << " @ " << nextMiningWorker->getTilePosition();
                    return;
                }
            }

            // If the worker has arrived but is not in WaitForMinerals, we know it won't be mining next frame either
            if (arrivalFrame <= currentFrame && nextMiningWorker->bwapiUnit->getOrder() != BWAPI::Orders::WaitForMinerals)
            {
                possibleOrderProcessTimerValues--;
                earliestMiningStartFrame++;
            }

            // Generate the increasing probabilities
            for (int i = 0; i < possibleOrderProcessTimerValues; i++)
            {
                int arrayIdx = earliestMiningStartFrame + i - currentFrame - 1;
                if (arrayIdx >= GATHER_FORECAST_FRAMES) break;

                forecast[arrayIdx] = probability * ((double)(i + 1) / (double)possibleOrderProcessTimerValues);
            }

            // Fill with 1s after we are sure the worker has started mining
            int latestMiningStartIndex = earliestMiningStartFrame + possibleOrderProcessTimerValues - currentFrame - 1;
            if (latestMiningStartIndex < GATHER_FORECAST_FRAMES)
            {
                std::fill_n(forecast.begin() + latestMiningStartIndex, GATHER_FORECAST_FRAMES - latestMiningStartIndex, probability);
            }
        }

        // Now handle the case where an order process timer reset happens after the worker arrives at the patch but potentially before
        // it starts mining.
        // There are two ways this can take effect:
        // - The reset happens while the worker is still in the MoveToMinerals order, so it has to transition to WaitForMinerals before mining
        // - The reset happens on the frame the worker should have transitioned from WaitForMinerals to mining
        // We consider both cases by looking at the mining probability we have computed beforehand.

        // First compute the index in the forecast array where the reset happens
        int orderTimerResetIndex = OrderProcessTimer::nextResetFrame(arrivalFrame) - currentFrame;
        if (orderTimerResetIndex >= GATHER_FORECAST_FRAMES) return; // reset is outside of forecast horizon

        // Get the probability that the worker will already have started mining at the reset
        // This requires the worker to have already transitioned to mining the frame before
        double miningProbabilityAtReset = 0.0;
        if (orderTimerResetIndex > 0) miningProbabilityAtReset = forecast[orderTimerResetIndex - 1] / probability;

        // If we are projecting the worker to definitely be mining at the reset, nothing further is needed
        if (miningProbabilityAtReset > (1.0 - EPSILON)) return;

        // Get the probability that the worker will be in WaitForMinerals at the reset
        // This is equivalent to our previously-calculated probability that the worker would be mining on the reset frame, minus the
        // probability that it was already mining
        double waitForMineralsProbabilityAtReset = 0.0;
        if (orderTimerResetIndex >= 0) waitForMineralsProbabilityAtReset = (forecast[orderTimerResetIndex] / probability) - miningProbabilityAtReset;

        // If it is at or after the reset frame, the worker may already be in WaitForMinerals
        if (orderTimerResetIndex <= 0 && nextMiningWorker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            waitForMineralsProbabilityAtReset = 1.0;
        }

        // At reset, the worker can get 8 possible order process timer values from 0-7 inclusive
        int possibleOrderProcessTimerValues = 8;

        // If we are after the reset, the number of possible values left decreases, as we would have already transitioned to mining with
        // some of the values
        if (orderTimerResetIndex < 0)
        {
            possibleOrderProcessTimerValues += orderTimerResetIndex;
            orderTimerResetIndex = 0;
        }

        // Now generate the increasing probabilities
        for (int i = 0; i < possibleOrderProcessTimerValues; i++)
        {
            int arrayIdx = orderTimerResetIndex + i;
            if (arrayIdx >= GATHER_FORECAST_FRAMES) break;

            double baseProbability = probability *
                    (miningProbabilityAtReset + (1.0 - miningProbabilityAtReset) * (double)(i + 1) / (double)possibleOrderProcessTimerValues);

            if (i == 0)
            {
                forecast[arrayIdx] = waitForMineralsProbabilityAtReset * baseProbability;
            }
            else
            {
                forecast[arrayIdx] += waitForMineralsProbabilityAtReset * baseProbability;
            }

            arrayIdx++;
            if (arrayIdx >= GATHER_FORECAST_FRAMES) break;
            forecast[arrayIdx] = (1.0 - waitForMineralsProbabilityAtReset) * baseProbability;
        }

#if DEBUG_SATURATION_DATA
        CherryVis::log(nextMiningWorker->id) << "Order timer reset after arrival"
                                                 << "; arrivalFrame=" << arrivalFrame
                                                 << "; resetFrame=" << OrderProcessTimer::nextResetFrame(arrivalFrame)
                                                 << "; possibleOrderProcessTimerValues=" << possibleOrderProcessTimerValues
                                                 << "; miningProbabilityAtReset=" << miningProbabilityAtReset
                                                 << "; waitForMineralsProbabilityAtReset=" << waitForMineralsProbabilityAtReset;
#endif
    };

    // If there is only one expected arrival frame, we can write directly into the result array
    if (gatherStatus->expectedArrivalFrameAndOccurrenceRate.size() == 1)
    {
        processArrivalFrame(gatherStatus->expectedArrivalFrameAndOccurrenceRate.begin()->first, 1.0, gatherProbabilityForecast);
    }
    else
    {
        // Otherwise we process each arrival frame into its own array and add it into the overall forecast array
        for (const auto &[arrivalFrame, occurrenceRate] : gatherStatus->expectedArrivalFrameAndOccurrenceRate)
        {
            std::array<double, GATHER_FORECAST_FRAMES> thisForecast = {0.0};
            processArrivalFrame(arrivalFrame, (double)occurrenceRate / 255.0, thisForecast);
            std::transform(gatherProbabilityForecast.begin(),
                           gatherProbabilityForecast.end(),
                           thisForecast.begin(),
                           gatherProbabilityForecast.begin(),
                           std::plus<>{});
        }

        // Clamp the values at 1.0
        std::transform(gatherProbabilityForecast.begin(),
                       gatherProbabilityForecast.end(),
                       gatherProbabilityForecast.begin(),
                       [](double d)
                       { return std::min(d, 1.0); });
    }

    // We know the next mining worker won't be mining next frame if it isn't in WaitForMinerals
    if (nextMiningWorker->bwapiUnit->getOrder() != BWAPI::Orders::WaitForMinerals)
    {
        gatherProbabilityForecast[0] = miningWorkerNextFrameProbability;
    }

    return returner();
}

std::ostream &operator<<(std::ostream &os, const ResourceImpl &resource)
{
    if (resource.isMinerals)
    {
        os << "Minerals";
    }
    else
    {
        os << "Gas";
    }

    os << ":" << resource.id << "@" << BWAPI::WalkPosition(resource.center);

    if (resource.destroyed)
    {
        os << " (destroyed)";
    }
    else
    {
        if (!resource.refinery || resource.refinery->player == BWAPI::Broodwar->self())
        {
            os << " " << resource.currentAmount << "/" << resource.initialAmount;
        }
        if (resource.refinery)
        {
            os << " with refinery " << *resource.refinery;
        }
    }

    return os;
}
