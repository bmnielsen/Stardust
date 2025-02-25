#include "WorkerGatherStatus.h"

#include "DebugFlag_WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    std::shared_ptr<PositionAndVelocity> WorkerGatherStatus::appendCurrentPosition()
    {
        lastProcessedFrame = currentFrame;

        std::shared_ptr<PositionAndVelocity> currentPosition;
        if (positionHistory.empty())
        {
            // For the first position, compute whether the path started at the depot
            // The logic for this is to treat it as starting at the depot if the distance to the depot was 0 or
            // the start position was the unit's spawn location (which is not quite at distance 0)
            // We exclude gathers from depots that are a long way away though to avoid tracking distance mining paths
            pathStartsAtDepot = (
                    (resource->getDistance(depot) < 256) && (
                            (depot->getDistance(worker) == 0) || (worker->lastPosition == worker->spawnPosition)));
        }

        currentPosition = std::make_shared<PositionAndVelocity>(worker);
        positionHistory.emplace_back(currentPosition);
        return currentPosition;
    }

    bool WorkerGatherStatus::sendGatherCommand(BWAPI::Unit resourceBwapiUnit, const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (!worker->gather(resourceBwapiUnit))
        {
#if OPTIMALPOSITIONS_DEBUG
            Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                       << BWAPI::Broodwar->getLastError();
            CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
            CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
            return false;
        }

        resentPositions.push_back(currentPosition);
        resentFrames.insert(currentFrame);
        return true;
    }
}
