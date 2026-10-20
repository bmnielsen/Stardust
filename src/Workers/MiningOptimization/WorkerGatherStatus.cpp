#include "WorkerGatherStatus.h"

#include "DebugFlag_WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    std::shared_ptr<PositionAndVelocity> WorkerGatherStatus::appendCurrentPosition()
    {
        lastProcessedFrame = frameNow;

        auto currentPosition = std::make_shared<PositionAndVelocity>(wrk);

        /* For the first position, compute whether the path started at the depot */
        /* We exclude gathers from depots that are a long way away though to avoid tracking distance mining paths */
        if (positionHistory.empty())
        {
            pathStartsAtDepot = ((resource->getDistance(depot) < 256) && ((depot->getDistance(wrk) == 0) || pathStartsAtSpawnPosition));
        }

        /* If we haven't observed leaving the depot yet, check if this position fulfills this */
        /* We consider the worker to have left the depot if it's previous position is at least 2 pixels from its initial position, and it's */
        /* current position is different from its previous position */
        if (!hasLeftDepot && !positionHistory.empty())
        {
            hasLeftDepot = (*positionHistory.begin())->pt().getApproxDistance((*positionHistory.rbegin())->pt()) >= 2
                           && (*positionHistory.rbegin())->pt() != currentPosition->pt();
        }

        positionHistory.emplace_back(currentPosition);
        return currentPosition;
    }

    bool WorkerGatherStatus::sendGatherCommand(BWAPI::Unit resourceBwapiUnit, const std::shared_ptr<PositionAndVelocity> &currentPosition)
    {
        if (!wrk->gather(resourceBwapiUnit))
        {
#if OPTIMALPOSITIONS_DEBUG
            AppLog::Get() << "Failed to send gather command for " << wrk->ident << " @ " << wrk->getTilePosition() << ": "
                       << BWAPI::Broodwar->getLastError();
            Telemetry::log(wrk->ident) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
            Telemetry::log(resource->ident) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
            return false;
        }

        resentPositions.push_back(currentPosition);
        resentFrames.insert(frameNow);
        return true;
    }
}
