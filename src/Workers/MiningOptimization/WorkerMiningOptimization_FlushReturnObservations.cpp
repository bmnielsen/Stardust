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
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator arrivalPositionIt;

            std::vector<ReturnPositionObservations*> positionHistory;
            ReturnPositionObservations* resendPosition = nullptr;

            int resendDistanceToArrival = 0;
        };

        bool extractPositionsInHistory(PositionsInHistory &positionsInHistory,
                                       WorkerReturnStatus &workerStatus,
                                       std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &rootNodes,
                                       bool createObservations)
        {
            if (workerStatus.positionHistory.empty()) return false;
            if (workerStatus.positionHistory.size() > 75) return false;

            positionsInHistory.firstMovedPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.arrivalPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.positionHistory.clear();
            positionsInHistory.resendPosition = nullptr;

            auto resendPositionIt = workerStatus.positionHistory.end();

            auto firstPos = (*workerStatus.positionHistory.begin())->pos();
            for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != workerStatus.positionHistory.end(); positionIt++)
            {
                if (workerStatus.resentPosition && (*workerStatus.resentPosition) == **positionIt)
                {
                    resendPositionIt = positionIt;
                }

                if (positionsInHistory.arrivalPositionIt == workerStatus.positionHistory.end())
                {
                    // Arrival position is defined as the position where:
                    // - distance to the depot is 0
                    // - position is the same as the position at delivery
                    // - heading is stable
                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        (*positionIt)->pos(),
                                                        workerStatus.depot->type,
                                                        workerStatus.depot->lastPosition);
                    if (dist == 0 && workerStatus.worker->lastPosition == (*positionIt)->pos()
                        && PositionAndVelocity::isStableArrivalPosition(workerStatus.positionHistory, positionIt))
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
#if LOGGING_ENABLED
                Log::Get() << "ERROR: Couldn't find first return move position in history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }
            if (positionsInHistory.arrivalPositionIt == workerStatus.positionHistory.end())
            {
#if LOGGING_ENABLED
                Log::Get() << "ERROR: Couldn't find arrival at depot position in history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }
            if (workerStatus.resentPosition && resendPositionIt == workerStatus.positionHistory.end())
            {
#if LOGGING_ENABLED
                Log::Get() << "ERROR: Couldn't find return resend position in history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }
            else if (workerStatus.resentPosition &&
                     std::distance(resendPositionIt, positionsInHistory.arrivalPositionIt) <= BWAPI::Broodwar->getLatencyFrames())
            {
#if LOGGING_ENABLED
                Log::Get() << "ERROR: Return resend was within LF of arrival"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }

            positionsInHistory.resendDistanceToArrival = (int)std::distance(resendPositionIt, positionsInHistory.arrivalPositionIt);

            // Reference the observations and potentially create new nodes

            // Start by finding or creating the root node
            auto rootNodeIt = rootNodes.find(**workerStatus.positionHistory.begin());
            if (rootNodeIt == rootNodes.end())
            {
                if (!createObservations) return false;
                auto result = rootNodes.emplace(**workerStatus.positionHistory.begin(), **workerStatus.positionHistory.begin());
                rootNodeIt = result.first;
            }
            else if (rootNodeIt->second.occurrences < OCCURRENCE_LIMIT)
            {
                rootNodeIt->second.occurrences++;
            }

            auto current = &(rootNodeIt->second);
            positionsInHistory.positionHistory.push_back(current);

            // Add positions up to the resend (or arrival position if there was no resend)
            auto limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
            if (resendPositionIt != workerStatus.positionHistory.end())
            {
                limit = resendPositionIt + 1;
            }
            for (auto positionIt = workerStatus.positionHistory.begin() + 1; positionIt != limit; positionIt++)
            {
                // Try to find the next position
                auto [next, atLimit] = findNextPositionCheckingOccurrences(**positionIt, current->nextPositions);

                // If we have a new path branch that we can't create, bail out now
                if (!next && (!createObservations || atLimit)) return false;

                // Create a new item if needed, otherwise bump the occurrence count if possible
                if (!next)
                {
                    next = &current->nextPositions.emplace_back(**positionIt);
                }
                else if (!atLimit)
                {
                    next->occurrences++;
                }

                current = next;
                positionsInHistory.positionHistory.push_back(current);
            }
            if (resendPositionIt != workerStatus.positionHistory.end())
            {
                positionsInHistory.resendPosition = current;
            }

            return true;
        }

        // Used to track whether a worker that has just returned resources had a collision, had a normal return, or kept its speed
        struct JustReturnedWorker
        {
            MyWorker worker;
            MyUnit depot;
            Resource resource;
            bool deliveredOnArrivalFrame;
            std::vector<ReturnPositionObservations*> positionHistory;
            ReturnPositionObservations* resentPosition;
        };

        std::vector<JustReturnedWorker> justReturnedWorkers;

#if OPTIMALRETURN_DEBUG
        ReturnSpeedOccurrences deliveryAfterArrivalSpeedTotals;
        ReturnSpeedOccurrences deliveryAtArrivalSpeedTotals;
#endif
#if LOGGING_ENABLED
        unsigned int hadPathData = 0;
        unsigned int didNotHavePathData = 0;
#endif

        void updateCollisionAndKeptSpeed(const JustReturnedWorker &justReturnedWorker)
        {
            auto &worker = justReturnedWorker.worker;

            ReturnSpeedOccurrences::ReturnSpeedObservation observation;

            // There is a collision if the worker is at the depot and isn't moving
            bool collision = (justReturnedWorker.depot->getDistance(worker) == 0 && (currentFrame - worker->frameLastMoved) > 2);
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

            // If no resend occurred, update all positions
            if (!justReturnedWorker.resentPosition)
            {
                for (const auto &position : justReturnedWorker.positionHistory)
                {
                    addObservation(position->noResendArrivalObservations);
                }
                return;
            }

            addObservation(justReturnedWorker.resentPosition->resendArrivalObservations);
        }

        void updateReturnOptimization(WorkerReturnStatus &workerStatus, const PositionsInHistory &positionsInHistory)
        {
#if LOGGING_ENABLED
            auto &worker = workerStatus.worker;
            if (workerStatus.plannedResendPosition && !workerStatus.resentPosition)
            {
                Log::Get() << "ERROR: Worker didn't resend at planned return position " << *workerStatus.plannedResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
#endif

            // If we sent no command, record the path for exploration
            if (!workerStatus.resentPosition)
            {
                // Update the metadata for the positions on this path
                for (auto positionIt = positionsInHistory.positionHistory.begin();
                     positionIt != positionsInHistory.positionHistory.end();
                     positionIt++)
                {
                    auto arrival =
                            (uint16_t)(std::distance(positionIt, positionsInHistory.positionHistory.end()) + BWAPI::Broodwar->getLatencyFrames());

#if OPTIMALRETURN_DEBUG
                    if ((*positionIt)->noResendArrivalObservations.arrivalDelayAndOccurrences.empty())
                    {
#if OPTIMALRETURN_DEBUG_VERBOSE
                        CherryVis::log(worker->id) << "Added metadata for " << **positionIt << " at arrival " << arrival;
#endif
                    }
                    else if (!(*positionIt)->noResendArrivalObservations.arrivalDelayAndOccurrences.contains(arrival))
                    {
                        CherryVis::log(worker->id) << "New arrival of " << arrival << " came up for " << (*positionIt)->pos;
                    }
#endif

                    (*positionIt)->noResendArrivalObservations.add(arrival);
                }
                return;
            }

            // Record the resend observation
            positionsInHistory.resendPosition->resendArrivalObservations.add(positionsInHistory.resendDistanceToArrival);

#if OPTIMALRETURN_DEBUG_VERBOSE
            CherryVis::log(worker->id) << "Added observation of " << *workerStatus.resentPosition
                                       << " with arrival " << positionsInHistory.resendDistanceToArrival;
#endif
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
        else if (currentFrame % 1000 == 0 && WorkerMiningOptimization::isExploring())
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
#if LOGGING_ENABLED
        if (currentFrame == 0)
        {
            hadPathData = 0;
            didNotHavePathData = 0;
        }
        else if (currentFrame % 1000 == 0)
        {
            auto total = hadPathData + didNotHavePathData;
            if (total > 0)
            {
                Log::Get() << std::fixed << std::setprecision(1)
                           << "Returns with path data: " << (100.0 * hadPathData) / (double)total
                           << "% over " << total << " collections";
            }
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

            // Wait until the worker delivered the resource
            if (worker->carryingResource)
            {
                it++;
                continue;
            }

            // Wait until the worker delivered the resource 8 frames ago
            int framesSinceDelivery = (currentFrame - worker->lastCarryingResourceChange);
            if (framesSinceDelivery < 8)
            {
                // If the worker has been reassigned to something else before our observation frame, abandon tracking it
                if (framesSinceDelivery < (8 - BWAPI::Broodwar->getLatencyFrames()) && worker->bwapiUnit->getOrder() != BWAPI::Orders::MoveToMinerals)
                {
#if OPTIMALRETURN_DEBUG
                    CherryVis::log(worker->id) << "Not tracking collision and speed observation, as the worker has apparently been reassigned";
#endif
                    it = justReturnedWorkers.erase(it);
                }
                else
                {
                    it++;
                }
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

#if LOGGING_ENABLED
            if (it->second.hasPathData)
            {
                hadPathData++;
            }
            else
            {
                didNotHavePathData++;
            }
#endif

            if (!WorkerMiningOptimization::isExploring() || !it->second.resource)
            {
                it = workerReturnStatuses.erase(it);
                continue;
            }

            // Add the final position to the history
            it->second.appendCurrentPosition();

            // We ignore workers that didn't start at the patch or had excessively long paths (indicating distance mining)
            auto &rootNodes = returnPositionRootNodesFor(it->second.resource);
            PositionsInHistory positionsInHistory;
            if (!it->second.pathStartsAtPatch ||
                !extractPositionsInHistory(positionsInHistory, it->second, rootNodes, WorkerMiningOptimization::isExploring()))
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

            // Move required fields into the MiningWorker struct that we use to track patch collisions
            justReturnedWorkers.emplace_back(JustReturnedWorker{
                    std::move(it->second.worker),
                    std::move(it->second.depot),
                    std::move(it->second.resource),
                    positionsInHistory.arrivalPositionIt == (it->second.positionHistory.end() - 1),
                    std::move(positionsInHistory.positionHistory),
                    positionsInHistory.resendPosition});

            // We now no longer need to do anything with this worker status
            it = workerReturnStatuses.erase(it);
        }
    }
}
