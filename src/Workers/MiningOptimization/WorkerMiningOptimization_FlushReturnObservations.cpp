// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing return of resources

#include "WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        // Used to track whether a worker that has just returned resources had a collision, had a normal return, or kept its speed
        struct JustReturnedWorker
        {
            MyWorker worker;
            Resource resource;
            std::vector<std::shared_ptr<const PositionAndVelocity>> positionHistory;
            std::shared_ptr<const PositionAndVelocity> resentPosition;
        };

        std::vector<JustReturnedWorker> justReturnedWorkers;

#if OPTIMALRETURN_DEBUG
        int collisions = 0;
        int noncollisions = 0;
#endif

        void updateCollisionAndKeptSpeed(const JustReturnedWorker &justReturnedWorker)
        {
            // There is a collision if the worker isn't moving
            bool collision = (currentFrame - justReturnedWorker.worker->frameLastMoved) > 2;

            // TODO: Check for kept speed

#if OPTIMALRETURN_DEBUG
            if (collision)
            {
                CherryVis::log(justReturnedWorker.worker->id) << "Collision with depot";
                collisions++;
            }
            else
            {
                noncollisions++;
            }
#endif

            // Update the stats on the appropriate position metadata
            auto &optimalReturnPositions = optimalReturnPositionsFor(justReturnedWorker.resource);

            // If no resend occurred, update all positions
            if (!justReturnedWorker.resentPosition)
            {
                for (const auto &position : justReturnedWorker.positionHistory)
                {
                    auto metadataIt = optimalReturnPositions.find(*position);
                    if (metadataIt != optimalReturnPositions.end())
                    {
                        if (collision)
                        {
                            metadataIt->second.noResendArrivalObservations.collision++;
                        }
                        else
                        {
                            metadataIt->second.noResendArrivalObservations.stopped++;
                        }
                    }
                }
                return;
            }

            auto resendMetadataIt = optimalReturnPositions.find(*justReturnedWorker.resentPosition);
            if (resendMetadataIt == optimalReturnPositions.end()) // should never happen
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Return metadata not found for " << *justReturnedWorker.resentPosition
                           << "; worker id " << justReturnedWorker.worker->id << " @ " << justReturnedWorker.worker->getTilePosition();
#endif
                return;
            }

            if (collision)
            {
                resendMetadataIt->second.resendArrivalObservations.collision++;
            }
            else
            {
                resendMetadataIt->second.resendArrivalObservations.stopped++;
            }
        }

        void updateReturnOptimization(WorkerReturnStatus &workerStatus)
        {
#if OPTIMALRETURN_DEBUG
            auto &worker = workerStatus.worker;
            if (workerStatus.plannedResendPosition && !workerStatus.resentPosition)
            {
                Log::Get() << "ERROR: Worker didn't resend at planned return position " << *workerStatus.plannedResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
#endif

            // Find the arrival position and resend position in the history
            auto arrivalPositionIt = workerStatus.positionHistory.end();
            auto resendPositionIt = workerStatus.positionHistory.end();
            for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != workerStatus.positionHistory.end(); positionIt++)
            {
                if (workerStatus.resentPosition && (*workerStatus.resentPosition) == **positionIt)
                {
                    resendPositionIt = positionIt;
                }

                if (arrivalPositionIt == workerStatus.positionHistory.end())
                {
                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        (*positionIt)->pos(),
                                                        workerStatus.depot->type,
                                                        workerStatus.depot->lastPosition);
                    if (dist == 0 && workerStatus.worker->lastPosition == (*positionIt)->pos())
                    {
                        arrivalPositionIt = positionIt;
                    }
                }
            }
            if (arrivalPositionIt == workerStatus.positionHistory.end())
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Couldn't find arrival at depot position in history"
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                return;
            }
            if (workerStatus.resentPosition && resendPositionIt == workerStatus.positionHistory.end())
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Couldn't find return resend position in history"
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                return;
            }

            auto &optimalReturnPositions = optimalReturnPositionsFor(workerStatus.resource);

            // If we sent no command, record the path for exploration
            if (!workerStatus.resentPosition)
            {
                // Get the "path hash", which is the hash of the first position the worker moved in the stored path
                uint32_t pathHash = 0;
                auto first = **workerStatus.positionHistory.begin();
                for (auto positionIt = workerStatus.positionHistory.begin() + 1; positionIt != workerStatus.positionHistory.end(); positionIt++)
                {
                    if (first == **positionIt) continue;

                    pathHash = (*positionIt)->previousPositionsHash;
                    break;
                }

                // Create metadata for any missing positions on this path
                for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != arrivalPositionIt; positionIt++)
                {
                    int arrival = (int)std::distance(positionIt, arrivalPositionIt);

                    auto existingIt = optimalReturnPositions.find(**positionIt);
                    if (existingIt != optimalReturnPositions.end())
                    {
#if OPTIMALRETURN_DEBUG
                        if (!existingIt->second.noResendArrivalObservations.arrivalDelayAndOccurrences.contains(arrival))
                        {
                            CherryVis::log(worker->id) << "New arrival of " << arrival << " came up for " << existingIt->second;
                        }
#endif

                        existingIt->second.noResendArrivalObservations.arrivalDelayAndOccurrences[arrival]++;
                        continue;
                    }

                    optimalReturnPositions.emplace(
                            **positionIt,
                            ReturnPositionObservations(
                                    pathHash,
                                    **positionIt,
                                    arrival)
                    );

#if OPTIMALRETURN_DEBUG
                    CherryVis::log(worker->id) << "Added metadata for " << **positionIt << " at arrival " << arrival;
#endif
                }
                return;
            }
        }
    }

    void flushReturnObservations(std::map<MyWorker, WorkerReturnStatus> &workerReturnStatuses)
    {
        if (currentFrame == 0) justReturnedWorkers.clear();

        // Update collision and speed state for workers that are finished returning
        for (auto it = justReturnedWorkers.begin(); it != justReturnedWorkers.end();)
        {
            auto &worker = it->worker;
            if (!worker->exists())
            {
                it = justReturnedWorkers.erase(it);
                continue;
            }

            // Wait until the worker delivered a resource 8 frames ago
            if (worker->carryingResource || worker->lastCarryingResourceChange != (currentFrame - 8))
            {
                it++;
                continue;
            }

            updateCollisionAndKeptSpeed(*it);

            // Don't need to track this any more
            it = justReturnedWorkers.erase(it);
        }

        // Flush the worker statuses for workers that have delivered their cargo
        for (auto it = workerReturnStatuses.begin(); it != workerReturnStatuses.end();)
        {
            auto &worker = it->first;
            if (!worker->exists())
            {
                it = workerReturnStatuses.erase(it);
                continue;
            }

            if (worker->carryingResource)
            {
                it++;
                continue;
            }

            // We ignore workers that didn't start at the patch or had excessively long paths (indicating distance mining)
            if (!it->second.pathStartsAtPatch || it->second.positionHistory.size() > 60)
            {
                it = workerReturnStatuses.erase(it);
                continue;
            }

            // Add the final position to the history
            it->second.appendCurrentPosition();

            updateReturnOptimization(it->second);

            // TODO: Track observations

            // Move required fields into the MiningWorker struct that we use to track patch collisions
            justReturnedWorkers.emplace_back(JustReturnedWorker{
                    std::move(it->second.worker),
                    std::move(it->second.resource),
                    std::move(it->second.positionHistory),
                    std::move(it->second.resentPosition)});

            // We now no longer need to do anything with this worker status
            it = workerReturnStatuses.erase(it);
        }
    }
}
