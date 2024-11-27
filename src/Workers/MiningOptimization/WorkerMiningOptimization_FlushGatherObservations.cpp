// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "GatherPositionObservations.h"
#include "WorkerGatherStatus.h"

#include "Geo.h"
#include "OrderProcessTimer.h"


namespace WorkerMiningOptimization
{
    namespace
    {
        struct PositionsInHistory
        {
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator firstMovedPositionIt;
            std::vector<std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator> resendPositionIts;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator arrivalPositionIt;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator tenDistancePositionIt;

            std::vector<std::shared_ptr<const PositionAndVelocity>> resendsBeforeArrival;
        };

        bool extractPositionsInHistory(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory)
        {
            // If the path is too short to possibly optimize, return here
            // This might happen if we have a case where the worker gets reassigned or otherwise doesn't follow a normal mining path
            if (workerStatus.positionHistory.size() < (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                return false;
            }

            positionsInHistory.firstMovedPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.resendPositionIts.clear();
            positionsInHistory.arrivalPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.tenDistancePositionIt = workerStatus.positionHistory.end();

            positionsInHistory.resendsBeforeArrival.clear();

            // Don't process histories over 60 positions, as this indicates either distance mining or some kind of weird pathing error
            if (workerStatus.positionHistory.size() > 60) return false;

            int headingBeforeMiningStart = (*(workerStatus.positionHistory.rbegin() + 1))->heading;

            auto nextResendPositionIt = workerStatus.resentPositions.begin();
            auto firstPos = (*workerStatus.positionHistory.begin())->pos();
            for (auto it = workerStatus.positionHistory.begin(); it != workerStatus.positionHistory.end(); it++)
            {
                if (nextResendPositionIt != workerStatus.resentPositions.end() && **nextResendPositionIt == **it)
                {
                    positionsInHistory.resendPositionIts.push_back(it);
                    nextResendPositionIt++;
                }

                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    (*it)->pos(),
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    workerStatus.resource->center);

                // Arrival position is defined as the position where:
                // - distance to the patch is 0
                // - position is the same as the position at mining start
                // - heading is the same as the heading immediately prior to mining start, unless this is the mining start position
                if (positionsInHistory.arrivalPositionIt == workerStatus.positionHistory.end()
                    && dist == 0
                    && workerStatus.worker->lastPosition == (*it)->pos()
                    && ((*it)->heading == headingBeforeMiningStart || (it + 1) == workerStatus.positionHistory.end()))
                {
                    positionsInHistory.arrivalPositionIt = it;
                }

                if (positionsInHistory.tenDistancePositionIt == workerStatus.positionHistory.end() && dist <= 10)
                {
                    positionsInHistory.tenDistancePositionIt = it - BWAPI::Broodwar->getLatencyFrames() - 1;
                }

                if (positionsInHistory.firstMovedPositionIt == workerStatus.positionHistory.end() &&
                    (it + 1) != workerStatus.positionHistory.end())
                {
                    // We define the "first moved position" as the first position at least 2 pixels from the initial position
                    // where the worker is moving
                    if ((*it)->pos().getApproxDistance(firstPos) >= 2 && (*it)->pos() != (*(it + 1))->pos())
                    {
                        positionsInHistory.firstMovedPositionIt = it;
#if OPTIMALRETURN_DEBUG
                        CherryVis::log(workerStatus.worker->id)
                                << "First move position at delta " << std::distance(workerStatus.positionHistory.begin(), it)
                                << " from first position";
#endif
                    }
                }
            }

            // Clear the ten distance position iterator if it is invalid
            if (positionsInHistory.tenDistancePositionIt != workerStatus.positionHistory.end() &&
                std::distance(workerStatus.positionHistory.begin(), positionsInHistory.tenDistancePositionIt) < 0)
            {
                positionsInHistory.tenDistancePositionIt = workerStatus.positionHistory.end();
            }

            if (positionsInHistory.firstMovedPositionIt == workerStatus.positionHistory.end())
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "ERROR: Couldn't find first gather move position in history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
#endif
                return false;
            }

            // Return false if any of the resend positions couldn't be found
            if (workerStatus.resentPositions.size() != positionsInHistory.resendPositionIts.size())
            {
                Log::Get() << "ERROR: Not all resent positions found in position history"
                           << "; worker id " << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition();
                return false;
            }

