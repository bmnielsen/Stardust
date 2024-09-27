// Worker mining optimization is split into multiple files
// This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining

#include "WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "PositionObservationMetadata.h"
#include "WorkerGatherStatus.h"

#include "Geo.h"

namespace WorkerMiningOptimization
{
    namespace
    {
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

            // Ensure we have enough history
            if (workerStatus.positionHistory.size() >= (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
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
            }

            // We now no longer need to do anything with this worker status
            it = workerGatherStatuses.erase(it);
        }
    }

    void handleStartOfMiningPatchSwitch(WorkerGatherStatus &workerStatus,
                                        const Resource &resource,
                                        std::map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                        std::map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions)
    {
        updateTakeoverMetadata(workerStatus, resource, tenDistancePositions, takeoverResendPositions, true);
    }
}