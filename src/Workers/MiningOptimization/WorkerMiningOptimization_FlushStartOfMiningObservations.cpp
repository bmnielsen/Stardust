// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "PositionObservationMetadata.h"
#include "WorkerGatherStatus.h"

#include "Geo.h"
#include "OrderProcessTimer.h"


namespace WorkerMiningOptimization
{
    namespace
    {
        /*
        void updateTakeoverMetadata(WorkerGatherStatus &workerStatus,
                                    const Resource &resource,
                                    std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                    std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions,
                                    bool switchedPatches = false)
        {
            // Abort if we've already cleared our history
            if (workerStatus.positionHistory.size() < (BWAPI::Broodwar->getLatencyFrames() + 11)) return;

            // Abort if we had to resend a position on a schedule, which might ruin our timings
            if (workerStatus.resentOnSchedule()) return;

            // Track 10-distance if there was no resend for optimizing arrival
            if (!workerStatus.resentPosition)
            {
                // Find the position where we were LF+1 from being 10 distance from the patch
                auto it = workerStatus.positionHistory.rbegin();
                auto beforeIt = it + BWAPI::Broodwar->getLatencyFrames();
                for (; beforeIt != workerStatus.positionHistory.rend();)
                {
                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        (*it)->pos(),
                                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                                        resource->center);
                    if (dist > 10) break;
                    it++;
                    beforeIt++;
                }
                if (it != workerStatus.positionHistory.rbegin() && beforeIt != workerStatus.positionHistory.rend())
                {
                    handleObservation(workerStatus, *beforeIt, workerStatus.passed10DistancePosition, tenDistancePositions, true);
                }
            }

            // If we switched patches, track a failure if we resent
            if (switchedPatches)
            {
                if (workerStatus.resentPosition)
                {
                    auto metadata = takeoverResendPositions.find(*workerStatus.resentPosition);
                    if (metadata == takeoverResendPositions.end())
                    {
                        metadata = takeoverResendPositions.emplace(*workerStatus.resentPosition,
                                                                   PositionObservationMetadata{*workerStatus.resentPosition}).first;
                    }
                    metadata->second.trackFailure();

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(workerStatus.worker->id) << "Tracking takeover failure by patch switch on " << metadata->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking takeover failure by patch switch on " << metadata->second;
#endif
                }

                return;
            }

            // Can't observe anything else in the case where we switched patches
            if (switchedPatches) return;

            // Track observations on any positions that would have got us to the patch in the right time frame
            bool passedOptimal = false;
            auto optimalIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
            auto resendIt = workerStatus.positionHistory.rend();
            auto secondResendIt = workerStatus.positionHistory.rend();
#if OPTIMALPOSITIONS_DEBUG
            std::ostringstream dbg;
            dbg << "Tracking takeover observation on:";
#endif
            for (auto it = workerStatus.positionHistory.rbegin(); it != workerStatus.positionHistory.rend(); it++)
            {
                // Any resends that got us to the patch in time are observed
                if (!passedOptimal)
                {
                    auto metadata = takeoverResendPositions.find(**it);
                    if (metadata == takeoverResendPositions.end())
                    {
                        metadata = takeoverResendPositions.emplace(**it, PositionObservationMetadata{**it}).first;
                    }
                    metadata->second.trackObservation();
#if OPTIMALPOSITIONS_DEBUG
                    dbg << "\n" << metadata->second;
#endif
                }

                if (workerStatus.resentPosition == *it) resendIt = it;
                if (workerStatus.secondResentPosition == *it) secondResendIt = it;

                if (it == optimalIt) passedOptimal = true;

                if (passedOptimal && (!workerStatus.resentPosition || resendIt != workerStatus.positionHistory.rend()) &&
                    (!workerStatus.secondResentPosition || secondResendIt != workerStatus.positionHistory.rend()))
                {
                    break;
                }
            }

#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(workerStatus.worker->id) << dbg.str();
            CherryVis::log(workerStatus.resource->id) << dbg.str();
#endif

            // Track success or failure of resend
            if (workerStatus.resentPosition && resendIt != workerStatus.positionHistory.rend())
            {
                auto resendPositionData = takeoverResendPositions.find(*workerStatus.resentPosition);
                if (resendPositionData == takeoverResendPositions.end())
                {
                    resendPositionData = takeoverResendPositions.emplace(*workerStatus.resentPosition,
                                                                         PositionObservationMetadata{*workerStatus.resentPosition}).first;
                }

                auto resendDelta = (int)std::distance(resendIt, optimalIt);
                auto secondResendDelta =
                        (workerStatus.secondResentPosition && secondResendIt != workerStatus.positionHistory.rend())
                        ? (int)std::distance(secondResendIt, optimalIt)
                        : INT_MAX;
                trackNonoptimalResend(resendPositionData->second,
                                      **optimalIt,
                                      workerStatus.secondResentPosition,
                                      resendDelta,
                                      secondResendDelta);

#if OPTIMALPOSITIONS_DEBUG
                if (resendDelta >= 0)
                {
                    CherryVis::log(workerStatus.worker->id) << "Tracking success on takeover " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking success on takeover " << resendPositionData->second;
                }
                else
                {
                    CherryVis::log(workerStatus.worker->id) << "Tracking failure on takeover " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking failure on takeover " << resendPositionData->second;
                }
#endif
            }
        }
         */