            // Create the filtered vectors with just resends that happened before arrival at the patch
            for (int i = 0; i < positionsInHistory.resendPositionIts.size(); i++)
            {
                if (std::distance(positionsInHistory.resendPositionIts[i], positionsInHistory.arrivalPositionIt)
                    < BWAPI::Broodwar->getLatencyFrames())
                {
                    break;
                }

                positionsInHistory.resendsBeforeArrival.push_back(workerStatus.resentPositions[i]);
            }

            return true;
        }

        // Used to track whether a mining worker collides with the patch after mining
        struct MiningWorker
        {
            MyWorker worker;
            Resource resource;
            std::vector<std::shared_ptr<const PositionAndVelocity>> positionHistory;
            std::vector<std::shared_ptr<const PositionAndVelocity>> resentPositions;
        };

        std::vector<MiningWorker> miningWorkers;

#if OPTIMALPOSITIONS_DEBUG
        std::set<uint32_t> exploredPaths;
        std::set<BWAPI::TilePosition> exploredPatches;
        int collisions = 0;
        int noncollisions = 0;
#endif

        void handlePossiblePatchCollision(const MiningWorker &miningWorker)
        {
            // There is a collision if the worker isn't moving
            bool collision = (currentFrame - miningWorker.worker->frameLastMoved) > 2;

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

            // If there have been more than two resends, we can't trust the data
            if (miningWorker.resentPositions.size() > 2) return;

            // Update the stats on the appropriate position metadata
            auto &optimalGatherPositions = optimalGatherPositionsFor(miningWorker.resource);

            // If no resend occurred, update all positions
            if (miningWorker.resentPositions.empty())
            {
                for (const auto &position : miningWorker.positionHistory)
                {
                    auto metadataIt = optimalGatherPositions.find(*position);
                    if (metadataIt != optimalGatherPositions.end())
                    {
                        (collision ? metadataIt->second.noResendCollisions : metadataIt->second.noResendNonCollisions)++;
                    }
                }
                return;
            }

            auto resendMetadataIt = optimalGatherPositions.find(*miningWorker.resentPositions[0]);
            if (resendMetadataIt == optimalGatherPositions.end()) // should never happen
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "ERROR: Resend metadata not found for " << *miningWorker.resentPositions[0]
                           << "; worker id " << miningWorker.worker->id << " @ " << miningWorker.worker->getTilePosition();
#endif
                return;
            }

            auto updateObservations = [&collision](GatherResendArrivalObservations &observations)
            {
                (collision ? observations.collisions : observations.nonCollisions)++;
            };

            // Update no second resend observations if there has not been a second resend
            if (miningWorker.resentPositions.size() == 1)
            {
                updateObservations(resendMetadataIt->second.noSecondResendArrivalObservations);
                return;
            }

            // If the initial resend didn't change the path, look up the second resend position in the normal positions set
            if (resendMetadataIt->second.resendChangesPath == ResendChangesPath::No)
            {
                auto secondResendObservationsIt = optimalGatherPositions.find(*miningWorker.resentPositions[1]);
                if (secondResendObservationsIt == optimalGatherPositions.end()) // should never happen
                {
#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "ERROR: Resend metadata for second resend not found for " << *miningWorker.resentPositions[0]
                               << " : " << *miningWorker.resentPositions[1]
                               << "; worker id " << miningWorker.worker->id << " @ " << miningWorker.worker->getTilePosition();
#endif
                    return;
                }

                updateObservations(secondResendObservationsIt->second.noSecondResendArrivalObservations);
                return;
            }

