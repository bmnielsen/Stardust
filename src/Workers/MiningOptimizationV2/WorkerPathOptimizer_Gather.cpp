#include "WorkerPathOptimizer.h"

#include "Workers.h"

namespace MiningOptimization
{
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
            worker->gather(resourceBwapiUnit);

            // TODO: Update relevant pathing status
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

                    // TODO: Check if it matches expected frame

#if VERBOSE_MINING_LOGGING
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