        struct PositionsInHistory
        {
            std::vector<std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator> resendPositionIts;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator arrivalPositionIt;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator tenDistancePositionIt;

            std::vector<std::shared_ptr<const PositionAndVelocity>> resendsBeforeArrival;
        };

        bool extractPositionsInHistory(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory)
        {
            positionsInHistory.resendPositionIts.clear();
            positionsInHistory.arrivalPositionIt = workerStatus.positionHistory.end();
            positionsInHistory.tenDistancePositionIt = workerStatus.positionHistory.end();

            positionsInHistory.resendsBeforeArrival.clear();

            auto nextResendPositionIt = workerStatus.resentPositions.begin();
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

                if (positionsInHistory.arrivalPositionIt == workerStatus.positionHistory.end()
                    && dist == 0
                    && workerStatus.worker->lastPosition == (*it)->pos())
                {
                    positionsInHistory.arrivalPositionIt = it;
                }

                if (positionsInHistory.tenDistancePositionIt == workerStatus.positionHistory.end() && dist <= 10)
                {
                    positionsInHistory.tenDistancePositionIt = it - BWAPI::Broodwar->getLatencyFrames();
                }
            }

            // Clear the ten distance position iterator if it is invalid
            if (positionsInHistory.tenDistancePositionIt != workerStatus.positionHistory.end() &&
                std::distance(workerStatus.positionHistory.begin(), positionsInHistory.tenDistancePositionIt) < 0)
            {
                positionsInHistory.tenDistancePositionIt = workerStatus.positionHistory.end();
            }

            // Return false if any of the resend positions or the arrival position couldn't be found
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
            int takeoverState;
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
            if (resendMetadataIt == optimalGatherPositions.end())
            {
                // Should only happen in takeover scenarios
#if OPTIMALPOSITIONS_DEBUG
                if (miningWorker.takeoverState == 0)
                {
                    Log::Get() << "ERROR: Resend metadata not found for " << *miningWorker.resentPositions[0]
                               << "; worker id " << miningWorker.worker->id << " @ " << miningWorker.worker->getTilePosition();
                }
#endif
                return;
            }

            SecondResendPositionObservationMetadata *secondResendMetadata = nullptr;
            if (miningWorker.resentPositions.size() > 1)
            {
                secondResendMetadata = resendMetadataIt->second.secondResendMetadataFor(miningWorker.resentPositions[1].get());
                if (!secondResendMetadata) // may happen when a new branch is detected and we send at the same delta
                {
                    return;
                }
            }

            auto &observations = (secondResendMetadata ? secondResendMetadata->observations : resendMetadataIt->second.noSecondResendObservations);
            (collision ? observations.collisions : observations.nonCollisions)++;
        }

