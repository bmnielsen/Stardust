#include "WorkerReturnStatus.h"

#include "DebugFlag_WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    std::shared_ptr<PositionAndVelocity> WorkerReturnStatus::appendCurrentPosition()
    {
        lastProcessedFrame = frameNow;

        auto currentPosition = std::make_shared<PositionAndVelocity>(wrk);

        /* For the first position, compute whether the path started at the patch */
        if (positionHistory.empty())
        {
            pathStartsAtPatch = resource && ((resource->getDistance(wrk) == 0) && (resource->getDistance(depot) < 256));
        }

        /* If we haven't observed leaving the patch yet, check if this position fulfills this */
        /* We consider the worker to have left the patch if it's previous position is at least 2 pixels from its initial position, and it's */
        /* current position is different from its previous position */
        if (!hasLeftPatch && !positionHistory.empty())
        {
            hasLeftPatch = (*positionHistory.begin())->pt().getApproxDistance((*positionHistory.rbegin())->pt()) >= 2
                           && (*positionHistory.rbegin())->pt() != currentPosition->pt();
        }

        positionHistory.emplace_back(currentPosition);
        return currentPosition;
    }

    void WorkerReturnStatus::sendReturnCommand(const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (!wrk->returnCargo())
        {
#if LOGGING_ENABLED
            AppLog::Get() << "Failed to send return command for " << wrk->ident << " @ " << wrk->getTilePosition() << ": "
                       << BWAPI::Broodwar->getLastError();
            Telemetry::log(wrk->ident) << "Failed to send return command; last error " << BWAPI::Broodwar->getLastError();
            if (resource) Telemetry::log(resource->ident) << "Failed to send return command; last error " << BWAPI::Broodwar->getLastError();
#endif
            return;
        }

        resentPosition = currentPosition;
    }
}
