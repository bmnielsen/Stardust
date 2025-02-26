#include "WorkerReturnStatus.h"

#include "DebugFlag_WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    std::shared_ptr<PositionAndVelocity> WorkerReturnStatus::appendCurrentPosition()
    {
        lastProcessedFrame = currentFrame;

        auto currentPosition = std::make_shared<PositionAndVelocity>(worker);

        // For the first position, compute whether the path started at the patch
        if (positionHistory.empty())
        {
            pathStartsAtPatch = resource && ((resource->getDistance(worker) == 0) && (resource->getDistance(depot) < 256));
        }

        // If we haven't observed leaving the patch yet, check if this position fulfills this
        // We consider the worker to have left the patch if it's previous position is at least 2 pixels from its initial position, and it's
        // current position is different from its previous position
        if (!hasLeftPatch && !positionHistory.empty())
        {
            hasLeftPatch = (*positionHistory.begin())->pos().getApproxDistance((*positionHistory.rbegin())->pos()) >= 2
                           && (*positionHistory.rbegin())->pos() != currentPosition->pos();
        }

        positionHistory.emplace_back(currentPosition);
        return currentPosition;
    }

    void WorkerReturnStatus::sendReturnCommand(const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (!worker->returnCargo())
        {
#if LOGGING_ENABLED
            Log::Get() << "Failed to send return command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                       << BWAPI::Broodwar->getLastError();
            CherryVis::log(worker->id) << "Failed to send return command; last error " << BWAPI::Broodwar->getLastError();
            if (resource) CherryVis::log(resource->id) << "Failed to send return command; last error " << BWAPI::Broodwar->getLastError();
#endif
            return;
        }

        resentPosition = currentPosition;
    }
}