        void updateNextPositions(WorkerGatherStatus &workerStatus,
                                 std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &metadata,
                                 PositionsInHistory &positionsInHistory)
        {
            if (!workerStatus.pathStartsAtDepot()) return;

            // Include LF-1 positions after the resend since the positions only change after the command kicks in
            auto limit = positionsInHistory.arrivalPositionIt;
            if (!positionsInHistory.resendsBeforeArrival.empty())
            {
                limit = (positionsInHistory.resendPositionIts[0]) + BWAPI::Broodwar->getLatencyFrames();
            }
            for (auto positionIt = workerStatus.positionHistory.begin();
                 positionIt != limit && positionIt != workerStatus.positionHistory.end();
                 positionIt++)
            {
                auto metadataIt = metadata.find(**positionIt);
                if (metadataIt == metadata.end()) continue;

                auto &positionMetadata = metadataIt->second;

                if ((positionIt + 1) != workerStatus.positionHistory.end() && metadata.find(**(positionIt + 1)) != metadata.end())
                {
                    positionMetadata.next[**(positionIt + 1)]++;
                }

                // Add metadata for second resend positions
                // If this isn't the resend position, we add following positions up to LF-1
                // If this is the resend position, we add following positions until the second resend + LF-1
                int secondLimit = (BWAPI::Broodwar->getLatencyFrames() - 1);
                if (!positionsInHistory.resendsBeforeArrival.empty() && std::distance(positionsInHistory.resendPositionIts[0], positionIt) == 0)
                {
                    if (positionsInHistory.resendsBeforeArrival.size() > 1)
                    {
                        secondLimit +=
                                (int)std::distance(positionsInHistory.resendPositionIts[0], positionsInHistory.resendPositionIts[1]);
                    }
                    else
                    {
                        secondLimit = EXPLORE_AFTER - positionMetadata.deltaToNormalPathOptimalPosition + EXPLORE_SECOND_RESEND_POSITIONS;
                    }
                }
                for (int secondResendIndex = 1; secondResendIndex <= secondLimit; secondResendIndex++)
                {
                    auto here = positionIt + secondResendIndex;
                    if (here == workerStatus.positionHistory.end()) break;

                    auto next = (secondResendIndex < secondLimit)
                            ? (positionIt + secondResendIndex + 1)
                            : workerStatus.positionHistory.end();

                    auto secondResendMetadataIt = positionMetadata.secondResendMetadata.find(**here);
                    if (secondResendMetadataIt == positionMetadata.secondResendMetadata.end())
                    {
                        auto nextPositions =
                                (next == workerStatus.positionHistory.end())
                                ? std::unordered_map<PositionAndVelocity, int>{}
                                : std::unordered_map<PositionAndVelocity, int>{{**next, 1}};
                        positionMetadata.secondResendMetadata.emplace(
                                **here,
                                SecondResendPositionObservationMetadata{
                                    **here,
                                    std::move(nextPositions),
                                    secondResendIndex});
                    }
                    else if (next != workerStatus.positionHistory.end())
                    {
                        secondResendMetadataIt->second.next[**next]++;
                    }
                }
            }
        }

