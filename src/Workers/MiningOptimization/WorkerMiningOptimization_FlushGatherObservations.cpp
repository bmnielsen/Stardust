// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "OccurrencesAndCollisions.h"
#include "GatherPositionObservations.h"
#include "WorkerGatherStatus.h"
#include "WorkerMiningInstrumentation.h"

#include "Geo.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        struct PositionsInHistory
        {
            std::vector<std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator> resendItsBeforeArrival;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator arrivalPositionIt;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator tenDistancePositionIt;

            std::vector<GatherPositionObservationPtr> positionHistory;
            std::vector<GatherPositionObservationPtr> resendsWithObservationData;
        };

        bool extractPositionsInHistory(PositionsInHistory &positionsInHistory,
                                       WorkerGatherStatus &workerStatus,
                                       bool createObservations)
        {
            if (!workerStatus.hasLeftDepot)
            {
                Log::Get() << "ERROR: Worker was not tracked as leaving depot"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
                return false;
            }

            // If the path is too short to possibly optimize, return here
            // This might happen if we have a case where the worker gets reassigned or otherwise doesn't follow a normal mining path
            if (workerStatus.positionHistory.size() < (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                return false;
            }

            positionsInHistory.resendItsBeforeArrival.clear();
            positionsInHistory.arrivalPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.tenDistancePositionIt = workerStatus.positionHistory.end();

            positionsInHistory.positionHistory.clear();
            positionsInHistory.resendsWithObservationData.clear();

            std::vector<std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator> resendPositionIts;
            auto nextResendPositionIt = workerStatus.resentPositions.begin();

            for (auto it = workerStatus.positionHistory.begin(); it != workerStatus.positionHistory.end(); it++)
            {
                if (nextResendPositionIt != workerStatus.resentPositions.end() && **nextResendPositionIt == **it)
                {
                    // We filter out resend positions after arrival later
                    resendPositionIts.push_back(it);
                    nextResendPositionIt++;
                }

                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    (*it)->pos(),
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    workerStatus.resource->center);

                // Arrival position is defined as the position where:
                // - distance to the patch is 0
                // - position is the same as the position at mining start
                // - heading is stable
                if (positionsInHistory.arrivalPositionIt == workerStatus.positionHistory.end()
                    && dist == 0
                    && workerStatus.worker->lastPosition == (*it)->pos()
                    && PositionAndVelocity::isStableArrivalPosition(workerStatus.positionHistory, it))
                {
                    positionsInHistory.arrivalPositionIt = it;
                }

                if (positionsInHistory.tenDistancePositionIt == workerStatus.positionHistory.end() && dist <= 10)
                {
                    positionsInHistory.tenDistancePositionIt = it - BWAPI::Broodwar->getLatencyFrames() - 1;
                }
            }

            // Don't process too short or two long paths
            auto startToEnd = std::distance(workerStatus.positionHistory.begin(), positionsInHistory.arrivalPositionIt);
            if (startToEnd < (BWAPI::Broodwar->getLatencyFrames() + 11)) return false;
            if (startToEnd > 75)
            {
                Log::Get() << "WARNING: Position history over 75 positions"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
                return false;
            }

            // Clear the ten distance position iterator if it is invalid
            if (positionsInHistory.tenDistancePositionIt != workerStatus.positionHistory.end() &&
                std::distance(workerStatus.positionHistory.begin(), positionsInHistory.tenDistancePositionIt) < 0)
            {
                positionsInHistory.tenDistancePositionIt = workerStatus.positionHistory.end();
            }

            // Return false if any of the resend positions couldn't be found
            if (workerStatus.resentPositions.size() != resendPositionIts.size())
            {
                Log::Get() << "ERROR: Not all resent positions found in position history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
                return false;
            }

            // Filter out the resends that happened before arrival
            for (auto resendPositionIt : resendPositionIts)
            {
                if (std::distance(resendPositionIt, positionsInHistory.arrivalPositionIt) <= BWAPI::Broodwar->getLatencyFrames())
                {
                    break;
                }

                positionsInHistory.resendItsBeforeArrival.push_back(resendPositionIt);
            }

            // Reference the observations and potentially create new nodes

            // Start by finding or creating the root node
            auto &rootNodes = gatherPositionRootNodesFor(workerStatus.resource);
            auto rootNodeIt = rootNodes.find(**workerStatus.positionHistory.begin());
            if (rootNodeIt == rootNodes.end())
            {
                if (!createObservations) return false;
                auto result = rootNodes.emplace(**workerStatus.positionHistory.begin(), **workerStatus.positionHistory.begin());
                rootNodeIt = result.first;
            }
            else if (rootNodeIt->second.occurrences < UINT32_MAX)
            {
                rootNodeIt->second.occurrences++;
            }

            auto current = GatherPositionObservationPtr(&(rootNodeIt->second));
            positionsInHistory.positionHistory.push_back(current);

            // Add main line positions up to the first resend (or arrival position if there was no resend)
            auto limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
            if (!positionsInHistory.resendItsBeforeArrival.empty())
            {
                limit = positionsInHistory.resendItsBeforeArrival[0] + 1;
            }
            for (auto positionIt = workerStatus.positionHistory.begin() + 1; positionIt != limit; positionIt++)
            {
                // Try to find the next position
                auto [next, atLimit] = findNextPositionCheckingOccurrences(**positionIt, current.pos->nextPositions);

                // If we have a new path branch that we can't create, bail out now
                if (!next && (!createObservations || atLimit)) return false;

                // Create a new item if needed, otherwise bump the occurrence count if possible
                if (!next)
                {
                    next = &current.pos->nextPositions.emplace_back(**positionIt);
                }
                else if (!atLimit)
                {
                    next->occurrences++;
                }
                updateNextOccurenceRates(current.pos->nextPositions);

                current = GatherPositionObservationPtr(next);
                positionsInHistory.positionHistory.push_back(current);
            }
            if (positionsInHistory.resendItsBeforeArrival.empty()) return true;

            positionsInHistory.resendsWithObservationData.push_back(current);

            // Add second resend positions up to the second resend (or arrival position if there was no second resend)
            limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
            if (positionsInHistory.resendItsBeforeArrival.size() > 1)
            {
                limit = positionsInHistory.resendItsBeforeArrival[1] + 1;
            }
            for (auto positionIt = positionsInHistory.resendItsBeforeArrival[0] + 1; positionIt != limit; positionIt++)
            {
                // Try to find the next position
                auto &nextPositions = current.nextSecondResendPositions();
                auto [next, atLimit] = findNextPositionCheckingOccurrences(**positionIt, nextPositions);

                // If we have a new path branch that we can't create, bail out now
                if (!next && (!createObservations || atLimit)) return false;

                // Create a new item if needed, otherwise bump the occurrence count if possible
                if (!next)
                {
                    next = &nextPositions.emplace_back(**positionIt);
                }
                else if (!atLimit)
                {
                    next->occurrences++;
                }
                updateNextOccurenceRates(nextPositions);

                current = GatherPositionObservationPtr(next);
                positionsInHistory.positionHistory.push_back(current);
            }

            if (positionsInHistory.resendItsBeforeArrival.size() > 1)
            {
                positionsInHistory.resendsWithObservationData.push_back(current);
            }

            return true;
        }

        // Used to track whether a mining worker collides with the patch after mining
        struct MiningWorker
        {
            MyWorker worker;
            Resource resource;
            std::vector<PositionAndVelocity> positionHistoryWithObservationData;
            std::vector<PositionAndVelocity> resendsWithObservationData;
            size_t resendsBeforeArrivalCount;
        };

        std::vector<MiningWorker> miningWorkers;

