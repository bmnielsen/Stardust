#include "Resource.h"

#include "Geo.h"
#include "Workers.h"
#include "OrderProcessTimer.h"
#include "MiningOptimization/WorkerMiningOptimization.h"

#define DEBUG_SATURATION_DATA true

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
    , gatherProbabilityForecast({})
    , gatherProbabilityForecastUpdated(-2)
    , allOtherPatchesGatheredProbabilityForecast({})
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

    // The probability is found by multiplying all of the other vectors together
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

    // Get the mining worker and the next mining worker, either or both of which may be null
    MyWorker miningWorker;
    MyWorker nextMiningWorker;
    for (auto &worker : Workers::getWorkersAssignedTo(shared_from_this()))
    {
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

    // Start with zeroes
    std::fill(gatherProbabilityForecast.begin(), gatherProbabilityForecast.end(), 0.0);

    // If there is a mining worker, fill in its data
    if (miningWorker)
    {
        // Compute the mining end frame if there was no order timer reset
        int miningEndFrame = miningWorker->lastStartedMining + 81;

        // If there was an order timer reset after the start of mining, the worker may end mining between frame 74 and 81
        int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(miningEndFrame - 1);
        if (previousOrderTimerReset >= miningWorker->lastStartedMining)
        {
            int earliestMiningEndFrame = miningWorker->lastStartedMining + 74;

            // If the reset happens after the mining timer expires, the earliest end frame is advanced to the reset point
            if (previousOrderTimerReset > earliestMiningEndFrame)
            {
                earliestMiningEndFrame = previousOrderTimerReset;
            }

            // Fill the array up to the earliest end frame to indicate that the patch is definitely being mined
            if (earliestMiningEndFrame > currentFrame)
            {
                std::fill_n(gatherProbabilityForecast.begin(), std::min(earliestMiningEndFrame - currentFrame - 1, GATHER_FORECAST_FRAMES), 1.0);
            }

            // Set a decaying probability from here
            for (int i = 0; i < 8; i++)
            {
                int arrayIdx = earliestMiningEndFrame + i - currentFrame - 1;
                if (arrayIdx < 0) continue;
                if (arrayIdx >= GATHER_FORECAST_FRAMES) break;

                gatherProbabilityForecast[arrayIdx] = 1.0 - (double)i / 8.0;
            }
        }
        else
        {
            std::fill_n(gatherProbabilityForecast.begin(), std::min(miningEndFrame - currentFrame - 1, GATHER_FORECAST_FRAMES), 1.0);
        }
    }

    // If there is a next mining worker, fill in its data
    if (nextMiningWorker)
    {
        // We can only predict if we have gather status data
        auto gatherStatus = WorkerMiningOptimization::gatherStatusFor(nextMiningWorker);
        if (gatherStatus)
        {
            // We will treat the start frame as either the frame the worker is expected to patch lock or the frame the worker will start mining
            // In the patch lock case, this isn't actually the frame the worker will start mining, but it will take over from the current one
            // with no delay, so we can just overwrite the current worker's data with 1s
            int startIndex = GATHER_FORECAST_FRAMES;
            if (gatherStatus->expectedPatchLockFrame != -1)
            {
                // Patch lock might have already happened, so if it was at or before the current frame we just clamp the index to 0
                startIndex = std::max(gatherStatus->expectedPatchLockFrame - currentFrame - 1, 0);
            }
            else if (gatherStatus->expectedMiningStartFrame > currentFrame)
            {
                // If the expected mining start frame is at or before the current frame, we guessed wrong and don't write any data
                startIndex = gatherStatus->expectedMiningStartFrame - currentFrame - 1;
            }

            if (startIndex < GATHER_FORECAST_FRAMES)
            {
                std::fill_n(gatherProbabilityForecast.begin() + startIndex, GATHER_FORECAST_FRAMES - startIndex, 1.0);
            }
        }
    }

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