        void updateApproachOptimization(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory)
        {
            auto &worker = workerStatus.worker;

            // We don't optimize the case where the worker switched patches
            if (workerStatus.switchedPatches) return;

            // Ensure we have enough position history to perform the optimization
            if (workerStatus.positionHistory.size() <
                (std::distance(positionsInHistory.arrivalPositionIt, workerStatus.positionHistory.end()) + BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                return;
            }

            auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);

            updateNextPositions(workerStatus, optimalGatherPositions, positionsInHistory);

            // If a third resend took effect before arrival at the patch, we bail out here
            // Third resends can happen when the worker is being takeover-optimized
            if (positionsInHistory.resendsBeforeArrival.size() > 2)
            {
                return;
            }

            // Iterator to the apparent optimal position in the position history
            auto optimalPositionIt = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames() - 11;

#if OPTIMALPOSITIONS_DEBUG
#if OPTIMALPOSITIONS_DEBUG_VERBOSE
            {
                std::ostringstream dbg;
                dbg << "Position history:";
                for (auto positionIt = optimalPositionIt + 5; positionIt >= optimalPositionIt - 5; positionIt--)
                {
                    dbg << "\n" << **positionIt;
                }
                CherryVis::log(worker->id) << dbg.str();
            }
#endif

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
                // If there is already metadata for the optimal position, then it was a decision by the optimizer not to resend there and
                // nothing further is needed
                auto optimalPositionDataIt = optimalGatherPositions.find(**optimalPositionIt);
                if (optimalPositionDataIt != optimalGatherPositions.end())
                {
#if OPTIMALPOSITIONS_DEBUG
                    if (optimalPositionDataIt->second.deltaToNormalPathOptimalPosition != 0)
                    {
                        Log::Get() << "ERROR: Position " << optimalPositionDataIt->second << " came up out of order"
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                    }
#endif
                    return;
                }

                // If the path did not start at the depot, we don't try to explore surrounding positions
                // The rationale for this is that paths not starting at the depot are not likely to come up often,
                // so we can't explore them efficiently
                if (!workerStatus.pathStartsAtDepot())
                {
                    optimalGatherPositions.emplace(
                            **optimalPositionIt,
                            PositionObservationMetadata{
                                    (*optimalPositionIt)->previousPositionsHash,
                                    **optimalPositionIt,
                                    std::unordered_map<PositionAndVelocity, int>{},
                                    0}
                    );

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Queued test of " << **optimalPositionIt;
#endif
                    return;
                }

                // Get the "path hash", which is the hash of the first position in the explored path
                uint32_t pathHash = 0;
                for (auto positionIt = optimalPositionIt; ; --positionIt)
                {
                    int delta = (int)std::distance(optimalPositionIt, positionIt);
                    if (delta < -EXPLORE_BEFORE) break;

                    pathHash = (*positionIt)->previousPositionsHash;

                    if (positionIt == workerStatus.positionHistory.begin()) break;
                }

                // Create metadata for positions we want to test
                std::shared_ptr<const PositionAndVelocity> nextPosition;
                for (auto positionIt = optimalPositionIt + EXPLORE_AFTER; ; --positionIt)
                {
                    int delta = (int)std::distance(optimalPositionIt, positionIt);
                    if (delta < -EXPLORE_BEFORE) break;

                    auto existingIt = optimalGatherPositions.find(**positionIt);
                    if (existingIt == optimalGatherPositions.end())
                    {
                        std::unordered_map<PositionAndVelocity, int> nextPositions;
                        std::unordered_map<PositionAndVelocity, SecondResendPositionObservationMetadata> secondResendPositions;
                        if (nextPosition)
                        {
                            nextPositions.emplace(*nextPosition, 1);
                            secondResendPositions.emplace(
                                    *nextPosition,
                                    SecondResendPositionObservationMetadata{
                                            *nextPosition,
                                            std::unordered_map<PositionAndVelocity, int>{},
                                            1});
                        }

                        optimalGatherPositions.emplace(
                                **positionIt,
                                PositionObservationMetadata{
                                        pathHash,
                                        **positionIt,
                                        std::move(nextPositions),
                                        delta,
                                        {},
                                        std::move(secondResendPositions)}
                        );

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Queued test of " << **positionIt << " at delta " << delta;
#endif
                    }

                    if (positionIt == workerStatus.positionHistory.begin()) break;
                    nextPosition = *positionIt;
                }

                return;
            }

            // Get the data for the resent position
            auto resentPositionDataIt = optimalGatherPositions.find(*positionsInHistory.resendsBeforeArrival[0]);
            if (resentPositionDataIt == optimalGatherPositions.end())
            {
                // This can legitimately happen when we are doing takeover optimization, but error otherwise
                if (workerStatus.takeoverState == 0)
                {
                    Log::Get() << "ERROR: Resent position metadata not found"
                               << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                }
                return;
            }
            auto &resentPositionData = resentPositionDataIt->second;

            SecondResendPositionObservationMetadata* secondResendData = nullptr;
            if (positionsInHistory.resendsBeforeArrival.size() > 1)
            {
                secondResendData = resentPositionData.secondResendMetadataFor(positionsInHistory.resendsBeforeArrival[1].get());
                if (!secondResendData)
                {
                    // This can legitimately happen when we are doing takeover optimization, but error otherwise
                    if (workerStatus.takeoverState == 0)
                    {
                        Log::Get() << "ERROR: Second resend position metadata not found"
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                    }
                    return;
                }
            }

