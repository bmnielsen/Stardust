// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining

#include "WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "PositionObservationMetadata.h"
#include "WorkerGatherStatus.h"

#include "Geo.h"

#define EXPLORE_BEFORE 5
#define EXPLORE_AFTER 3
#define MAX_SECOND_RESEND_POSITIONS 5

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
    }

    void flushStartOfMiningObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses)
    {
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

            // Remove positions before the worker reached the patch
            {
                auto positionIt = workerStatus.positionHistory.begin();
                for (; positionIt != workerStatus.positionHistory.end(); positionIt++)
                {
                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        (*positionIt)->pos(),
                                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                                        workerStatus.resource->center);
                    if (dist == 0 && worker->lastPosition.getApproxDistance((*positionIt)->pos()) < 2) break;
                }
                workerStatus.positionHistory.erase(positionIt, workerStatus.positionHistory.end());
            }

            // Ensure we have enough history and we haven't done a scheduled resend
            if (workerStatus.resentOnSchedule() || workerStatus.positionHistory.size() < (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            auto &optimalGatherPositions = optimalGatherPositionsFor(workerStatus.resource);

            // Iterator to the optimal position in the position history
            // This might not be achievable, but is what we use to measure against
            auto optimalPositionIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;

#if OPTIMALPOSITIONS_DEBUG
            std::ostringstream dbg;
            dbg << "Position history:";
            for (auto positionIt = optimalPositionIt + 5; positionIt >= optimalPositionIt - 5; positionIt--)
            {
                dbg << "\n" << **positionIt;
            }
            CherryVis::log(worker->id) << dbg.str();

            if (workerStatus.plannedResendPosition && !workerStatus.resentPosition)
            {
                Log::Get() << "ERROR: Worker didn't resend at planned position " << *workerStatus.plannedResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
            }
            if (workerStatus.plannedSecondResendPosition && !workerStatus.secondResentPosition)
            {
                Log::Get() << "ERROR: Worker didn't resend at second planned position " << *workerStatus.plannedSecondResendPosition
                           << "; worker id " << worker->id << " @ " << worker->getTilePosition();
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

            // If we had already explored all possible options before this resend, nothing more is needed
            if (!exploring)
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

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
            if (resentPositionData.bestDelta > resentPositionData.deltaToNormalPathOptimalPosition && !workerStatus.secondResentPosition)
            {
                // If we have already queued second resend positions, this is an indication that the path is unstable:
                // we saw some second resend positions the first time that we have not seen now
                // When this happens, we just distrust all of the second resent positions
                if (!resentPositionData.secondResendMetadata.empty())
                {
                    resentPositionData.secondResendMetadata.clear();
#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Cleared second resend metadata of " << resentPositionData;
                    Log::Get() << "ERROR: Second resend positions unstable for " << resentPositionData
                               << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                }
                else
                {
                    auto targetDelta = std::min(resentPositionData.bestDelta, resentPositionData.bestFollowingPositionDelta)
                                       - resentPositionData.deltaToNormalPathOptimalPosition - 1;
                    for (int i = 1; i <= std::min(targetDelta, MAX_SECOND_RESEND_POSITIONS); i++)
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
                }
            }

            resentPositionData.updateState();

            // If there are still things to explore in this position, continue now
            if (resentPositionData.hasPositionToTry)
            {
                it = workerGatherStatuses.erase(it);
                continue;
            }

            // Unmark following positions to try that can not be better than the current position
            if (resentPositionData.bestDelta < EXPLORE_AFTER)
            {
                // Unset future try positions where relevant and collect them in a vector
                std::vector<PositionObservationMetadata*> futurePositions;
                {
                    auto current = &resentPositionData;
                    while (current->next)
                    {
                        auto nextPositionDataIt = optimalGatherPositions.find(*current->next);
                        if (nextPositionDataIt != optimalGatherPositions.end())
                        {
                            current = &nextPositionDataIt->second;
                            futurePositions.push_back(current);

                            if (resentPositionData.bestDelta < current->deltaToNormalPathOptimalPosition)
                            {
                                current->hasPositionToTry = false;
#if OPTIMALPOSITIONS_DEBUG
                                CherryVis::log(worker->id) << "Unqueued as it can no longer be better: " << nextPositionDataIt->second;
#endif
                            }
                        }
                    }
                }

                // Loop backwards to update whether the following has a position to try
                bool followingHasPositionToTry = false;
                for (auto futurePositionIt = futurePositions.rbegin(); futurePositionIt != futurePositions.rend(); futurePositionIt++)
                {
                    (*futurePositionIt)->followingHasPositionToTry = followingHasPositionToTry;
                    followingHasPositionToTry = followingHasPositionToTry || (*futurePositionIt)->hasPositionToTry;
                }
                resentPositionData.followingHasPositionToTry = followingHasPositionToTry;
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