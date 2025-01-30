#include "WorkerReturnStatus.h"

#include "DebugFlag_WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    std::shared_ptr<PositionAndVelocity> WorkerReturnStatus::appendCurrentPosition()
    {
        lastProcessedFrame = currentFrame;

        std::shared_ptr<PositionAndVelocity> currentPosition;
        if (positionHistory.empty())
        {
            // For the first position, compute whether the path started at the patch
            pathStartsAtPatch = resource && ((resource->getDistance(worker) == 0) && (resource->getDistance(depot) < 256));
            currentPosition = std::make_shared<PositionAndVelocity>(worker, nullptr);
        }
        else
        {
            // For subsequent positions, include hashes of the previous positions if the path started at the patch
            // This helps us detect when the worker reaches the same position via a different path, indicating different subpixel positioning
            currentPosition = std::make_shared<PositionAndVelocity>(
                    worker,
                    pathStartsAtPatch ? positionHistory.rbegin()->get() : nullptr);
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
