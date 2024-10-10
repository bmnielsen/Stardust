// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining

#include "WorkerMiningOptimization.h"

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

#if OPTIMALPOSITIONS_DEBUG
        std::set<uint32_t> exploredPaths;
        std::set<BWAPI::TilePosition> exploredPatches;
#endif

        void updateNextPositions(const WorkerGatherStatus &workerStatus, std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &metadata)
        {
            if (!workerStatus.pathStartsAtDepot()) return;

            // Include LF-1 positions after the resend since the positions only change after the command kicks in
            int afterResentPosition = INT_MAX;
            for (auto positionIt = workerStatus.positionHistory.begin(); positionIt != workerStatus.positionHistory.end(); positionIt++)
            {
                afterResentPosition--;
                if (afterResentPosition <= 0) break;
                if (workerStatus.resentPosition == *positionIt) afterResentPosition = BWAPI::Broodwar->getLatencyFrames() - 1;

                auto metadataIt = metadata.find(**positionIt);
                if (metadataIt == metadata.end()) continue;

                auto &positionMetadata = metadataIt->second;

                if ((positionIt + 1) != workerStatus.positionHistory.end() && metadata.find(**(positionIt + 1)) != metadata.end())
                {
                    positionMetadata.next[**(positionIt + 1)]++;
                }

                // Add metadata for second resend positions
                // If this is the resend position, we add up to the second resend metadata limit or LF-1 past the second resend position
                // If not, we add up to LF-1
                int maxIndex = (BWAPI::Broodwar->getLatencyFrames() - 1);
                if (workerStatus.resentPosition == *positionIt)
                {
                    maxIndex = EXPLORE_AFTER - positionMetadata.deltaToNormalPathOptimalPosition + EXPLORE_SECOND_RESEND_POSITIONS;
                }
                int afterSecondResentPosition = INT_MAX;

                for (int secondResendIndex = 1; secondResendIndex <= maxIndex; secondResendIndex++)
                {
                    auto here = positionIt + secondResendIndex;
                    if (here == workerStatus.positionHistory.end()) break;

                    afterSecondResentPosition--;
                    if (afterSecondResentPosition <= 0) break;
                    if (workerStatus.secondResentPosition == *here)
                    {
                        afterSecondResentPosition = BWAPI::Broodwar->getLatencyFrames() - 1;
                    }

                    auto next = (secondResendIndex < maxIndex && afterSecondResentPosition > 1)
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

        void handleMiningWorker(WorkerGatherStatus &workerStatus)
        {
            auto &worker = workerStatus.worker;

            // We don't optimize the case where we have done a scheduled resend
            if (workerStatus.resentOnSchedule()) return;

            // Find the position in the position history where the worker arrived at the patch
            auto arrivalPositionIt = workerStatus.positionHistory.begin();
            for (; arrivalPositionIt != workerStatus.positionHistory.end(); arrivalPositionIt++)
            {
                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    (*arrivalPositionIt)->pos(),
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    workerStatus.resource->center);
                if (dist == 0 && worker->lastPosition == (*arrivalPositionIt)->pos()) break;
            }

            // Ensure we have enough position history to perform the optimization
            if (workerStatus.positionHistory.size() <
                (std::distance(arrivalPositionIt, workerStatus.positionHistory.end()) + BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                return;
            }

            auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);

            updateNextPositions(workerStatus, optimalGatherPositions);

            // Reverse iterator to the apparent optimal position in the position history
            auto optimalPositionIt = workerStatus.positionHistory.rbegin()
                                     + std::distance(arrivalPositionIt, workerStatus.positionHistory.end())
                                     + BWAPI::Broodwar->getLatencyFrames() + 10;

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

            if (workerStatus.plannedResendPosition && !workerStatus.resentPosition)
            {
                Log::Get() << "ERROR: Worker didn't resend at planned position " << *workerStatus.plannedResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
            else if (workerStatus.plannedSecondResendPosition && !workerStatus.secondResentPosition)
            {
                Log::Get() << "ERROR: Worker didn't resend at second planned position " << *workerStatus.plannedSecondResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
#endif

            // If we sent no command, queue a test on the apparent optimal position
            if (!workerStatus.resentPosition)
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
                for (auto positionIt = optimalPositionIt; positionIt != workerStatus.positionHistory.rend(); positionIt++)
                {
                    int delta = (int)std::distance(positionIt, optimalPositionIt);
                    if (delta < -EXPLORE_BEFORE) break;

                    pathHash = (*positionIt)->previousPositionsHash;
                }

                // Create metadata for positions we want to test
                std::shared_ptr<const PositionAndVelocity> nextPosition;
                for (auto positionIt = optimalPositionIt - EXPLORE_AFTER; positionIt != workerStatus.positionHistory.rend(); positionIt++)
                {
                    int delta = (int)std::distance(positionIt, optimalPositionIt);
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

                    nextPosition = *positionIt;
                }

                return;
            }

            // Get the data for the resent position
            auto resentPositionDataIt = optimalGatherPositions.find(*workerStatus.resentPosition);

            // Find the resent position(s) in the position history
            // Second resent position always comes first when reverse iterating
            auto resentPositionIt = workerStatus.positionHistory.rbegin();
            auto secondResentPositionIt = workerStatus.positionHistory.rend();
            for (; resentPositionIt != workerStatus.positionHistory.rend(); resentPositionIt++)
            {
                if (workerStatus.secondResentPosition == *resentPositionIt) secondResentPositionIt = resentPositionIt;
                if (workerStatus.resentPosition == *resentPositionIt) break;
            }

            // If we couldn't locate the resent position, bail out now
            if (resentPositionDataIt == optimalGatherPositions.end() || resentPositionIt == workerStatus.positionHistory.rend() ||
                (workerStatus.secondResentPosition && secondResentPositionIt == workerStatus.positionHistory.rend()))
            {
                Log::Get() << "ERROR: Resent position metadata or history not found"
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                return;
            }

            auto &resentPositionData = resentPositionDataIt->second;

            // Track the observation
            bool exploring = resentPositionData.addObservation(
                    workerStatus.secondResentPosition,
                    (int)std::distance((workerStatus.secondResentPosition ? secondResentPositionIt : resentPositionIt), optimalPositionIt));

#if OPTIMALPOSITIONS_DEBUG
            if (workerStatus.secondResentPosition)
            {
                CherryVis::log(worker->id) << "Added observation of " << resentPositionData
                                           << " : " << *workerStatus.secondResentPosition;
            }
            else
            {
                CherryVis::log(worker->id) << "Added observation of " << resentPositionData;
            }
#endif

            // If this resend was not exploring a new position, nothing more is needed
            if (!exploring && (!workerStatus.plannedSecondResendPosition || workerStatus.secondResentPosition))
            {
#if OPTIMALPOSITIONS_DEBUG
                // Measure effectiveness of this gather path
                if (workerStatus.resentPosition)
                {
                    // Find the last resend position in the position history
                    auto lastResendPositionIt = workerStatus.positionHistory.rbegin();
                    for (; lastResendPositionIt != workerStatus.positionHistory.rend(); lastResendPositionIt++)
                    {
                        if (workerStatus.secondResentPosition == *lastResendPositionIt) break;
                        if (workerStatus.resentPosition == *lastResendPositionIt) break;
                    }

                    auto secondResendData = resentPositionData.secondResendMetadataFor(workerStatus.secondResentPosition.get());
                    int arrivalDelay = secondResendData
                                       ? secondResendData->observations.mostCommonArrivalDelay()
                                       : resentPositionData.noResendObservations.mostCommonArrivalDelay();

                    auto actualFramesToArrival = std::distance(lastResendPositionIt.base(), arrivalPositionIt);
                    if (actualFramesToArrival > (BWAPI::Broodwar->getLatencyFrames() + 10 - arrivalDelay))
                    {
                        Log::Get() << "ERROR: Position " << resentPositionData << " has unexpected arrival delta"
                                   << "; expected=" << (BWAPI::Broodwar->getLatencyFrames() + 10 - arrivalDelay)
                                   << "; actual=" << actualFramesToArrival
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();
                    }

                    auto actualFramesToMining = (int)std::distance(lastResendPositionIt.base(), workerStatus.positionHistory.end());
                    auto framesToReset =
                            OrderProcessTimer::framesToNextReset(currentFrame - actualFramesToMining + BWAPI::Broodwar->getLatencyFrames());
                    int minExpectedFramesToMining, maxExpectedFramesToMining;
                    if (framesToReset > 0 && framesToReset < 12)
                    {
                        minExpectedFramesToMining = (BWAPI::Broodwar->getLatencyFrames() + 10 - arrivalDelay);
                        maxExpectedFramesToMining = minExpectedFramesToMining + 9;
                    }
                    else
                    {
                        minExpectedFramesToMining = maxExpectedFramesToMining = (BWAPI::Broodwar->getLatencyFrames() + 11);
                    }
                    if (actualFramesToMining < minExpectedFramesToMining || actualFramesToMining > maxExpectedFramesToMining)
                    {
                        Log::Get() << "ERROR: Position " << resentPositionData << " has unexpected mining start delta"
                                   << "; expected=" << minExpectedFramesToMining << "-" << maxExpectedFramesToMining
                                   << "; actual=" << actualFramesToMining
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();

                        std::ostringstream dbg;
                        dbg << "Distance mismatch:";
                        bool hasDistanceMismatch = false;
                        for (auto posIt = arrivalPositionIt - 5; posIt != workerStatus.positionHistory.end(); posIt++)
                        {
                            auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                                (*posIt)->pos(),
                                                                BWAPI::UnitTypes::Resource_Mineral_Field,
                                                                workerStatus.resource->center);
                            auto distCurrent = worker->lastPosition.getApproxDistance((*posIt)->pos());

                            dbg << "\n" << **posIt << "; distPatch=" << dist << "; distCurrentPos=" << distCurrent;

                            if (dist == 0 && distCurrent == 1) hasDistanceMismatch = true;
                        }
                        if (hasDistanceMismatch) Log::Get() << dbg.str();
                    }
                }
#endif
                return;
            }

#if OPTIMALPOSITIONS_DEBUG
            exploredPaths.insert(resentPositionData.pathHash);
            exploredPatches.insert(workerStatus.resource->tile);
#endif

            // Queue up second resend positions to test
            if (!workerStatus.secondResentPosition && workerStatus.pathStartsAtDepot())
            {
                int maxIndex = EXPLORE_AFTER - resentPositionData.deltaToNormalPathOptimalPosition + EXPLORE_SECOND_RESEND_POSITIONS;
                for (int i = 1; i <= maxIndex; i++)
                {
                    auto here = resentPositionIt - i;
                    auto next = resentPositionIt - i - 1;
                    if (here == workerStatus.positionHistory.rbegin()) break; // should never be true

                    auto secondResendMetadataIt = resentPositionData.secondResendMetadata.find(**here);
                    if (secondResendMetadataIt == resentPositionData.secondResendMetadata.end())
                    {
                        std::unordered_map<PositionAndVelocity, int> nextPositions;
                        if (i < maxIndex && next != workerStatus.positionHistory.rbegin())
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
    }

    void flushStartOfMiningObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses)
    {
#if OPTIMALPOSITIONS_DEBUG
        if (currentFrame == 0)
        {
            exploredPaths.clear();
            exploredPatches.clear();
        }
        else if (currentFrame % 1000 == 0)
        {
            Log::Get() << "Explored " << exploredPaths.size() << " path(s) over " << exploredPatches.size() << " patch(es)";
        }
#endif

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

            handleMiningWorker(it->second);

/*
            // Tracking of 10-distance positions and resend positions for takeover
            updateTakeoverMetadata(workerStatus,
                                   workerStatus.resource,
                                   tenDistancePositionsFor(workerStatus.resource),
                                   takeoverPositionsFor(workerStatus.resource));

                                   */

            // We now no longer need to do anything with this worker status
            it = workerGatherStatuses.erase(it);
        }
    }

    void handleStartOfMiningPatchSwitch(WorkerGatherStatus &workerStatus,
                                        const Resource &resource,
                                        std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                        std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions)
    {
        /*
        updateTakeoverMetadata(workerStatus, resource, tenDistancePositions, takeoverResendPositions, true);
         */
    }
}