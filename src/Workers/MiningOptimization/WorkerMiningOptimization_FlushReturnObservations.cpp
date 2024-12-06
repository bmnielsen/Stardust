// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing return of resources

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "Geo.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        struct PositionsInHistory
        {
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator firstMovedPositionIt;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator resendPositionIt;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator arrivalPositionIt;
        };

        bool extractPositionsInHistory(WorkerReturnStatus &workerStatus, PositionsInHistory &positionsInHistory)
        {
            if (workerStatus.positionHistory.empty()) return false;

            positionsInHistory.firstMovedPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.resendPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.arrivalPositionIt = workerStatus.positionHistory.end();

            int headingBeforeDelivery = INT_MAX;
            if (workerStatus.positionHistory.size() > 1)
            {
                headingBeforeDelivery = (*(workerStatus.positionHistory.rbegin() + 1))->heading;
            }

            auto firstPos = (*workerStatus.positionHistory.begin())->pos();
            for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != workerStatus.positionHistory.end(); positionIt++)
            {
                if (workerStatus.resentPosition && (*workerStatus.resentPosition) == **positionIt)
                {
                    positionsInHistory.resendPositionIt = positionIt;
                }

                if (positionsInHistory.arrivalPositionIt == workerStatus.positionHistory.end())
                {
                    // Arrival position is defined as the position where:
                    // - distance to the depot is 0
                    // - position is the same as the position at delivery
                    // - heading is the same as the heading immediately prior to delivery, unless this is the delivery position
                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        (*positionIt)->pos(),
                                                        workerStatus.depot->type,
                                                        workerStatus.depot->lastPosition);
                    if (dist == 0 && workerStatus.worker->lastPosition == (*positionIt)->pos()
                        && ((*positionIt)->heading == headingBeforeDelivery || (positionIt + 1) == workerStatus.positionHistory.end()))
                    {
                        positionsInHistory.arrivalPositionIt = positionIt;
                    }
                }

                if (positionsInHistory.firstMovedPositionIt == workerStatus.positionHistory.end() &&
                    (positionIt + 1) != workerStatus.positionHistory.end())
                {
                    // We define the "first moved position" as the first position at least 2 pixels from the initial position
                    // where the worker is moving
                    if ((*positionIt)->pos().getApproxDistance(firstPos) >= 2 && (*positionIt)->pos() != (*(positionIt + 1))->pos())
                    {
                        positionsInHistory.firstMovedPositionIt = positionIt;
#if OPTIMALRETURN_DEBUG
                        CherryVis::log(workerStatus.worker->id)
                                << "First move position at delta " << std::distance(workerStatus.positionHistory.begin(), positionIt)
                                << " from first position";
#endif
                    }
                }
            }
            if (positionsInHistory.firstMovedPositionIt == workerStatus.positionHistory.end())
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Couldn't find first return move position in history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }
            if (positionsInHistory.arrivalPositionIt == workerStatus.positionHistory.end())
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Couldn't find arrival at depot position in history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }
            if (workerStatus.resentPosition && positionsInHistory.resendPositionIt == workerStatus.positionHistory.end())
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Couldn't find return resend position in history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }

            return true;
        }

        // Used to track whether a worker that has just returned resources had a collision, had a normal return, or kept its speed
        struct JustReturnedWorker
        {
            MyWorker worker;
            Resource resource;
            bool deliveredOnArrivalFrame;
            std::vector<std::shared_ptr<const PositionAndVelocity>> positionHistory;
            std::shared_ptr<const PositionAndVelocity> resentPosition;
        };

        std::vector<JustReturnedWorker> justReturnedWorkers;

#if OPTIMALRETURN_DEBUG
        ReturnSpeedOccurrences deliveryAfterArrivalSpeedTotals;
        ReturnSpeedOccurrences deliveryAtArrivalSpeedTotals;