            auto secondResendObservations = resendMetadataIt->second.secondResendObservationsFor(miningWorker.resentPositions[1].get());
            if (!secondResendObservations) // should never happen
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "ERROR: Second resend metadata not found for " << *miningWorker.resentPositions[0]
                           << " : " << *miningWorker.resentPositions[1]
                           << "; worker id " << miningWorker.worker->id << " @ " << miningWorker.worker->getTilePosition();
#endif
                return;
            }

            updateObservations(secondResendObservations->arrivalObservations);
        }

        void updateNextPositions(WorkerGatherStatus &workerStatus,
                                 PositionsInHistory &positionsInHistory)
        {
            if (!workerStatus.pathStartsAtDepot) return;

            auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);

            // Helper to ensure a given iterator doesn't exceed the given limit
            auto clampToLimit = [](auto it, auto limit)
            {
                if (std::distance(it, limit) < 0)
                {
                    return limit;
                }
                return it;
            };

            // Include LF positions after the resend since the positions only change after the command kicks in
            bool passedExistingPosition = false;
            auto limit = positionsInHistory.arrivalPositionIt;
            if (!positionsInHistory.resendsBeforeArrival.empty())
            {
                limit = clampToLimit(
                        positionsInHistory.resendPositionIts[0] + BWAPI::Broodwar->getLatencyFrames() + 1, positionsInHistory.arrivalPositionIt);
            }
            for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != limit; positionIt++)
            {
                auto metadataIt = optimalGatherPositions.find(**positionIt);
                if (metadataIt == optimalGatherPositions.end())
                {
                    // We create default metadata for anything we create a next link to, but not anything that comes before
                    if (!passedExistingPosition) continue;

                    metadataIt = optimalGatherPositions.emplace(
                            **positionIt,
                            GatherPositionObservations(
                                    workerStatus.pathStartsAtDepot ? (*positionsInHistory.firstMovedPositionIt)->previousPositionsHash : 0,
                                    **positionIt)
                    ).first;
                }
                else
                {
                    passedExistingPosition = true;
                }

                auto &positionMetadata = metadataIt->second;

                if ((positionIt + 1) != limit)
                {
                    positionMetadata.nextPositionAndOccurrences[**(positionIt + 1)]++;
                }

                // Only record next positions for second resend if we actually track them here
                if (positionMetadata.resendChangesPath != ResendChangesPath::Yes) continue;

                // Add metadata for second resend positions
                // If this isn't the resend position, we add following positions up to LF
                // If this is the resend position, we add following positions until the second resend + LF
                auto secondLimit = clampToLimit(
                        positionIt + BWAPI::Broodwar->getLatencyFrames() + 1,
                        positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames());
                if (!positionsInHistory.resendsBeforeArrival.empty() && std::distance(positionsInHistory.resendPositionIts[0], positionIt) == 0)
                {
                    if (positionsInHistory.resendsBeforeArrival.size() > 1)
                    {
                        secondLimit = clampToLimit(
                                positionsInHistory.resendPositionIts[1] + BWAPI::Broodwar->getLatencyFrames() + 1,
                                positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames());
                    }
                    else
                    {
                        secondLimit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
                    }
                }
                for (auto secondPositionIt = positionIt + 1; std::distance(secondPositionIt, secondLimit) > 0; secondPositionIt++)
                {
                    auto secondResendObservationsIt = positionMetadata.secondResendObservations.find(**secondPositionIt);
                    if (secondResendObservationsIt == positionMetadata.secondResendObservations.end())
                    {
                        secondResendObservationsIt = positionMetadata.secondResendObservations.emplace(
                                **secondPositionIt,
                                SecondResendGatherPositionObservations{
                                        **secondPositionIt,
                                        (uint16_t)std::distance(positionIt, secondPositionIt)}).first;
                    }

                    if ((secondPositionIt + 1) != secondLimit)
                    {
                        secondResendObservationsIt->second.nextPositionAndOccurrences[**(secondPositionIt + 1)]++;
                    }
                }
            }
        }

        void updateApproachOptimization(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory)
        {
#if OPTIMALPOSITIONS_DEBUG
            auto &worker = workerStatus.worker;
#endif

            // Ensure we have enough position history to perform the optimization
            if (workerStatus.positionHistory.size() <
                (std::distance(positionsInHistory.arrivalPositionIt, workerStatus.positionHistory.end()) + BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                return;
            }

            // If a third resend took effect before arrival at the patch, we bail out here
            // Third resends can happen when the worker is being takeover-optimized
            if (positionsInHistory.resendsBeforeArrival.size() > 2)
            {
                return;
            }

            auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);

            // Iterator to the apparent optimal position in the position history
            auto optimalPositionIt = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames() - 11;

#if OPTIMALPOSITIONS_DEBUG
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

            // If we sent no command, record the path for exploration
            if (positionsInHistory.resendsBeforeArrival.empty())
            {
                // Get the "path hash", which is the hash of the first position the worker moved in the stored path
                uint32_t pathHash = (*positionsInHistory.firstMovedPositionIt)->previousPositionsHash;

                // Update the metadata for the positions in the path
                for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != positionsInHistory.arrivalPositionIt; positionIt++)
                {
                    auto delta = (int16_t)std::distance(optimalPositionIt, positionIt);

                    auto existingIt = optimalGatherPositions.find(**positionIt);

                    if (existingIt != optimalGatherPositions.end())
                    {
#if OPTIMALPOSITIONS_DEBUG
                        if (!existingIt->second.deltaToBenchmarkAndOccurrences.contains(delta))
                        {
                            CherryVis::log(worker->id) << "New delta of " << delta << " came up for " << existingIt->second;
                        }
#endif
                        existingIt->second.deltaToBenchmarkAndOccurrences[delta]++;
                        continue;
                    }

                    // Create a position here for exploring
                    if (!workerStatus.pathStartsAtDepot && delta != 0) continue;

                    optimalGatherPositions.emplace(
                            **positionIt,
                            GatherPositionObservations(
                                    workerStatus.pathStartsAtDepot ? pathHash : 0,
                                    **positionIt,
                                    delta)
                    );

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Added metadata for " << **positionIt << " at delta " << delta;
#endif
                }
                return;
            }

            // Get the data for the resent position
            auto resentPositionDataIt = optimalGatherPositions.find(*positionsInHistory.resendsBeforeArrival[0]);
            if (resentPositionDataIt == optimalGatherPositions.end())
            {
                // This can happen if we are exploring takeover positions or if workers are competing to get to a patch first
                // We create an entry with placeholders for path hash and delta
                resentPositionDataIt = optimalGatherPositions.emplace(
                        *positionsInHistory.resendsBeforeArrival[0],
                        GatherPositionObservations(
                                workerStatus.pathStartsAtDepot ? UINT32_MAX : 0,
                                *positionsInHistory.resendsBeforeArrival[0])
                ).first;
            }
            auto &resentPositionData = resentPositionDataIt->second;

            // If there was a second resend, reference its metadata
            SecondResendGatherPositionObservations *secondResendData = nullptr;
            if (positionsInHistory.resendsBeforeArrival.size() > 1)
            {
                secondResendData = resentPositionData.secondResendObservationsFor(positionsInHistory.resendsBeforeArrival[1].get());
                if (!secondResendData)
                {
                    auto secondResendObservationsIt = resentPositionData.secondResendObservations.emplace(
                            *positionsInHistory.resendsBeforeArrival[1],
                            SecondResendGatherPositionObservations{
                                    *positionsInHistory.resendsBeforeArrival[1],
                                    (uint16_t)std::distance(positionsInHistory.resendPositionIts[0], positionsInHistory.resendPositionIts[1])}).first;

                    secondResendData = &secondResendObservationsIt->second;
                }
            }

            auto lastResendPositionIt = positionsInHistory.resendPositionIts[(positionsInHistory.resendsBeforeArrival.size() > 1) ? 1 : 0];