#if OPTIMALPOSITIONS_DEBUG
        std::set<BWAPI::TilePosition> exploredPatches;
        int collisions = 0;
        int noncollisions = 0;
#endif
#if LOGGING_ENABLED
        unsigned int hadPathData = 0;
        unsigned int didNotHavePathData = 0;
#endif

        void handlePossiblePatchCollision(MiningWorker &miningWorker)
        {
            // There is a collision if the worker is at the patch and isn't moving
            bool collision = (miningWorker.resource->getDistance(miningWorker.worker) == 0
                    && (currentFrame - miningWorker.worker->frameLastMoved) > 2);
            WorkerMiningInstrumentation::trackCollisionObservation(miningWorker.resource, collision);

#if OPTIMALPOSITIONS_DEBUG
            if (collision)
            {
                CherryVis::log(miningWorker.worker->id) << "Collision with patch";
                collisions++;
            }
            else
            {
                noncollisions++;
            }
#endif
            if (!WorkerMiningOptimization::isExploring()) return;

            // If there have been more than two resends, we can't record an observation since the path may have changed in an unexpected way
            if (miningWorker.resendsBeforeArrivalCount > 2) return;

            // Guard against invalid data
            if (miningWorker.positionHistoryWithObservationData.empty()) return;
            if (miningWorker.resendsWithObservationData.size() != miningWorker.resendsBeforeArrivalCount) return;

            // Find the root node
            auto &rootNodes = gatherPositionRootNodesFor(miningWorker.resource);
            auto rootNodeIt = rootNodes.find(miningWorker.positionHistoryWithObservationData.front());
            if (rootNodeIt == rootNodes.end())
            {
#if LOGGING_ENABLED
                Log::Get() << "ERROR: No root node found when handling gather collisions"
                           << "; worker id " << miningWorker.worker->id << " @ " << miningWorker.worker->getTilePosition();
#endif
                return;
            }

            std::shared_ptr<PositionAndVelocity> firstResendPosition;
            std::shared_ptr<PositionAndVelocity> lastResendPosition;
            if (!miningWorker.resendsWithObservationData.empty())
            {
                firstResendPosition = std::make_shared<PositionAndVelocity>(miningWorker.resendsWithObservationData.front());
                lastResendPosition = std::make_shared<PositionAndVelocity>(miningWorker.resendsWithObservationData.back());
            }

            auto recordObservationsOnNode = [&](GatherPositionObservationPtr &node)
            {
                if (lastResendPosition)
                {
                    if (node.position() == *lastResendPosition)
                    {
                        node.resendArrivalObservations().addCollision(collision);
                    }
                    return;
                }

                node.pos->addNoResendCollision(collision);
            };

            auto current = GatherPositionObservationPtr(&(rootNodeIt->second));
            recordObservationsOnNode(current);

            for (auto positionIt = miningWorker.positionHistoryWithObservationData.begin() + 1;
                 positionIt != miningWorker.positionHistoryWithObservationData.end();
                 positionIt++)
            {
                auto next = current.nextPositionIfExists(*positionIt, firstResendPosition);
                if (!next) break;

                current = *next;
                recordObservationsOnNode(current);
            }
        }

        void updateApproachOptimization(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory)
        {
#if LOGGING_ENABLED
            auto &worker = workerStatus.worker;
#endif

            // Iterator to the apparent optimal position in the position history
            auto optimalPositionIt = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames() - 11;

#if LOGGING_ENABLED
            if (workerStatus.plannedResendPosition && workerStatus.resentPositions.empty())
            {
                Log::Get() << "ERROR: Worker didn't resend at planned position " << *workerStatus.plannedResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
            else if (workerStatus.plannedSecondResendPosition && workerStatus.resentPositions.size() < 2)
            {
                Log::Get() << "ERROR: Worker didn't resend at second planned position " << *workerStatus.plannedSecondResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
#endif

            // Check if we want to make any provisional observations about resend positions where we know the worker doesn't arrive on time,
            // but don't know the exact timing as we were forced to resend another command later
            // We only do this if there are no earlier observations of the position, and we replace this observation later if we get a clean
            // path
            if (workerStatus.switchedPatches || positionsInHistory.resendItsBeforeArrival.size() > std::min(2, workerStatus.plannedResendCount()))
            {
                auto provisionalObservation = [&](int resendIndex)
                {
                    // There must be a resend after this - otherwise we could just make a normal observation
                    if (positionsInHistory.resendItsBeforeArrival.size() < (resendIndex + 2)) return;

                    // Must have resend data for this position
                    if (positionsInHistory.resendsWithObservationData.size() > resendIndex) return;

                    // There must have been a full order process cycle after this resend before the next one
                    if (std::distance(positionsInHistory.resendItsBeforeArrival[resendIndex],
                                      positionsInHistory.resendItsBeforeArrival[resendIndex + 1]) < 11)
                    {
                        return;
                    }

                    // Reference the observations for this position
                    auto &observations = positionsInHistory.resendsWithObservationData[resendIndex].resendArrivalObservations();

                    // There must not have been any observations already
                    if (!observations.empty()) return;

                    // Make the observation
                    observations.addArrival((int)std::distance(positionsInHistory.resendItsBeforeArrival[resendIndex], optimalPositionIt));

#if OPTIMALPOSITIONS_DEBUG
                    if (resendIndex == 1)
                    {
                        CherryVis::log(worker->id) << "Added provisional observation of " << **positionsInHistory.resendItsBeforeArrival[0]
                                                   << " : " << **positionsInHistory.resendItsBeforeArrival[1];
                    }
                    else
                    {
                        CherryVis::log(worker->id) << "Added provisional observation of " << **positionsInHistory.resendItsBeforeArrival[0];
                    }
#endif
                };
                if (workerStatus.plannedResendPosition) provisionalObservation(0);
                if (workerStatus.plannedSecondResendPosition) provisionalObservation(1);
                if (workerStatus.switchedPatches || positionsInHistory.resendItsBeforeArrival.size() > 2) return;
            }

            // If we sent no command, record the path for exploration
            if (positionsInHistory.resendItsBeforeArrival.empty())
            {
                // Update the metadata for the positions in the path
                auto optimalPositionDataIt =
                        positionsInHistory.positionHistory.begin() + std::distance(workerStatus.positionHistory.begin(), optimalPositionIt);
                for (auto positionIt = positionsInHistory.positionHistory.begin();
                     positionIt != positionsInHistory.positionHistory.end();
                     positionIt++)
                {
                    auto delta = (int)std::distance(optimalPositionDataIt, positionIt);

#if OPTIMALPOSITIONS_DEBUG
                    if (positionIt->pos->deltaToBenchmarkAndOccurrenceRate.empty())
                    {
#if OPTIMALPOSITIONS_DEBUG_VERBOSE
                        CherryVis::log(worker->id) << "Added metadata for " << *positionIt << " at delta " << delta;
#endif
                    }
                    else if (!positionIt->pos->deltaToBenchmarkAndOccurrenceRate.contains((int8_t)delta))
                    {
                        CherryVis::log(worker->id) << "New delta of " << delta << " came up for " << positionIt->pos->pos;
                    }
#endif

                    positionIt->pos->addDeltaToBenchmark(delta);
                }
                return;
            }

            auto lastResendPositionIt = *positionsInHistory.resendItsBeforeArrival.rbegin();

#if OPTIMALPOSITIONS_DEBUG
            bool exploring = positionsInHistory.resendsWithObservationData.rbegin()->resendArrivalObservations().addArrival(
                    (int)std::distance(lastResendPositionIt, optimalPositionIt));

#if OPTIMALPOSITIONS_DEBUG_VERBOSE
            if (positionsInHistory.resendItsBeforeArrival.size() > 1)
            {
                CherryVis::log(worker->id) << "Added observation of " << positionsInHistory.resendsWithObservationData[0]
                                           << " : " << positionsInHistory.resendsWithObservationData[1];
            }
            else
            {
                CherryVis::log(worker->id) << "Added observation of " << positionsInHistory.resendsWithObservationData[0];
            }
#endif

            if (exploring)
            {
                exploredPatches.insert(workerStatus.resource->tile);
            }
#else
            // Track the observation
            positionsInHistory.resendsWithObservationData.rbegin()->resendArrivalObservations().addArrival(
                    (int)std::distance(lastResendPositionIt, optimalPositionIt));
#endif

//            // Consider exploration of second resend positions
//            // This is not needed if:
//            // - There was a second resend
//            // - The information for this path is incomplete
//            // - This position is not inside our optimization horizon
//            // - The path did not start at the depot
//            if (secondResendData) return;
//            if (resentPositionData.deltaToBenchmarkAndOccurrences.empty()) return;
//
//            int probableDeltaToBenchmark = resentPositionData.probableDeltaToBenchmark();
//            if (probableDeltaToBenchmark < -GATHER_EXPLORE_BEFORE) return;
//            if (probableDeltaToBenchmark > GATHER_EXPLORE_AFTER) return;
//
//            if (!workerStatus.pathStartsAtDepot) return;
//
//            // Check if the path after the resend is the same as the path without a resend
//            // If so, we don't bother tracking second resends on this, as they will be the same as the normal path
//            auto pathsMatch = [&]()
//            {
//                if (resentPositionData.resendChangesPath == ResendChangesPath::Yes) return false;
//
//                auto noResendPath = resentPositionData.followingPositionsIfStable(optimalGatherPositions);
//                if (noResendPath.empty()) return false; // No resend path is unstable
//
//                size_t noResendPathIdx = 0;
//                for (auto positionIt = positionsInHistory.resendPositionIts[0] + 1; positionIt != positionsInHistory.arrivalPositionIt; positionIt++)
//                {
//                    if (noResendPath[noResendPathIdx]->pos != **positionIt)
//                    {
//                        return false;
//                    }
//
//                    noResendPathIdx++;
//                    if (noResendPathIdx >= noResendPath.size()) break;
//                }
//                return true;
//            };
//
//            if (pathsMatch())
//            {
//                resentPositionData.resendChangesPath = ResendChangesPath::No;
//                return;
//            }
//
//            // If this is the first detection of a changed path, add the existing next positions as a second resend position
//            if (resentPositionData.resendChangesPath != ResendChangesPath::Yes)
//            {
//                resentPositionData.resendChangesPath = ResendChangesPath::Yes;
//
//                for (const auto &[nextPosition, _] : resentPositionData.nextPositionAndOccurrences)
//                {
//                    auto secondResendObservationsIt = resentPositionData.secondResendObservations.find(nextPosition);
//                    if (secondResendObservationsIt == resentPositionData.secondResendObservations.end())
//                    {
//                        resentPositionData.secondResendObservations.emplace(
//                                nextPosition,
//                                SecondResendGatherPositionObservations{1});
//
//#if OPTIMALPOSITIONS_DEBUG_VERBOSE
//                        CherryVis::log(worker->id) << "Added metadata for " << resentPositionData
//                                                   << " : " << nextPosition
//                                                   << " after discovering unstable path after resends";
//#endif
//                    }
//                }
//            }
//
//            // Queue up second resend positions to explore
//            auto limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
//            for (auto positionIt = positionsInHistory.resendPositionIts[0] + 1; std::distance(positionIt, limit) > 0; positionIt++)
//            {
//                auto secondResendObservationsIt = resentPositionData.secondResendObservations.find(**positionIt);
//                if (secondResendObservationsIt != resentPositionData.secondResendObservations.end()) continue;
//
//                resentPositionData.secondResendObservations.emplace(
//                        **positionIt,
//                        SecondResendGatherPositionObservations{
//                                (uint8_t)std::distance(positionsInHistory.resendPositionIts[0], positionIt)});
//
//#if OPTIMALPOSITIONS_DEBUG_VERBOSE
//                CherryVis::log(worker->id) << "Added metadata for " << resentPositionData
//                                           << " : " << **positionIt
//                                           << ", delta " << std::distance(positionsInHistory.resendPositionIts[0], positionIt);
//#endif
//            }
        }

        void updateTenDistancePosition(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory, bool switchedPatch)
        {
            // Update 10-distance position
            // For simplicity we track all of them we encounter even though there is some overlap with the "main" path data
            if (positionsInHistory.tenDistancePositionIt != workerStatus.positionHistory.end() &&
                (positionsInHistory.resendItsBeforeArrival.empty() ||
                 std::distance(positionsInHistory.tenDistancePositionIt, positionsInHistory.resendItsBeforeArrival[0]) > 0))
            {
#if TAKEOVER_DEBUG
                auto result = tenDistancePositionsFor(workerStatus.resource).insert(**positionsInHistory.tenDistancePositionIt);
                if (result.second)
                {
                    CherryVis::log(workerStatus.worker->id) << "Added new 10-distance position " << **positionsInHistory.tenDistancePositionIt;
                }
#else
                tenDistancePositionsFor(workerStatus.resource).insert(**positionsInHistory.tenDistancePositionIt);
#endif
            }
        }
    }

    void flushGatherObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses)
    {
        if (currentFrame == 0) miningWorkers.clear();

#if OPTIMALPOSITIONS_DEBUG
        if (currentFrame == 0)
        {
            exploredPatches.clear();
            collisions = 0;
            noncollisions = 0;
        }
        else if (currentFrame % 1000 == 0 && WorkerMiningOptimization::isExploring())
        {
            Log::Get() << "Explored " << exploredPatches.size() << " patch(es)";
            if ((collisions + noncollisions) > 0)
            {
                Log::Get() << std::fixed << std::setprecision(1)
                           << "Gather collision rate: " << (100.0 * collisions) / (double)(collisions + noncollisions)
                           << "% over " << (collisions + noncollisions) << " collections";
            }
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
                           << "Gathers with path data: " << (100.0 * hadPathData) / (double)total
                           << "% over " << total << " collections";
            }
        }
