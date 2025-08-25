#include "Resource.h"

#include "Geo.h"
#include "Workers.h"
#include "OrderProcessTimer.h"

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
    , gatherProbabilityForecastUpdated(-1)
    , allOtherPatchesGatheredProbabilityForecast({})
    , allOtherPatchesGatheredProbabilityForecastUpdated(-1)
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
    allOtherPatchesGatheredProbabilityForecastUpdated = currentFrame;

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

    return allOtherPatchesGatheredProbabilityForecast;
}

std::array<double, GATHER_FORECAST_FRAMES> &ResourceImpl::getGatherProbabilityForecast()
{
    if (gatherProbabilityForecastUpdated == currentFrame)
    {
        return gatherProbabilityForecast;
    }
    gatherProbabilityForecastUpdated = currentFrame;

    // Get the mining worker and the next mining worker, either or both of which may be null
    MyWorker miningWorker;
    MyWorker nextMiningWorker;
    for (auto &worker : Workers::getWorkersAssignedTo(shared_from_this()))
    {
        // Don't consider workers returning, since we currently don't have the capability to simulate when they will get back to the patch
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

    // TODO: Track forecast error

    // There are a few possibilities:
    // Worker is mining and next worker is expected to be patch-locked before it finishes:
    // - patch is saturated for the entire forecast horizon
    // Worker is mining and next worker will take over without being locked:
    // - mining worker will finish between 75 and 83 frames after starting
    // - take over worker will take over based on its approach optimization
    // No worker is mining and next worker is approaching:
    // - mining will start based on the approach optimization
    // Most of the above may be affected by order timer resets

    // Current logic is only setting it based on when the mining worker will finish
    // TODO: Implement other cases
    std::fill(gatherProbabilityForecast.begin(), gatherProbabilityForecast.end(), 0.0);
    if (miningWorker)
    {
        // Compute the mining end frame if there was no order timer reset
        int miningEndFrame = miningWorker->lastStartedMining + 80;

        // If there was an order timer reset after the start of mining, the worker may end mining between frame 75 and 83
        int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(miningEndFrame);
        if (previousOrderTimerReset >= miningWorker->lastStartedMining)
        {
            int firstMiningEndFrame = miningWorker->lastStartedMining + 75;
            if (firstMiningEndFrame > currentFrame)
            {
                std::fill_n(gatherProbabilityForecast.begin(), std::min(firstMiningEndFrame - currentFrame, GATHER_FORECAST_FRAMES), 1.0);
            }

            for (int i = 0; i < 8; i++)
            {
                int arrayIdx = firstMiningEndFrame + i - currentFrame;
                if (arrayIdx < 0) continue;
                if (arrayIdx >= GATHER_FORECAST_FRAMES) break;

                gatherProbabilityForecast[arrayIdx] = 1.0 - (double)i / 8.0;
            }
        }
        else
        {
            std::fill_n(gatherProbabilityForecast.begin(), std::min(miningEndFrame - currentFrame, GATHER_FORECAST_FRAMES), 1.0);
        }
    }

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
