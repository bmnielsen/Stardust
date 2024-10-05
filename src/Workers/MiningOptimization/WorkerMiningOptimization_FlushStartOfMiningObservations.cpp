// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining

#include "WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "PositionObservationMetadata.h"
#include "WorkerGatherStatus.h"

#include "Geo.h"
#include "OrderProcessTimer.h"

#define EXPLORE_BEFORE 8
#define EXPLORE_AFTER 5
#define EXPLORE_SECOND_RESEND_POSITIONS 2 // This is in addition to EXPLORE_AFTER

namespace WorkerMiningOptimization
{
    namespace
    {
        /*
        // Tracks a nonoptimal resend for the normal approach timing optimization (where we expect to reach the patch 11+LF frames after resend)
        void trackNonoptimalResend(PositionObservationMetadata &metadata,
                                   const PositionAndVelocity &optimalPosition,
                                   const std::shared_ptr<PositionAndVelocity> &secondResendPosition,
                                   int resentDelta,
                                   int secondResentDelta)
        {
            if (metadata.atObservationCap()) return;

            // A special case is if the worker actually arrived at the patch faster because of the resend
            // This doesn't help us, since the worker has to wait for the order process timer to reach 0, but
            // we get the same performance as we expected (as if the path hadn't changed), so we can treat it as
            // a success
            if (resentDelta >= 0)
            {
                metadata.successes++;
                return;
            }

            metadata.failures++;

            // Track an observation on the optimal second resend position if we didn't resend anything
            if (!secondResendPosition)
            {
                auto &positions = metadata.failurePositionMetadata[optimalPosition];

                auto it = positions.find(-resentDelta);
                if (it == positions.end())
                {
                    it = positions.emplace(-resentDelta, PositionObservationMetadata{optimalPosition}).first;
                }

                it->second.trackObservation();
            }
            else
            {
                // Track success or failure on all of the matching positions
                auto positionsIt = metadata.failurePositionMetadata.find(*secondResendPosition);
                if (positionsIt != metadata.failurePositionMetadata.end())
                {
                    for (auto &[_, secondMetadata] : positionsIt->second)
                    {
                        (secondResentDelta >= 0 ? secondMetadata.successes : secondMetadata.failures)++;
                    }
                }
            }
        }

        void handleObservation(const WorkerGatherStatus &workerStatus,
                               const std::shared_ptr<PositionAndVelocity> &observedPosition,
                               const std::shared_ptr<PositionAndVelocity> &resentPosition,
                               std::map<PositionAndVelocity, PositionObservationMetadata> &observations,
                               bool tenDistance = false)
        {
            if (workerStatus.resentOnSchedule()) return;

            // If no command was resent, we can track an observation
            // We don't track observations otherwise since resending the command can change the path
            if (!resentPosition)
            {
                auto metadata = observations.find(*observedPosition);
                if (metadata == observations.end())
                {
                    metadata = observations.emplace(*observedPosition, PositionObservationMetadata{*observedPosition}).first;
                }
                metadata->second.trackObservation();

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(workerStatus.worker->id) << "Tracking observation on " << (tenDistance ? "10-distance " : "") << metadata->second;
                CherryVis::log(workerStatus.resource->id) << "Tracking observation on " << (tenDistance ? "10-distance " : "") << metadata->second;
#endif

                return;
            }

            // If a command was resent, try to find its metadata
            auto resendPositionData = observations.find(*resentPosition);
            if (resendPositionData == observations.end()) return;

            // Find the delta between the observed and resent positions in the positions history, which we use to measure
            // the risk/reward of using a position
            auto getDelta = [&](const std::shared_ptr<PositionAndVelocity> &position)
            {
                if (!position) return LONG_MAX;

                auto observedIt = workerStatus.positionHistory.rend();
                auto posIt = workerStatus.positionHistory.rend();
                for (auto it = workerStatus.positionHistory.rbegin(); it != workerStatus.positionHistory.rend(); it++)
                {
                    if ((*it) == observedPosition) observedIt = it;
                    if ((*it) == position) posIt = it;
                    if (observedIt != workerStatus.positionHistory.rend() && posIt != workerStatus.positionHistory.rend()) break;
                }
                if (observedIt != workerStatus.positionHistory.rend() && posIt != workerStatus.positionHistory.rend())
                {
                    return std::distance(posIt, observedIt);
                }

                return LONG_MAX;
            };

            auto resentDelta = getDelta(workerStatus.resentPosition);
            if (resentDelta == LONG_MAX) return;

            if (tenDistance)
            {
                // It is OK if we sent the order too early, as this does not introduce a risk of losing mineral locking
                // Otherwise we just mark it as a failure without including any additional metadata right now
                if (resentDelta <= 1)
                {
                    resendPositionData->second.successes++;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(workerStatus.worker->id) << "Tracking success on 10-distance " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking success on 10-distance " << resendPositionData->second;
#endif
                }
                else
                {
                    resendPositionData->second.failures++;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(workerStatus.worker->id) << "Tracking failure on 10-distance " << resendPositionData->second;
                    CherryVis::log(workerStatus.resource->id) << "Tracking failure on 10-distance " << resendPositionData->second;
#endif
                }
                return;
            }

            // Track the nonoptimal resend for arrival optimization
            auto secondResentDelta = getDelta(workerStatus.secondResentPosition);
            trackNonoptimalResend(resendPositionData->second,
                                  *observedPosition,
                                  workerStatus.secondResentPosition,
                                  (int)resentDelta,
                                  (int)secondResentDelta);

#if OPTIMALPOSITIONS_DEBUG
            if (resentDelta >= 0)
            {
                CherryVis::log(workerStatus.worker->id) << "Tracking success on " << resendPositionData->second;
                CherryVis::log(workerStatus.resource->id) << "Tracking success on " << resendPositionData->second;
            }
            else
            {
                CherryVis::log(workerStatus.worker->id) << "Tracking failure on " << resendPositionData->second;
                CherryVis::log(workerStatus.resource->id) << "Tracking failure on " << resendPositionData->second;
            }
#endif
        }

        void updateTakeoverMetadata(WorkerGatherStatus &workerStatus,
                                    const Resource &resource,
                                    std::map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                    std::map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions,
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

            auto &workerStatus = it->second;

            // We don't optimize the case where we have done a scheduled resend
            if (workerStatus.resentOnSchedule())
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

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
                it = workerGatherStatuses.erase(it);
                continue;
            }

            auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);

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
            else
            {
                // Measure effectiveness of this gather path
                if (workerStatus.resentPosition)
                {
                    auto resentPositionDataIt = optimalGatherPositions.find(*workerStatus.resentPosition);
                    if (resentPositionDataIt != optimalGatherPositions.end() && !resentPositionDataIt->second.hasPositionToTry)
                    {
                        auto &resentPositionData = resentPositionDataIt->second;

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

                        auto actualFramesToMining = std::distance(lastResendPositionIt.base(), workerStatus.positionHistory.end());
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
                }
            }
#endif

            auto applyOnPositionsBefore = [&](
                    std::vector<std::shared_ptr<const PositionAndVelocity>>::reverse_iterator it,
                    const std::function<bool(PositionObservationMetadata&)> &func)
            {
                for (it++; it != workerStatus.positionHistory.rend(); it++)
                {
                    auto previousPositionDataIt = optimalGatherPositions.find(**it);
                    if (previousPositionDataIt == optimalGatherPositions.end()) return;
                    if (func(previousPositionDataIt->second)) return;
                }
            };

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

                    it = workerGatherStatuses.erase(it);
                    continue;
                }

                // If the path did not start at the depot, we don't try to explore surrounding positions
                // The rationale for this is that paths not starting at the depot are not likely to come up often,
                // so we can't explore them efficiently
                if (!workerStatus.pathStartsAtDepot())
                {
                    auto emplacedIt = optimalGatherPositions.emplace(
                            **optimalPositionIt,
                            PositionObservationMetadata{(*optimalPositionIt)->previousPositionsHash, **optimalPositionIt, nullptr, 0}
                    ).first;

                    emplacedIt->second.hasPositionToTry = true;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Queued test of " << **optimalPositionIt;
#endif

                    it = workerGatherStatuses.erase(it);
                    continue;
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
                        existingIt = optimalGatherPositions.emplace(
                                **positionIt,
                                PositionObservationMetadata{pathHash, **positionIt, nextPosition, delta}
                        ).first;

                        if (delta >= 0) existingIt->second.hasPositionToTry = true;
                        if (delta < EXPLORE_AFTER) existingIt->second.followingHasPositionToTry = true;

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Queued test of " << **positionIt << " at delta " << delta;
                    }
                    else
                    {
                        Log::Get() << "ERROR: Position " << existingIt->second
                                   << " came up again during processing of a new position at delta " << delta
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                    }

                    nextPosition = *positionIt;
                }

                applyOnPositionsBefore(optimalPositionIt, [](PositionObservationMetadata &metadata)
                {
                    metadata.followingHasPositionToTry = true;
                    return false;
                });

                it = workerGatherStatuses.erase(it);
                continue;
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
                it = workerGatherStatuses.erase(it);
                continue;
            }

            auto &resentPositionData = resentPositionDataIt->second;
            bool exploring = resentPositionData.hasPositionToTry;
            int previousBestDelta = resentPositionData.bestDelta;

            // Track the observation
            resentPositionData.addObservation(
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

            // Clears the second resend positions data from the current position
            // We do this if we observe instability in the positions reached after the resend
            auto clearSecondResendPositionsData = [&]()
            {
                resentPositionData.secondResendMetadata.clear();

                // If the best delta changes as a result of this, cascade that backwards
                auto bestDelta = 100;
                if (!resentPositionData.noResendObservations.empty())
                {
                    int arrivalDelay = resentPositionData.noResendObservations.mostCommonArrivalDelay();
                    if (arrivalDelay >= 0)
                    {
                        bestDelta = resentPositionData.deltaToNormalPathOptimalPosition + arrivalDelay;
                    }
                }
                if (bestDelta != resentPositionData.bestDelta)
                {
                    resentPositionData.bestDelta = bestDelta;
                    int followingBest = std::min(resentPositionData.bestDelta, resentPositionData.bestFollowingPositionDelta);
                    applyOnPositionsBefore(resentPositionIt, [&](PositionObservationMetadata &metadata)
                    {
                        if (metadata.bestFollowingPositionDelta != followingBest)
                        {
                            metadata.bestFollowingPositionDelta = followingBest;
                            followingBest = std::min(metadata.bestDelta, followingBest);
                            return false;
                        }
                        return true;
                    });
                }

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Cleared second resend metadata of " << resentPositionData;
                Log::Get() << "ERROR: Cleared unstable second resend positions for " << resentPositionData
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
            };

            // If we had already explored all possible options before this resend, nothing more is needed
            if (!exploring)
            {
                if (workerStatus.plannedSecondResendPosition && !workerStatus.secondResentPosition)
                {
                    clearSecondResendPositionsData();
                }

                it = workerGatherStatuses.erase(it);
                continue;
            }

#if OPTIMALPOSITIONS_DEBUG
            exploredPaths.insert(resentPositionData.pathHash);
            exploredPatches.insert(workerStatus.resource->tile);
#endif

            // Cascade a new best delta to previous positions on this path
            if (resentPositionData.bestDelta < previousBestDelta)
            {
                applyOnPositionsBefore(resentPositionIt, [&resentPositionData](PositionObservationMetadata &metadata)
                {
                    if (metadata.bestFollowingPositionDelta > resentPositionData.bestDelta)
                    {
                        metadata.bestFollowingPositionDelta = resentPositionData.bestDelta;
                    }
                    return false;
                });
            }

            // Queue up second resend positions to test
            if (!workerStatus.secondResentPosition &&
                (resentPositionData.secondResendMetadata.empty() || WorkerMiningOptimization::isExploring()))
            {
                // If we have already queued second resend positions, this is an indication that the path is unstable:
                // we saw some second resend positions the first time that we have not seen now
                // When this happens, we just distrust all of the second resent positions
                if (!resentPositionData.secondResendMetadata.empty())
                {
                    clearSecondResendPositionsData();
                }
                else if (workerStatus.pathStartsAtDepot())
                {
                    for (int i = 1; i <= (EXPLORE_AFTER - resentPositionData.deltaToNormalPathOptimalPosition + EXPLORE_SECOND_RESEND_POSITIONS); i++)
                    {
                        auto here = resentPositionIt - i;
                        if (here == workerStatus.positionHistory.rbegin()) break; // should never be true

                        // We can't issue gather commands LF apart, second will fail with Unit_Busy
                        if (i == BWAPI::Broodwar->getLatencyFrames()) continue;

                        resentPositionData.secondResendMetadata.emplace_back(SecondResendPositionObservationMetadata{**here, i});

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Queued test of " << resentPositionData
                                                   << " : " << **here << ", delta " << i;
#endif
                    }

                    resentPositionData.hasPositionToTry = true;
                }
            }

            // If there are still things to explore in this position, continue now
            if (resentPositionData.hasPositionToTry)
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            // If we are finished exploring forwards, explore backwards
            if (!resentPositionData.followingHasPositionToTry && resentPositionData.deltaToNormalPathOptimalPosition >= 0)
            {
                applyOnPositionsBefore(resentPositionIt, [](PositionObservationMetadata &metadata)
                {
                    if (metadata.deltaToNormalPathOptimalPosition < 0) metadata.hasPositionToTry = true;
                    metadata.followingHasPositionToTry = (metadata.deltaToNormalPathOptimalPosition < -1);
                    return false;
                });
            }

            // If we no longer have following positions to try, cascade this backwards
            if (!resentPositionData.followingHasPositionToTry)
            {
                applyOnPositionsBefore(resentPositionIt, [](PositionObservationMetadata &metadata)
                {
                    metadata.followingHasPositionToTry = false;
                    return metadata.hasPositionToTry;
                });
            }

            // Potentially queue up other positions to test if we are finished testing this position
            // TODO
//            if (!resentPositionData.hasPositionToTry)
//            {
//                if (resentPositionData.deltaToNormalPathOptimalPosition >= 0 && resentPositionData.deltaToNormalPathOptimalPosition <= 2)
//                {
//
//                }
//            }

            /*

            // If we have just finished exploring the original optimal position candidate, unlock the other candidates
            if (resentPositionData.deltaToNormalPathOptimalPosition == 0 && !resentPositionData.hasPositionToTry)
            {
                resentPositionData.followingHasPositionToTry = true;

            }
            else if (!resentPositionData.hasPositionToTry)
            {
                auto unsetFollowingHasUntriedPosition = [](PositionObservationMetadata &metadata)
                {
                    metadata.followingHasPositionToTry = false;
                    return metadata.hasPositionToTry;
                };
                applyOnPositionsBefore(resentPositionIt, unsetFollowingHasUntriedPosition);
            }
*/

/*

                    // If this resent position hasn't found an optimal delta yet, queue some second resends to try
                    bool queuedSecondResend = false;
                    if (resentPositionData.bestDelta > 0 && !workerStatus.secondResentPosition)
                    {
                        for (auto secondResendPositionIt = positionIt - 1;
                             secondResendPositionIt != workerStatus.positionHistory.rbegin();
                             secondResendPositionIt--)
                        {
                            // We queue at most 5 but skip the position at LF since that is not usable due to Unit_Busy
                            if (secondResendPositionIt == (positionIt - 6)) break;
                            if (std::distance(secondResendPositionIt, positionIt) == BWAPI::Broodwar->getLatencyFrames()) continue;

                            auto secondResendPositionDataIt = resentPositionData.resendPositionToData.find(**secondResendPositionIt);
                            if (secondResendPositionDataIt == resentPositionData.resendPositionToData.end())
                            {
                                resentPositionData.resendPositionToData.emplace(**secondResendPositionIt, std::map<int, int>{});
                                if (!queuedSecondResend)
                                {
                                    applyOnPositionsBefore(positionIt, setFollowingHasPositionToTry);
                                    queuedSecondResend = true;
                                }

#if OPTIMALPOSITIONS_DEBUG
                                CherryVis::log(worker->id) << "Queued test of " << resentPositionData
                                                           << " : " << **secondResendPositionIt << ", delta "
                                                           << std::distance(secondResendPositionIt, positionIt);
#endif
                            }
                        }
                    }

                    // If we haven't queued a new experiment, and this position no longer has any untried positions, update previous positions
                    if (!queuedSecondResend && !resentPositionData.hasUntriedPosition())
                    {
                        auto unsetFollowingHasUntriedPosition = [](PositionObservationMetadata &metadata)
                        {
                            metadata.followingHasPositionToTry = false;
                            return metadata.hasUntriedPosition();
                        };
                        applyOnPositionsBefore(positionIt, unsetFollowingHasUntriedPosition);
                    }

                    // Update all of the earlier positions' best following delta
                    applyOnPositionsBefore(positionIt, [&resentPositionData](PositionObservationMetadata &metadata)
                    {
                        if (metadata.bestFollowingPositionDelta > resentPositionData.bestDelta)
                        {
                            metadata.bestFollowingPositionDelta = resentPositionData.bestDelta;
                        }
                        return false;
                    });

#if OPTIMALPOSITIONS_DEBUG
                    if (workerStatus.secondResentPosition)
                    {
                        CherryVis::log(worker->id) << "Added observation of " << resentPositionDataIt->second
                                                   << " : " << *workerStatus.secondResentPosition;
                    }
                    else
                    {
                        CherryVis::log(worker->id) << "Added observation of " << resentPositionDataIt->second;
                    }
#endif
                }
            }
*/
            /*
            // Handle observations for the optimizing arrival at the patch
            auto optimalPositionIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
            handleObservation(workerStatus,
                              *optimalPositionIt,
                              workerStatus.resentPosition,
                              optimalGatherPositionsFor(workerStatus.resource));

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
                                        std::map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                        std::map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions)
    {
        /*
        updateTakeoverMetadata(workerStatus, resource, tenDistancePositions, takeoverResendPositions, true);
         */
    }
}