#endif

        // Update collision state for workers that are finished mining
        for (auto it = miningWorkers.begin(); it != miningWorkers.end();)
        {
            auto &worker = it->worker;
            if (!worker->exists())
            {
                it = miningWorkers.erase(it);
                continue;
            }

            // Wait until the worker started carrying a resource 8 frames ago
            if (!worker->carryingResource || worker->lastCarryingResourceChange != (currentFrame - 8))
            {
                it++;
                continue;
            }

            // Skip the worker if it has been ordered to do something else in the meantime
            if (worker->bwapiUnit->getOrder() != BWAPI::Orders::ReturnMinerals)
            {
#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Not tracking collision and speed observation, as the worker has apparently been re-ordered";
#endif
                it = miningWorkers.erase(it);
                continue;
            }

            handlePossiblePatchCollision(*it);

            // Don't need to track this any more
            it = miningWorkers.erase(it);
        }

        // Flush the worker statuses for workers that have started mining
        for (auto it = workerGatherStatuses.begin(); it != workerGatherStatuses.end();)
        {
            auto &worker = it->first;
            if (!worker->exists())
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            // Workers that have already been in the WaitForMinerals state while another was still mining are not run through the logic again
            if (it->second.waitForMineralsWhileOtherStillMining)
            {
                // If the worker is now mining, clean up
                if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    it = workerGatherStatuses.erase(it);
                }
                else
                {
                    it++;
                }
                continue;
            }

            if (worker->bwapiUnit->getOrder() != BWAPI::Orders::WaitForMinerals)
            {
                it++;
                continue;
            }

            // Mark workers that reached WaitForMinerals while another was waiting to mine
            if (it->second.lastProcessedFrame == currentFrame)
            {
                it->second.waitForMineralsWhileOtherStillMining = true;
                if (!WorkerMiningOptimization::isExploring() || !it->second.pathStartsAtDepot)
                {
                    it++;
                    continue;
                }
            }
            else if (!it->second.pathStartsAtDepot)
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }
            else
            {
                // Add the final position to the history
                it->second.appendCurrentPosition();
            }

            // We skip processing this worker if it hasn't tracked its positions history correctly
            PositionsInHistory positionsInHistory;
            if (!extractPositionsInHistory(positionsInHistory, it->second, WorkerMiningOptimization::isExploring())
                || positionsInHistory.arrivalPositionIt == it->second.positionHistory.end())
            {
                if (it->second.waitForMineralsWhileOtherStillMining)
                {
                    it++;
                }
                else
                {
                    it = workerGatherStatuses.erase(it);
                }
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

            if (WorkerMiningOptimization::isExploring())
            {
                updateApproachOptimization(it->second, positionsInHistory);

                // Tracking of 10-distance positions for paths that don't start at the depot
                if (!it->second.switchedPatches)
                {
                    updateTenDistancePosition(it->second, positionsInHistory, false);
                }

                if (it->second.waitForMineralsWhileOtherStillMining)
                {
                    it++;
                    continue;
                }
            }

            // We don't need to do any more if the worker switched patches
            if (it->second.switchedPatches)
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            // Move required fields into the MiningWorker struct that we use to track patch collisions
            // As the underlying vectors may change in the meantime, we convert pointers to positions
            auto convertToPositions = [](const std::vector<GatherPositionObservationPtr> &source)
            {
                std::vector<PositionAndVelocity> result;
                result.reserve(source.size());
                for (const auto &sourcePos : source)
                {
                    result.emplace_back(sourcePos.position());
                }
                return result;
            };
            miningWorkers.emplace_back(MiningWorker{
                    std::move(it->second.worker),
                    std::move(it->second.resource),
                    convertToPositions(positionsInHistory.positionHistory),
                    convertToPositions(positionsInHistory.resendsWithObservationData),
                    positionsInHistory.resendItsBeforeArrival.size()});

            // We now no longer need to do anything with this worker status
            it = workerGatherStatuses.erase(it);
        }
    }

    void handleGatherPatchSwitch(WorkerGatherStatus &workerStatus)
    {
        if (workerStatus.waitForMineralsWhileOtherStillMining) return;

        PositionsInHistory positionsInHistory;
        if (!extractPositionsInHistory(positionsInHistory, workerStatus, WorkerMiningOptimization::isExploring())) return;

        updateTenDistancePosition(workerStatus, positionsInHistory, true);
    }
}