#endif

        void updateCollisionAndKeptSpeed(const JustReturnedWorker &justReturnedWorker)
        {
            auto &worker = justReturnedWorker.worker;

            ReturnSpeedOccurrences::ReturnSpeedObservation observation;

            // There is a collision if the worker isn't moving
            bool collision = (currentFrame - worker->frameLastMoved) > 2;
            if (collision)
            {
                observation = ReturnSpeedOccurrences::ReturnSpeedObservation::Collision;
#if OPTIMALRETURN_DEBUG
                CherryVis::log(worker->id) << "Collision with depot";
#endif
            }

            if (!collision)
            {
                auto speed = sqrt(
                        worker->bwapiUnit->getVelocityX() * worker->bwapiUnit->getVelocityX()
                        + worker->bwapiUnit->getVelocityY() * worker->bwapiUnit->getVelocityY()
                );
                auto speedFraction = speed / worker->type.topSpeed();

                if (speedFraction >= 0.8)
                {
                    observation = ReturnSpeedOccurrences::ReturnSpeedObservation::HighExitSpeed;
#if OPTIMALRETURN_DEBUG
                    CherryVis::log(worker->id) << "High exit speed: " << std::fixed << std::setprecision(1) << (100.0 * speedFraction) << "%";
#endif
                }
                else if (speedFraction >= 0.5)
                {
                    observation = ReturnSpeedOccurrences::ReturnSpeedObservation::MediumExitSpeed;
#if OPTIMALRETURN_DEBUG
                    CherryVis::log(worker->id) << "Medium exit speed: " << std::fixed << std::setprecision(1) << (100.0 * speedFraction) << "%";
#endif
                }
                else
                {
                    observation = ReturnSpeedOccurrences::ReturnSpeedObservation::LowExitSpeed;
#if OPTIMALRETURN_DEBUG
                    CherryVis::log(worker->id) << "Low exit speed: " << std::fixed << std::setprecision(1) << (100.0 * speedFraction) << "%";
#endif
                }
            }

#if OPTIMALRETURN_DEBUG
            (justReturnedWorker.deliveredOnArrivalFrame ? deliveryAtArrivalSpeedTotals : deliveryAfterArrivalSpeedTotals).addObservation(observation);
#endif

            auto addObservation = [&](ReturnArrivalObservations &observations)
            {
                (justReturnedWorker.deliveredOnArrivalFrame
                 ? observations.deliveryAtArrivalSpeeds
                 : observations.deliveryAfterArrivalSpeeds).addObservation(observation);
            };

            // Update the stats on the appropriate position metadata
            auto &optimalReturnPositions = optimalReturnPositionsFor(justReturnedWorker.resource);

            // If no resend occurred, update all positions that have metadata
            if (!justReturnedWorker.resentPosition)
            {
                for (const auto &position : justReturnedWorker.positionHistory)
                {
                    auto metadataIt = optimalReturnPositions.find(*position);
                    if (metadataIt != optimalReturnPositions.end())
                    {
                        addObservation(metadataIt->second.noResendArrivalObservations);
                    }
                }
                return;
            }

            auto resendMetadataIt = optimalReturnPositions.find(*justReturnedWorker.resentPosition);
            if (resendMetadataIt == optimalReturnPositions.end()) // should never happen
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Return metadata not found for " << *justReturnedWorker.resentPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                return;
            }

            addObservation(resendMetadataIt->second.resendArrivalObservations);
        }

        void updateNextPositions(WorkerReturnStatus &workerStatus,
                                 PositionsInHistory &positionsInHistory)
        {
            auto &optimalReturnPositions = optimalReturnPositionsFor(workerStatus.resource);

            auto ensureBeforeArrival = [&positionsInHistory](auto it)
            {
                if (std::distance(it, positionsInHistory.arrivalPositionIt) < 0)
                {
                    return positionsInHistory.arrivalPositionIt;
                }
                return it;
            };

            // Include LF positions after the resend since the positions only change after the command kicks in
            bool passedExistingPosition = false;
            auto limit = positionsInHistory.arrivalPositionIt;
            if (workerStatus.resentPosition)
            {
                limit = ensureBeforeArrival(positionsInHistory.resendPositionIt + BWAPI::Broodwar->getLatencyFrames() + 1);
            }
            for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != limit; positionIt++)
            {
                auto metadataIt = optimalReturnPositions.find(**positionIt);
                if (metadataIt == optimalReturnPositions.end())
                {
                    // We create default metadata for anything we create a next link to, but not anything that comes before
                    if (!passedExistingPosition) continue;

                    metadataIt = optimalReturnPositions.emplace(
                            **positionIt,
                            ReturnPositionObservations(**positionIt)
                    ).first;
                }
                else
                {
                    passedExistingPosition = true;
                }

                if ((positionIt + 1) != limit)
                {
                    metadataIt->second.addNext(**(positionIt + 1));
                }
            }
        }

        void updateReturnOptimization(WorkerReturnStatus &workerStatus, const PositionsInHistory &positionsInHistory)
        {
#if OPTIMALRETURN_DEBUG
            auto &worker = workerStatus.worker;
            if (workerStatus.plannedResendPosition && !workerStatus.resentPosition)
            {
                Log::Get() << "ERROR: Worker didn't resend at planned return position " << *workerStatus.plannedResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
#endif

            auto &optimalReturnPositions = optimalReturnPositionsFor(workerStatus.resource);

            // If we sent no command, record the path for exploration
            if (!workerStatus.resentPosition)
            {
                // Create metadata for any missing positions on this path
                for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != positionsInHistory.arrivalPositionIt; positionIt++)
                {
                    auto arrival = (uint16_t)std::distance(positionIt, positionsInHistory.arrivalPositionIt);

                    auto existingIt = optimalReturnPositions.find(**positionIt);
                    if (existingIt != optimalReturnPositions.end())
                    {
#if OPTIMALRETURN_DEBUG
                        if (!existingIt->second.noResendArrivalObservations.arrivalDelayAndOccurrences.contains(arrival))
                        {
                            CherryVis::log(worker->id) << "New arrival of " << arrival << " came up for " << existingIt->second;
                        }
#endif

                        existingIt->second.noResendArrivalObservations.add(arrival);
                        continue;
                    }

                    if (arrival > (RETURN_EXPLORE_BEFORE + 8 + BWAPI::Broodwar->getLatencyFrames())) continue;

                    optimalReturnPositions.emplace(
                            **positionIt,
                            ReturnPositionObservations(**positionIt, arrival)
                    );

#if OPTIMALRETURN_DEBUG_VERBOSE
                    CherryVis::log(worker->id) << "Added metadata for " << **positionIt << " at arrival " << arrival;
#endif
                }
                return;
            }

            // Get the data for the resent position
            auto resentPositionDataIt = optimalReturnPositions.find(*workerStatus.resentPosition);
            if (resentPositionDataIt == optimalReturnPositions.end()) // should never happen
            {
#if OPTIMALRETURN_DEBUG
                Log::Get() << "ERROR: Observations not found for return resend position " << *workerStatus.resentPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                return;
            }

            // Record the observation
            int arrival = (int)std::distance(positionsInHistory.resendPositionIt, positionsInHistory.arrivalPositionIt);
            resentPositionDataIt->second.resendArrivalObservations.add(arrival);

#if OPTIMALRETURN_DEBUG_VERBOSE
            CherryVis::log(worker->id) << "Added observation of " << *workerStatus.resentPosition << " with arrival " << arrival;
#endif

#if OPTIMALRETURN_DEBUG
            // Validate that the delivery frame matches what we would expect
            auto actualFramesToDelivery = (int)std::distance(positionsInHistory.resendPositionIt, workerStatus.positionHistory.end()) - 1;
            auto noResetExpectedFramesToDelivery = (BWAPI::Broodwar->getLatencyFrames() + 10);
            while (arrival > noResetExpectedFramesToDelivery)
            {
                noResetExpectedFramesToDelivery += 9;
            }

            auto framesToReset =
                    OrderProcessTimer::framesToNextReset(currentFrame - actualFramesToDelivery + BWAPI::Broodwar->getLatencyFrames() + 1);

            framesToReset += BWAPI::Broodwar->getLatencyFrames();

            int minExpectedFramesToDelivery, maxExpectedFramesToDelivery;
            if (framesToReset < arrival)
            {
                // Reset prior to arrival
                minExpectedFramesToDelivery = arrival;
                maxExpectedFramesToDelivery = arrival + 9;
            }
            else if (framesToReset < noResetExpectedFramesToDelivery)
            {
                // Reset between arrival and delivery
                minExpectedFramesToDelivery = framesToReset + 1;
                maxExpectedFramesToDelivery = minExpectedFramesToDelivery + 8;
            }
            else
            {
                minExpectedFramesToDelivery = maxExpectedFramesToDelivery = noResetExpectedFramesToDelivery;
            }
            if (actualFramesToDelivery < minExpectedFramesToDelivery || actualFramesToDelivery > maxExpectedFramesToDelivery)
            {
                Log::Get() << "ERROR: Position " << *workerStatus.resentPosition << " has unexpected delivery delay"
                           << "; expected=" << minExpectedFramesToDelivery << "-" << maxExpectedFramesToDelivery
                           << "; actual=" << actualFramesToDelivery
                           << "; framesToReset=" << framesToReset
                           << "; framesToArrival=" << arrival
                           << "; framesToNoResetMining=" << noResetExpectedFramesToDelivery
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
#endif

            // TODO: Validate effectiveness of resends (also wrt. collisions and kept speed)
        }
    }

    void flushReturnObservations(std::map<MyWorker, WorkerReturnStatus> &workerReturnStatuses)
    {
        if (currentFrame == 0) justReturnedWorkers.clear();

#if OPTIMALRETURN_DEBUG
        if (currentFrame == 0)
        {
            deliveryAfterArrivalSpeedTotals = {0, 0, 0, 0};
            deliveryAtArrivalSpeedTotals = {0, 0, 0, 0};
        }
        else if (currentFrame % 1000 == 0)
        {
            auto outputSpeedTotals = [](const ReturnSpeedOccurrences &speedTotals, const std::string &label)
            {
                uint16_t total = speedTotals.collision + speedTotals.lowExitSpeed + speedTotals.mediumExitSpeed + speedTotals.highExitSpeed;
                if (total == 0) return;

                Log::Get() << std::fixed << std::setprecision(1)
                           << "Speed statistics for " << label << ":\n"
                           << " Collision rate:    " << (100.0 * speedTotals.collision) / (double)(total) << "%\n"
                           << " Low speed rate:    " << (100.0 * speedTotals.lowExitSpeed) / (double)(total) << "%\n"
                           << " Medium speed rate: " << (100.0 * speedTotals.mediumExitSpeed) / (double)(total) << "%\n"
                           << " High speed rate:   " << (100.0 * speedTotals.highExitSpeed) / (double)(total) << "%\n"
                           << "over " << total << " deliveries";
            };
            outputSpeedTotals(deliveryAfterArrivalSpeedTotals, "delivery after arrival frame");
            outputSpeedTotals(deliveryAtArrivalSpeedTotals, "delivery at arrival frame");
        }
#endif

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

            // Skip the worker if it has been ordered to do something else in the meantime or isn't moving to minerals
            if (worker->bwapiUnit->getLastCommandFrame() >= (BWAPI::Broodwar->getFrameCount() - 8 - BWAPI::Broodwar->getLatencyFrames()) ||
                worker->bwapiUnit->getOrder() != BWAPI::Orders::MoveToMinerals)
            {
#if OPTIMALRETURN_DEBUG
                CherryVis::log(worker->id) << "Not tracking collision and speed observation, as the worker has apparently been reassigned";
#endif
                it = justReturnedWorkers.erase(it);
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

            // Add the final position to the history
            it->second.appendCurrentPosition();

            // We ignore workers that didn't start at the patch or had excessively long paths (indicating distance mining)
            PositionsInHistory positionsInHistory;
            if (!it->second.pathStartsAtPatch || it->second.positionHistory.size() > 75 || !extractPositionsInHistory(it->second, positionsInHistory))
            {
#if OPTIMALRETURN_DEBUG
                CherryVis::log(worker->id) << "Not tracking observations for this return"
                    << ": pathStartsAtPatch=" << it->second.pathStartsAtPatch
                    << ": path length=" << it->second.positionHistory.size();
#endif
                it = workerReturnStatuses.erase(it);
                continue;
            }

            updateReturnOptimization(it->second, positionsInHistory);

            updateNextPositions(it->second, positionsInHistory);

            // Move required fields into the MiningWorker struct that we use to track patch collisions
            justReturnedWorkers.emplace_back(JustReturnedWorker{
                    std::move(it->second.worker),
                    std::move(it->second.resource),
                    positionsInHistory.arrivalPositionIt == (it->second.positionHistory.end() - 1),
                    std::move(it->second.positionHistory),
                    std::move(it->second.resentPosition)});

            // We now no longer need to do anything with this worker status
            it = workerReturnStatuses.erase(it);
        }
    }
}