            auto lastResendPositionIt = positionsInHistory.resendPositionIts[(positionsInHistory.resendsBeforeArrival.size() > 1) ? 1 : 0];

            // Track the observation
            bool exploring = resentPositionData.addObservation(
                    secondResendData,
                    (int)std::distance(lastResendPositionIt, optimalPositionIt));

#if OPTIMALPOSITIONS_DEBUG
            if (secondResendData)
            {
                CherryVis::log(worker->id) << "Added observation of " << resentPositionData
                                           << " : " << secondResendData->pos;
            }
            else
            {
                CherryVis::log(worker->id) << "Added observation of " << resentPositionData;
            }
#endif

            // If this resend was not exploring a new position, nothing more is needed, but we do some debug logging to validate things are
            // working as we expect
            if (!exploring && (!workerStatus.plannedSecondResendPosition || (positionsInHistory.resendsBeforeArrival.size() > 1)))
            {
#if OPTIMALPOSITIONS_DEBUG
                // Measure effectiveness of this gather path

                int arrivalDelay = secondResendData
                                   ? secondResendData->observations.mostCommonArrivalDelay()
                                   : resentPositionData.noSecondResendObservations.mostCommonArrivalDelay();

                auto actualFramesToArrival = std::distance(lastResendPositionIt, positionsInHistory.arrivalPositionIt);
                if (actualFramesToArrival != (BWAPI::Broodwar->getLatencyFrames() + 11 + arrivalDelay))
                {
                    Log::Get() << "ERROR: Position " << resentPositionData << " has unexpected arrival delta"
                               << "; expected=" << (BWAPI::Broodwar->getLatencyFrames() + 10 + arrivalDelay)
                               << "; actual=" << actualFramesToArrival
                               << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                }

                // We can only predict frames to mining when not taking over from another worker
                if (workerStatus.takeoverState != 0) return;

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
#endif
                return;
            }

#if OPTIMALPOSITIONS_DEBUG
            exploredPaths.insert(resentPositionData.pathHash);
            exploredPatches.insert(workerStatus.resource->tile);
#endif

