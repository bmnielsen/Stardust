// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the start of mining

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"
#include "Workers.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
        double expectedPatchCollisionDelay(int observedCollisions, int observedNonCollisions)
        {
            int total = observedCollisions + observedNonCollisions;
            if (total == 0) return 0.0;

            // If we are exploring and don't have enough data yet, allow it no matter what
            if (WorkerMiningOptimization::isExploring() && total < 5) return 0.0;

            // A collision adds 14 frames of delay
            return 14.0 * (double)observedCollisions / (double)total;
        }

        struct PositionEvaluation
        {
            double expectedDelta = 100.0;
            std::deque<PositionAndVelocity> expectedPath;
            std::shared_ptr<PositionAndVelocity> resendPosition;
            bool positionToTryOnExpectedPath = false;
            int positionToTryDelta = 0;
        };

        double computeExpectedDelta(int commandFrame,
                                    const PositionObservationMetadata &positionMetadata,
                                    int deltaToFirstResend,
                                    const ResendPositionObservations &observations)
        {
            double expectedMiningDelay = observations.expectedMiningDelay(false, commandFrame);
            auto collisionDelay = expectedPatchCollisionDelay(observations.collisions, observations.nonCollisions);
            return positionMetadata.deltaToNormalPathOptimalPosition + deltaToFirstResend + expectedMiningDelay + collisionDelay;
        }

        PositionEvaluation evaluateSecondResendPositions(int normalPathCommandFrame,
                                                         const PositionObservationMetadata &positionMetadata,
                                                         const PositionAndVelocity &here,
                                                         int deltaToFirstResend,
                                                         const ResendPositionObservations &observations,
                                                         const std::unordered_map<PositionAndVelocity, int> &nextPositions)
        {
            // Start by getting the data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            {
                double deltaAccumulator = 0.0;
                int occurrenceCount = 0;
                int bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : nextPositions)
                {
                    auto nextPositionDataIt = positionMetadata.secondResendMetadata.find(nextPosition);
                    if (nextPositionDataIt == positionMetadata.secondResendMetadata.end())
                    {
#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "ERROR: No second resend metadata found for next position " << nextPosition;
#if OPTIMALPOSITIONS_DEBUG_VERBOSE
                        std::ostringstream dbg;
                        dbg << "Second resend positions:";
                        for (const auto &pos : positionMetadata.expectedPathAfterResend())
                        {
                            dbg << "\n" << pos->pos;
                            if (pos->next.empty())
                            {
                                dbg << " (no next)";
                            }
                            else
                            {
                                dbg << " : " << pos->next.begin()->first;
                            }
                        }
                        Log::Get() << dbg.str();
#endif
#endif
                        continue;
                    }

                    auto nextPositionEvaluation = evaluateSecondResendPositions(normalPathCommandFrame,
                                                                                positionMetadata,
                                                                                nextPosition,
                                                                                nextPositionDataIt->second.deltaToFirstResend,
                                                                                nextPositionDataIt->second.observations,
                                                                                nextPositionDataIt->second.next);
                    deltaAccumulator += nextPositionEvaluation.expectedDelta * occurrences;
                    occurrenceCount += occurrences;
                    if (occurrences > bestOccurrences)
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelta = (deltaAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), here);

            // We can't send another command at LF after previous command
            if (deltaToFirstResend == BWAPI::Broodwar->getLatencyFrames()) return nextPositionsEvaluation;

            // We can't send a command LF+1 frames before an order process timer reset
            int commandFrame = normalPathCommandFrame + positionMetadata.deltaToNormalPathOptimalPosition + deltaToFirstResend;
            if (OrderProcessTimer::framesToNextReset(commandFrame) == (BWAPI::Broodwar->getLatencyFrames() + 1)) return nextPositionsEvaluation;

            // If we want to try this position and it is better than the current best, return this
            if (observations.empty() &&
                (WorkerMiningOptimization::isExploring() || (positionMetadata.deltaToNormalPathOptimalPosition == 0 && deltaToFirstResend == 0)))
            {
                int positionToTryDelta = std::abs(positionMetadata.deltaToNormalPathOptimalPosition + deltaToFirstResend);
                if (!nextPositionsEvaluation.positionToTryOnExpectedPath || positionToTryDelta < nextPositionsEvaluation.positionToTryDelta)
                {
                    return {100, {here}, std::make_shared<PositionAndVelocity>(positionMetadata.pos), true, positionToTryDelta};
                }
            }

            // If the next positions' expected path has a position to try, return it
            if (nextPositionsEvaluation.positionToTryOnExpectedPath) return nextPositionsEvaluation;

            // Compute the expected delta for this position
            double expectedDelta = computeExpectedDelta(commandFrame, positionMetadata, deltaToFirstResend, observations);
            if (expectedDelta < (nextPositionsEvaluation.expectedDelta - EPSILON))
            {
                return {expectedDelta, {here}, std::make_shared<PositionAndVelocity>(positionMetadata.pos), false, 0};
            }

            return nextPositionsEvaluation;
        }

        PositionEvaluation evaluatePosition(int normalPathCommandFrame,
                                            const std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &allPositionData,
                                            const PositionObservationMetadata &positionMetadata)
        {
            // Start by getting the data for all of the next positions
            PositionEvaluation nextPositionsEvaluation;
            {
                double deltaAccumulator = 0.0;
                int occurrenceCount = 0;
                int bestOccurrences = 0;
                for (const auto &[nextPosition, occurrences] : positionMetadata.next)
                {
                    auto nextPositionDataIt = allPositionData.find(nextPosition);
                    if (nextPositionDataIt == allPositionData.end())
                    {
#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "ERROR: No metadata found for next position " << nextPosition;
#endif
                        continue;
                    }

                    auto nextPositionEvaluation = evaluatePosition(normalPathCommandFrame, allPositionData, nextPositionDataIt->second);
                    deltaAccumulator += nextPositionEvaluation.expectedDelta * occurrences;
                    occurrenceCount += occurrences;
                    if (occurrences > bestOccurrences)
                    {
                        bestOccurrences = occurrences;
                        nextPositionsEvaluation = std::move(nextPositionEvaluation);
                    }
                }
                if (occurrenceCount > 0) nextPositionsEvaluation.expectedDelta = (deltaAccumulator / (double)occurrenceCount);
            }
            nextPositionsEvaluation.expectedPath.insert(nextPositionsEvaluation.expectedPath.begin(), positionMetadata.pos);

            // Now evaluate this position using the second resend metadata
            auto evaluationHere = evaluateSecondResendPositions(normalPathCommandFrame,
                                                                positionMetadata,
                                                                positionMetadata.pos,
                                                                0,
                                                                positionMetadata.noSecondResendObservations,
                                                                positionMetadata.next);

            // If one of the branches wants to explore, return it
            if (evaluationHere.positionToTryOnExpectedPath &&
                (!nextPositionsEvaluation.positionToTryOnExpectedPath
                 || evaluationHere.positionToTryDelta < nextPositionsEvaluation.positionToTryDelta))
            {
                return evaluationHere;
            }
            else if (nextPositionsEvaluation.positionToTryOnExpectedPath)
            {
                return nextPositionsEvaluation;
            }

            // Return the best branch
            if (evaluationHere.expectedDelta < (nextPositionsEvaluation.expectedDelta - 0.0001))
            {
                return evaluationHere;
            }

            return nextPositionsEvaluation;
        }

        void validatePlannedPath(WorkerGatherStatus &workerStatus,
                                 BWAPI::Unit resourceBwapiUnit,
                                 const std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &optimalPositions,
                                 const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (!workerStatus.resendsPlanned) return; // haven't planned yet
            if (workerStatus.expectedPath.empty()) return; // have no further resends planned
            if (workerStatus.expectedPath.front() == *currentPosition) return; // path matches expectations

            // We have reached an unexpected position

            // If we haven't passed the first resend position yet, then just clear the planned data so we can replan
            if (!workerStatus.resentPosition)
            {
#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(workerStatus.worker->id) << "Worker did not follow expected path; expected " << workerStatus.expectedPath.front()
                                                        << "; actual " << *currentPosition
                                                        << "; falling through to replan resend position";
#endif

                workerStatus.resendsPlanned = false;
                workerStatus.expectedPath.clear();
                workerStatus.plannedResendPosition = nullptr;
                workerStatus.plannedSecondResendPosition = nullptr;
                return;
            }

            // We have sent the first resend, but hit a different path before reaching the second resend position
            auto resentPositionDataIt = optimalPositions.find(*workerStatus.resentPosition);
            if (resentPositionDataIt == optimalPositions.end())
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "ERROR: Didn't find resend position metadata: " << *workerStatus.resentPosition;
#endif
                return;
            }

            auto &resentPositionData = resentPositionDataIt->second;

            // Check if we have observed this path
            auto secondGatherPositionIt = resentPositionData.secondResendMetadata.find(*currentPosition);
            if (secondGatherPositionIt == resentPositionData.secondResendMetadata.end())
            {
                // We haven't observed this path, so let's just schedule a resend at the same delta and hope the result will be the same
                secondGatherPositionIt = resentPositionData.secondResendMetadata.find(*workerStatus.plannedSecondResendPosition);
                if (secondGatherPositionIt == resentPositionData.secondResendMetadata.end())
                {
#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "ERROR: Didn't find resend position in positions history: " << *workerStatus.resentPosition;
#endif
                    return;
                }

                // Get the delta from the first resend position to here
                auto positionIt = workerStatus.positionHistory.rbegin();
                for (; positionIt != workerStatus.positionHistory.rend(); positionIt++)
                {
                    if ((**positionIt) == (*workerStatus.resentPosition)) break;
                }
                if (positionIt == workerStatus.positionHistory.rend())
                {
#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "ERROR: Didn't find resend position in positions history: " << *workerStatus.resentPosition;
#endif
                    return;
                }

                int resendIn = secondGatherPositionIt->second.deltaToFirstResend
                               - (int)std::distance(workerStatus.positionHistory.rbegin(), positionIt);
                if (resendIn == 0)
                {
                    workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                }
                else
                {
                    workerStatus.resendCommandOnFrame = currentFrame + resendIn;
                }
                return;
            }

            // We have observed this path, so we can replan given that we already performed one resend

            // Evaluate second resends
            int normalPathCommandFrame = BWAPI::Broodwar->getFrameCount() - resentPositionData.deltaToNormalPathOptimalPosition;
            auto evaluation = evaluateSecondResendPositions(normalPathCommandFrame,
                                                            resentPositionData,
                                                            *currentPosition,
                                                            secondGatherPositionIt->second.deltaToFirstResend,
                                                            secondGatherPositionIt->second.observations,
                                                            secondGatherPositionIt->second.next);

            // Evaluate no resend
            int commandFrame = normalPathCommandFrame + resentPositionData.deltaToNormalPathOptimalPosition;
            double expectedDelta = computeExpectedDelta(commandFrame,
                                                        resentPositionData,
                                                        0,
                                                        resentPositionData.noSecondResendObservations);

            // Pick the best strategy - either resend at a different position or clear
            if (evaluation.positionToTryOnExpectedPath ||
                (evaluation.expectedDelta < 10 && evaluation.expectedDelta < (expectedDelta + EPSILON)))
            {
                workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
                workerStatus.expectedPath = std::move(evaluation.expectedPath);
            }
            else
            {
                workerStatus.plannedSecondResendPosition = nullptr;
                workerStatus.expectedPath.clear();
            }
        }

        bool handleTakeover(WorkerGatherStatus &workerStatus,
                            const std::shared_ptr<PositionAndVelocity> &currentPosition,
                            BWAPI::Unit resourceBwapiUnit)
        {
            auto &resource = workerStatus.resource;
            auto &worker = workerStatus.worker;

            // Keep track of whether the worker has passed a 10-distance position
            auto &tenDistancePositions = tenDistancePositionsFor(resource);
            if (!workerStatus.passed10DistancePosition && tenDistancePositions.contains(*currentPosition))
            {
                workerStatus.passed10DistancePosition = currentPosition;

#if TAKEOVER_DEBUG
                CherryVis::log(worker->id) << "Will reach 10-distance position in LF+1 from here";
#endif
            }

            // Run the state machine
            // State 0: haven't detected a worker to take over from yet
            // State 1: have initialized takeover and may be resending commands up to the takeover frame
            // State 2: are following a planned path using our normal approach optimization
            // State 10: issued last command while the worker was at the patch
            // State 11: issued last command after 2 or more prior resends
            // State 12: issued last command after 1 prior resend
            // State 13: issued last command after no prior resends
            // State 20: worker was already at patch after takeover frame, so nothing further was required
            switch (workerStatus.takeoverState)
            {
                case 0:
                {
                    // Try to find another worker assigned to the patch that is mining or has recently mined
                    auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
                    if (!otherWorker || !otherWorker->exists() || (currentFrame - otherWorker->lastStartedMining) >= 100) return false;

                    // We found another worker, so compute the takeover frame
                    // We need to add an extra frame if the worker taking over might have its orders processed first
                    int addedFrame = 1;
                    if (otherWorker->orderProcessIndex > worker->orderProcessIndex)
                    {
                        addedFrame = 0;
                    }

                    // Without order timer resets, we can compute the exact takeover frame
                    workerStatus.takeoverFrame = otherWorker->lastStartedMining + 81 + addedFrame;

                    // Compute the frame of the order timer reset prior to the take over frame
                    int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(workerStatus.takeoverFrame);
                    if (previousOrderTimerReset == workerStatus.takeoverFrame) previousOrderTimerReset -= 150;

                    // If the order timer reset during mining, adjust our take over frame
                    // We always assume the worst-case scenario (needing to wait a full cycle after the mining timer expires)
                    // Because the order timer is at 6 when mining ends without a reset, we only have to wait two extra frames
                    if (previousOrderTimerReset >= otherWorker->lastStartedMining)
                    {
                        workerStatus.takeoverFrame = std::max(otherWorker->lastStartedMining + 83, previousOrderTimerReset + 8) + addedFrame;
                    }

#if TAKEOVER_DEBUG
                    CherryVis::log(worker->id)
                            << "Initializing takeover from " << otherWorker->id << ": "
                            << "otherStarted=" << otherWorker->lastStartedMining << "; "
                            << "takeOverFrame=" << workerStatus.takeoverFrame << "; "
                            << "previousOrderTimerReset=" << previousOrderTimerReset << "; "
                            << "addedFrame=" << addedFrame;
#endif

                    workerStatus.takeoverState = 1;

                    // Intentionally fall through to next case
                }
                case 1:
                {
                    auto distToPatch = resource->getDistance(worker);

                    // If the worker is at the patch and the takeover frame has passed, don't touch it
                    // We normally won't reach this but it might happen if workers are reassigned
                    if (distToPatch == 0 && currentFrame >= workerStatus.takeoverFrame)
                    {
                        workerStatus.takeoverState = 20;
                        return true;
                    }

                    // Compute the frame of the order timer reset prior to the take over frame
                    int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(workerStatus.takeoverFrame);
                    if (previousOrderTimerReset == workerStatus.takeoverFrame) previousOrderTimerReset -= 150;

                    // Now compute when we need to issue mining commands
                    // Besides issuing a mining command for the takeover frame, we also want to issue a command if the order timer resets
                    int commandFrameForTakeOver = workerStatus.takeoverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
                    int commandFrameForReset = previousOrderTimerReset - BWAPI::Broodwar->getLatencyFrames();

                    // If the takeover frame comes first, delay sending the order so it takes effect when the order timer resets instead
                    // This is to avoid situations where the second worker's command takes effect too soon,
                    // causing it to switch to a different patch
                    if (commandFrameForReset > commandFrameForTakeOver)
                    {
                        commandFrameForTakeOver = commandFrameForReset;
                    }

                    // Compute the number of frames until the next command we have to send
                    // We ignore order process timer resets if we are far enough away from the patch that it will not affect us
                    int framesToNextCommand;
                    if (currentFrame <= commandFrameForReset && (commandFrameForTakeOver - commandFrameForReset) > 3
                        && (workerStatus.passed10DistancePosition || distToPatch == 0))
                    {
                        framesToNextCommand = std::min(commandFrameForReset, commandFrameForTakeOver) - currentFrame;
                    }
                    else
                    {
                        framesToNextCommand = commandFrameForTakeOver - currentFrame;
                    }

                    // Logic for when the next command is in the future
                    if (framesToNextCommand > 0)
                    {
                        // Once the worker is close to the patch, resend commands every 2 frames to avoid the worker switching patches
                        if (framesToNextCommand % 2 == 0 && (workerStatus.passed10DistancePosition || distToPatch == 0))
                        {
#if TAKEOVER_DEBUG
                            CherryVis::log(worker->id) << "Resending to ensure mineral locking";
#endif

                            workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        }

                        // Try to plan an approach based on the approach optimization data we have
                        // TODO: implement and go to state 2

                        return true;
                    }

                    // If the worker is at the patch, just send the command
                    if (distToPatch == 0)
                    {
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Sending final command; worker is at patch";
#endif

                        workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        workerStatus.takeoverState = 10;
                        return true;
                    }

                    // The worker is still approaching the patch, so use our recorded data to figure out if we
                    // will get to the patch on time if we resend here

                    // If we've already resent twice, we assume resending again will always get to the patch on time
                    // TODO: Implement check for this
                    if (workerStatus.secondResentPosition)
                    {
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Sending final command; worker has already resent twice (or more)";
#endif

                        workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        workerStatus.takeoverState = 11;
                        return true;
                    }

                    // Reference the observations we have for when we resend at this position
                    auto &optimalPositions = optimalGatherPositionsFor(resource);
                    auto &takeoverPositions = takeoverPositionsFor(resource);
                    const std::map<int, int>* observations = nullptr;
                    if (workerStatus.resentPosition)
                    {
                        auto resentPositionDataIt = optimalPositions.find(*workerStatus.resentPosition);
                        if (resentPositionDataIt != optimalPositions.end())
                        {
                            auto secondResendMetadata = resentPositionDataIt->second.secondResendMetadataFor(currentPosition.get());
                            if (secondResendMetadata)
                            {
                                observations = &secondResendMetadata->observations.arrivalDelayAndOccurrences;
                            }
                        }
                        else
                        {
                            auto takeoverPositionDataIt = takeoverPositions.find(*workerStatus.resentPosition);
                            if (takeoverPositionDataIt != takeoverPositions.end())
                            {
                                observations = takeoverPositionDataIt->second.secondResendObservationsFor(currentPosition.get());
                            }
                        }
                    }
                    else
                    {
                        auto optimalPositionDataIt = optimalPositions.find(*currentPosition);
                        if (optimalPositionDataIt != optimalPositions.end())
                        {
                            observations = &optimalPositionDataIt->second.noSecondResendObservations.arrivalDelayAndOccurrences;
                        }
                        else
                        {
                            auto takeoverPositionDataIt = takeoverPositions.find(*currentPosition);
                            if (takeoverPositionDataIt != takeoverPositions.end())
                            {
                                observations = &takeoverPositionDataIt->second.noSecondResendArrivalDelayAndOccurrences;
                            }
                        }
                    }

                    // Resend if we either don't have any data or only see success from this position
                    bool send = true;
                    if (!observations || observations->empty())
                    {
#if TAKEOVER_DEBUG
                        CherryVis::log(worker->id) << "Sending final command; no observations for this position";
#endif
                    }
                    else
                    {
                        for (const auto &[delay, _] : *observations)
                        {
                            if (delay > 0)
                            {
#if TAKEOVER_DEBUG
                                CherryVis::log(worker->id) << "Not sending here; have observed an arrival delay of " << delay;
#endif
                                send = false;
                                break;
                            }
                        }

#if TAKEOVER_DEBUG
                        if (send)
                        {
                            CherryVis::log(worker->id) << "Sending final command; observations have no failures at this position";
                        }
#endif
                    }

                    if (send)
                    {
                        workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
                        workerStatus.takeoverState = (workerStatus.resentPosition ? 12 : 13);
                    }

                    return true;
                }
                default:
                {
                    // Handles all final states where we have sent the last command needed
                    return true;
                }
            }
        }
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto &workerStatus = gatherStatusFor(worker, depot, resource);

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            workerStatus.reset();
            return;
        }

        // Track the worker's visited positions
        auto currentPosition = workerStatus.appendCurrentPosition();

        // Don't touch the worker if it is transitioning to mine
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) return;

        // Resend the gather command if it has been scheduled for this frame
        if (workerStatus.resentOnSchedule())
        {
            if (workerStatus.resendCommandOnFrame == currentFrame)
            {
                workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending gather command on schedule";
#endif
            }
            return;
        }

        // Our logic ensures mineral locking automatically except in some specific cases:
        // - worker has been released from combat, which can leave it with a gather order to a random patch used for kiting
        // - workers have been avoiding a no-go area and returning to mining as a group, so the timing gets messed up
        // - both workers reach the patch at approximately the same time after one or both are (re)assigned
        // - we don't have enough observed resend positions and get unlucky on the order timer
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            // Hook to update our observations based on this potential failure of mineral locking
            handleStartOfMiningPatchSwitch(workerStatus);

            CherryVis::log(worker->id) << "targeting different patch; resending order";
            Log::Get() << "ERROR: patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition() << " switched patch";

            worker->gather(resourceBwapiUnit);
            workerStatus.switchedPatches = true;
            return;
        }

        // Handle case where another worker is assigned to the patch
        if (handleTakeover(workerStatus, currentPosition, resourceBwapiUnit)) return;

        auto &optimalPositions = optimalGatherPositionsFor(resource);

        // If we have a path planned, validate that we are following it
        validatePlannedPath(workerStatus, resourceBwapiUnit, optimalPositions, currentPosition);

        // Plan resends if we haven't done so already
        if (!workerStatus.resendsPlanned)
        {
            auto metadataIt = optimalPositions.find(*currentPosition);
            if (metadataIt == optimalPositions.end()) return; // haven't reached an observed position yet

            workerStatus.resendsPlanned = true;

            // Used as a reference frame when figuring out if we will be affected by an order process timer reset
            int normalPathCommandFrame = BWAPI::Broodwar->getFrameCount() - metadataIt->second.deltaToNormalPathOptimalPosition;

            auto shouldResend = [&](const PositionEvaluation &evaluation)
            {
                if (!evaluation.resendPosition) return false;
                if (evaluation.positionToTryOnExpectedPath) return true;

                // Ensure the path gets us to the patch better than the worst case of letting the worker be
                auto normalPathCollisionDelay = expectedPatchCollisionDelay(metadataIt->second.noResendCollisions,
                                                                            metadataIt->second.noResendNonCollisions);
                if (evaluation.expectedDelta > (9 + normalPathCollisionDelay)) return false;

                // If we can predict the worker's order process timer at normal arrival, check if it is better than the evaluated result
                double orderProcessTimerDelay = 4.5;
                int framesToNormalPathArrival = BWAPI::Broodwar->getLatencyFrames() + 10 - metadataIt->second.deltaToNormalPathOptimalPosition;
                if (worker->orderProcessTimer != -1 && OrderProcessTimer::framesToNextReset() > framesToNormalPathArrival)
                {
                    int orderProcessTimerAtArrival = worker->orderProcessTimer - framesToNormalPathArrival;
                    while (orderProcessTimerAtArrival < 0)
                    {
                        orderProcessTimerAtArrival += 9;
                    }
                    orderProcessTimerDelay = (double)orderProcessTimerAtArrival;
                }

                if ((normalPathCollisionDelay + orderProcessTimerDelay) < evaluation.expectedDelta)
                {
#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << std::fixed << std::setprecision(1)
                            << "Not resending as expected order timer delay " << orderProcessTimerDelay
                            << " and collision delay " << normalPathCollisionDelay
                            << " is better than expected delta " << evaluation.expectedDelta;
#endif

                    return false;
                }

                return true;
            };

            auto evaluation = evaluatePosition(normalPathCommandFrame, optimalPositions, metadataIt->second);
            if (shouldResend(evaluation))
            {
                workerStatus.plannedResendPosition = evaluation.resendPosition;
                workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(*evaluation.expectedPath.rbegin());
                if ((*workerStatus.plannedResendPosition) == (*workerStatus.plannedSecondResendPosition))
                {
                    workerStatus.plannedSecondResendPosition = nullptr;
                }

                workerStatus.expectedPath = std::move(evaluation.expectedPath);

#if OPTIMALPOSITIONS_DEBUG
                std::ostringstream out;
                out << std::fixed << std::setprecision(1) << "Planned gather command(s): ";
                if (workerStatus.plannedResendPosition)
                {
                    out << *workerStatus.plannedResendPosition;
                }
                else
                {
                    out << "none";
                }
                if (workerStatus.plannedSecondResendPosition)
                {
                    out << " : " << *workerStatus.plannedSecondResendPosition;
                }
                if (evaluation.positionToTryOnExpectedPath)
                {
                    out << " (exploring)";
                }
                else
                {
                    out << " expected delta " << evaluation.expectedDelta;
                }

                CherryVis::log(worker->id) << out.str();
#endif
            }
        }

        // Send commands we have pre-planned
        if (workerStatus.resendsPlanned)
        {
            auto handlePlannedResend = [&](
                    const std::shared_ptr<const PositionAndVelocity> &plannedPosition,
                    const std::shared_ptr<const PositionAndVelocity> &resentPosition)
            {
                if (!plannedPosition) return; // nothing planned for this position
                if (resentPosition) return; // already resent
                if ((*plannedPosition) != (*currentPosition)) return; // not at the position yet

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending for " << *plannedPosition;
#endif

                workerStatus.sendGatherCommand(resourceBwapiUnit, currentPosition);
            };

            handlePlannedResend(workerStatus.plannedResendPosition, workerStatus.resentPosition);
            handlePlannedResend(workerStatus.plannedSecondResendPosition, workerStatus.secondResentPosition);

            // Remove this position from the expected path
            if (!workerStatus.expectedPath.empty()) workerStatus.expectedPath.pop_front();
        }
    }
}