#if OPTIMALPOSITIONS_DEBUG
            bool exploring = resentPositionData.addArrivalObservation(
                    secondResendData,
                    (int)std::distance(lastResendPositionIt, optimalPositionIt));

            if (secondResendData)
            {
                CherryVis::log(worker->id) << "Added observation of " << resentPositionData
                                           << " : " << secondResendData->pos;
            }
            else
            {
                CherryVis::log(worker->id) << "Added observation of " << resentPositionData;
            }

            // If this resend was not exploring a new position, do some validation of whether the timing matched our expectations
            if (!exploring && (!workerStatus.plannedSecondResendPosition || (positionsInHistory.resendsBeforeArrival.size() > 1)))
            {
                // Measure effectiveness of this gather path

                int arrivalDelay = secondResendData
                                   ? secondResendData->arrivalObservations.mostCommonArrivalDelay()
                                   : resentPositionData.noSecondResendArrivalObservations.mostCommonArrivalDelay();

                auto actualFramesToArrival = std::distance(lastResendPositionIt, positionsInHistory.arrivalPositionIt);
                if (actualFramesToArrival != (BWAPI::Broodwar->getLatencyFrames() + 11 + arrivalDelay))
                {
                    Log::Get() << "ERROR: Position " << resentPositionData << " has unexpected arrival delta"
                               << "; expected=" << (BWAPI::Broodwar->getLatencyFrames() + 10 + arrivalDelay)
                               << "; actual=" << actualFramesToArrival
                               << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                }

                // We can only predict frames to mining when not taking over from another worker
                if (workerStatus.takeoverState == 0)
                {
                    auto actualFramesToMining = (int)std::distance(lastResendPositionIt, workerStatus.positionHistory.end()) - 1;

                    int noResetExpectedFramesToMining = (BWAPI::Broodwar->getLatencyFrames() + 11);
                    if (arrivalDelay > 0)
                    {
                        noResetExpectedFramesToMining += 9 * (((arrivalDelay - 1) / 9) + 1);
                    }

                    auto framesToReset =
                            OrderProcessTimer::framesToNextReset(currentFrame - actualFramesToMining + BWAPI::Broodwar->getLatencyFrames() + 1);

                    if (framesToReset == 0)
                    {
                        Log::Get() << "ERROR: Sent command 4 frames before order process timer reset"
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                    }

                    framesToReset += BWAPI::Broodwar->getLatencyFrames();

                    int minExpectedFramesToMining, maxExpectedFramesToMining;
                    if (framesToReset < actualFramesToArrival)
                    {
                        // Reset prior to arrival
                        minExpectedFramesToMining = (BWAPI::Broodwar->getLatencyFrames() + 11 + arrivalDelay);
                        maxExpectedFramesToMining = minExpectedFramesToMining + 9;
                    }
                    else if (framesToReset < noResetExpectedFramesToMining)
                    {
                        // Reset between arrival and start of mining
                        minExpectedFramesToMining = framesToReset + 1;
                        maxExpectedFramesToMining = minExpectedFramesToMining + 8;
                    }
                    else
                    {
                        minExpectedFramesToMining = maxExpectedFramesToMining = noResetExpectedFramesToMining;
                    }
                    if (actualFramesToMining < minExpectedFramesToMining || actualFramesToMining > maxExpectedFramesToMining)
                    {
                        Log::Get() << "ERROR: Position " << resentPositionData << " has unexpected mining start delta"
                                   << "; expected=" << minExpectedFramesToMining << "-" << maxExpectedFramesToMining
                                   << "; actual=" << actualFramesToMining
                                   << "; framesToReset=" << framesToReset
                                   << "; framesToArrival=" << actualFramesToArrival
                                   << "; framesToNoResetMining=" << noResetExpectedFramesToMining
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                    }
                }
            }
            else
            {
                exploredPaths.insert(resentPositionData.pathHash);
                exploredPatches.insert(workerStatus.resource->tile);
            }