            // Queue up second resend positions to test
            if (positionsInHistory.resendsBeforeArrival.size() == 1 && workerStatus.pathStartsAtDepot())
            {
                int maxIndex = EXPLORE_AFTER - resentPositionData.deltaToNormalPathOptimalPosition + EXPLORE_SECOND_RESEND_POSITIONS;
                for (int i = 1; i <= maxIndex; i++)
                {
                    auto here = positionsInHistory.resendPositionIts[0] + i;
                    auto next = positionsInHistory.resendPositionIts[0] + i + 1;
                    if (here == workerStatus.positionHistory.end()) break; // should never be true

                    auto secondResendMetadataIt = resentPositionData.secondResendMetadata.find(**here);
                    if (secondResendMetadataIt == resentPositionData.secondResendMetadata.end())
                    {
                        std::unordered_map<PositionAndVelocity, int> nextPositions;
                        if (i < maxIndex && next != workerStatus.positionHistory.end())
                        {
                            nextPositions.emplace(**next, 1);
                        }

                        resentPositionData.secondResendMetadata.emplace(
                                **here,
                                SecondResendPositionObservationMetadata{
                                        **here,
                                        std::move(nextPositions),
                                        i});

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Queued test of " << resentPositionData
                                                   << " : " << **here << ", delta " << i;
#endif
                    }
                }
            }
        }

        void updateTakeoverOptimization(WorkerGatherStatus &workerStatus, PositionsInHistory &positionsInHistory, bool switchedPatch)
        {
            // Update 10-distance position
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

            // Don't need to do anything further if we didn't resend a gather command before arrival
            if (positionsInHistory.resendsBeforeArrival.empty()) return;

            // If we switched patches before arrival and before the last resend kicked in, don't measure any observations on it
            if (switchedPatch && positionsInHistory.resendsBeforeArrival.size() < positionsInHistory.resendPositionIts.size())
            {
                return;
            }

            // If there is normal approach optimization data for this resend position, we would have already tracked the observation
            if (positionsInHistory.resendsBeforeArrival.size() == 1)
            {
                auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);
                if (optimalGatherPositions.contains(*positionsInHistory.resendsBeforeArrival[0])) return;
            }
            else if (positionsInHistory.resendsBeforeArrival.size() == 2)
            {
                auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);
                auto optimalGatherPositionIt = optimalGatherPositions.find(*positionsInHistory.resendsBeforeArrival[0]);
                if (optimalGatherPositionIt != optimalGatherPositions.end())
                {
                    if (optimalGatherPositionIt->second.secondResendMetadataFor(positionsInHistory.resendsBeforeArrival[1].get()))
                    {
                        return;
                    }
                }
            }

            // Determine the arrival delay
            int arrivalDelay = 100; // default for when we switched before reaching the patch
            if (positionsInHistory.arrivalPositionIt != workerStatus.positionHistory.end())
            {
                // Get actual frames between last resend and arrival
                auto actualFramesToArrival = std::distance(positionsInHistory.resendPositionIts[positionsInHistory.resendsBeforeArrival.size() - 1],
                                                           positionsInHistory.arrivalPositionIt);

                // Arrival delay is measured from when the command kicks in
                arrivalDelay = (int)actualFramesToArrival - BWAPI::Broodwar->getLatencyFrames() - 11;
            }

            // Get the takeover position record
            auto &takeoverPositions = takeoverPositionsFor(workerStatus.resource);
            auto firstResendPos = positionsInHistory.resendsBeforeArrival[std::max((int)positionsInHistory.resendsBeforeArrival.size() - 2, 0)];
            auto takeoverPositionIt = takeoverPositions.find(*firstResendPos);
            if (takeoverPositionIt == takeoverPositions.end())
            {
                takeoverPositionIt = takeoverPositions.emplace(*firstResendPos, PositionObservationMetadataForTakeoverResends{*firstResendPos}).first;
            }

            // Add the observation
            auto secondResendPos = positionsInHistory.resendsBeforeArrival[positionsInHistory.resendsBeforeArrival.size() - 1];
            if (firstResendPos == secondResendPos) secondResendPos = nullptr;
            takeoverPositionIt->second.addObservation(secondResendPos, arrivalDelay);
        }
    }

    void flushStartOfMiningObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses)
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
                           << "Collision rate: " << (100.0 * collisions)/(double)(collisions + noncollisions)
                           << "% over " << (collisions + noncollisions) << " collections";
            }
        }
#endif

        // Update collision state for workers that are finished mining
        for (auto it = miningWorkers.begin(); it != miningWorkers.end(); )
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

            handlePossiblePatchCollision(*it);

            // Don't need to track this any more
            it = miningWorkers.erase(it);
        }

        // Flush the worker statuses for workers that have started mining
        for (auto it = workerGatherStatuses.begin(); it != workerGatherStatuses.end(); )
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

            PositionsInHistory positionsInHistory;
            if (it->second.switchedPatches || !extractPositionsInHistory(it->second, positionsInHistory)
                    || positionsInHistory.arrivalPositionIt == it->second.positionHistory.end())
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            updateApproachOptimization(it->second, positionsInHistory);

            // Tracking of 10-distance positions and resend positions for takeover
            updateTakeoverOptimization(it->second, positionsInHistory, false);

            // Move required fields into the MiningWorker struct that we use to track patch collisions
            miningWorkers.emplace_back(MiningWorker{
                    std::move(it->second.worker),
                    std::move(it->second.resource),
                    std::move(it->second.positionHistory),
                    std::move(positionsInHistory.resendsBeforeArrival),
                    it->second.takeoverState});

            // We now no longer need to do anything with this worker status
            it = workerGatherStatuses.erase(it);
        }
    }

    void handleStartOfMiningPatchSwitch(WorkerGatherStatus &workerStatus)
    {
        PositionsInHistory positionsInHistory;
        if (!extractPositionsInHistory(workerStatus, positionsInHistory)) return;

        updateTakeoverOptimization(workerStatus, positionsInHistory, true);
    }
}