#else
            // Track the observation
            resentPositionData.addArrivalObservation(secondResendData, (int)std::distance(lastResendPositionIt, optimalPositionIt));
#endif

            // Consider exploration of second resend positions
            // This is not needed if:
            // - There was a second resend
            // - The information for this path is incomplete
            // - This position is not inside our optimization horizon
            // - The path did not start at the depot
            if (secondResendData) return;
            if (resentPositionData.deltaToBenchmarkAndOccurrences.empty()) return;

            int probableDeltaToBenchmark = resentPositionData.probableDeltaToBenchmark();
            if (probableDeltaToBenchmark < -GATHER_EXPLORE_BEFORE) return;
            if (probableDeltaToBenchmark > GATHER_EXPLORE_AFTER) return;

            if (!workerStatus.pathStartsAtDepot) return;

            // Check if the path after the resend is the same as the path without a resend
            // If so, we don't bother tracking second resends on this, as they will be the same as the normal path
            auto pathsMatch = [&]()
            {
                if (resentPositionData.resendChangesPath == ResendChangesPath::Yes) return false;

                auto noResendPath = resentPositionData.followingPositionsIfStable(optimalGatherPositions);
                if (noResendPath.empty()) return false; // No resend path is unstable

                size_t noResendPathIdx = 0;
                for (auto positionIt = positionsInHistory.resendPositionIts[0] + 1; positionIt != positionsInHistory.arrivalPositionIt; positionIt++)
                {
                    if (noResendPath[noResendPathIdx]->pos != **positionIt)
                    {
                        return false;
                    }

                    noResendPathIdx++;
                    if (noResendPathIdx >= noResendPath.size()) break;
                }
                return true;
            };

            if (pathsMatch())
            {
                resentPositionData.resendChangesPath = ResendChangesPath::No;
                return;
            }

            // If this is the first detection of a changed path, add the existing next positions as a second resend position
            if (resentPositionData.resendChangesPath != ResendChangesPath::Yes)
            {
                resentPositionData.resendChangesPath = ResendChangesPath::Yes;

                for (const auto &[nextPosition, _] : resentPositionData.nextPositionAndOccurrences)
                {
                    auto secondResendObservationsIt = resentPositionData.secondResendObservations.find(nextPosition);
                    if (secondResendObservationsIt == resentPositionData.secondResendObservations.end())
                    {
                        resentPositionData.secondResendObservations.emplace(
                                nextPosition,
                                SecondResendGatherPositionObservations{nextPosition, 1});

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Added metadata for " << resentPositionData
                                                   << " : " << nextPosition
                                                   << " after discovering unstable path after resends";
#endif
                    }
                }
            }

            // Queue up second resend positions to explore
            auto limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
            for (auto positionIt = positionsInHistory.resendPositionIts[0] + 1; std::distance(positionIt, limit) > 0; positionIt++)
            {
                auto secondResendObservationsIt = resentPositionData.secondResendObservations.find(**positionIt);
                if (secondResendObservationsIt != resentPositionData.secondResendObservations.end()) continue;

                resentPositionData.secondResendObservations.emplace(
                        **positionIt,
                        SecondResendGatherPositionObservations{
                                **positionIt,
                                (uint16_t)std::distance(positionsInHistory.resendPositionIts[0], positionIt)});

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Added metadata for " << resentPositionData
                                           << " : " << **positionIt
                                           << ", delta " << std::distance(positionsInHistory.resendPositionIts[0], positionIt);
#endif
            }
        }

        void updateTenDistancePosition(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory, bool switchedPatch)
        {
            // Update 10-distance position
            // For simplicity we track all of them we encounter even though there is some overlap with the "main" path data
            if (positionsInHistory.tenDistancePositionIt != workerStatus.positionHistory.end() &&
                (positionsInHistory.resendsBeforeArrival.empty() ||
                 std::distance(positionsInHistory.tenDistancePositionIt, positionsInHistory.resendPositionIts[0]) > 0))
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
            exploredPaths.clear();
            exploredPatches.clear();
            collisions = 0;
            noncollisions = 0;
        }
        else if (currentFrame % 1000 == 0)
        {
            Log::Get() << "Explored " << exploredPaths.size() << " path(s) over " << exploredPatches.size() << " patch(es)";
            if ((collisions + noncollisions) > 0)
            {
                Log::Get() << std::fixed << std::setprecision(1)
                           << "Gather collision rate: " << (100.0 * collisions) / (double)(collisions + noncollisions)
                           << "% over " << (collisions + noncollisions) << " collections";
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

            // Skip the worker if it has been ordered to do something else in the meantime or isn't moving to return
            if (worker->bwapiUnit->getLastCommandFrame() >= (BWAPI::Broodwar->getFrameCount() - 8 - BWAPI::Broodwar->getLatencyFrames()) ||
                worker->bwapiUnit->getOrder() != BWAPI::Orders::ReturnMinerals)
            {
#if OPTIMALRETURN_DEBUG
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

            if (worker->bwapiUnit->getOrder() != BWAPI::Orders::WaitForMinerals)
            {
                it++;
                continue;
            }

            // Add the final position to the history
            it->second.appendCurrentPosition();

            // We skip processing this worker if it has switched patches (in which case we've already observed what we could) or if
            // we for some reason haven't tracked its positions history correctly
            PositionsInHistory positionsInHistory;
            if (it->second.switchedPatches || !extractPositionsInHistory(it->second, positionsInHistory)
                || positionsInHistory.arrivalPositionIt == it->second.positionHistory.end())
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            updateApproachOptimization(it->second, positionsInHistory);

            // Tracking of 10-distance positions for paths that don't start at the depot
            updateTenDistancePosition(it->second, positionsInHistory, false);

            updateNextPositions(it->second, positionsInHistory);

            // Move required fields into the MiningWorker struct that we use to track patch collisions
            miningWorkers.emplace_back(MiningWorker{
                    std::move(it->second.worker),
                    std::move(it->second.resource),
                    std::move(it->second.positionHistory),
                    std::move(positionsInHistory.resendsBeforeArrival)});

            // We now no longer need to do anything with this worker status
            it = workerGatherStatuses.erase(it);
        }
    }

    void handleGatherPatchSwitch(WorkerGatherStatus &workerStatus)
    {
        PositionsInHistory positionsInHistory;
        if (!extractPositionsInHistory(workerStatus, positionsInHistory)) return;

        updateTenDistancePosition(workerStatus, positionsInHistory, true);

        // TODO: Once the optimization is refactored, reconsider whether any other observations are needed here
    }
}