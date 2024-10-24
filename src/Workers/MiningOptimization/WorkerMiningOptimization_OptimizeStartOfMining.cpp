// Worker mining optimization is split into multiple files
// This file contains the logic that optimizes the start of mining

#include "WorkerMiningOptimization.h"

#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"

#define EPSILON 0.001

namespace WorkerMiningOptimization
{
    namespace
    {
/*
        unsigned int expectedGatherDelay(const PositionObservationMetadata &metadata)
        {
            // Gather the aggregated delays across all nonoptimal resends
            unsigned int count = metadata.successes;
            unsigned int delays = 0;
            for (const auto &[_, delayAndMetadata] : metadata.failurePositionMetadata)
            {
                for (const auto &[delay, secondMetadata] : delayAndMetadata)
                {
                    count += secondMetadata.successes + secondMetadata.failures;

                    // If the delay is exactly LF after, the unit will be busy and this will incur an extra frame of delay
                    int effectiveDelay = delay + ((delay == BWAPI::Broodwar->getFrameCount()) ? 1 : 0);
                    delays += secondMetadata.successes * effectiveDelay + secondMetadata.failures * (effectiveDelay + 5);
                }
            }

            if (count == 0) return 0;

            // Integer division with ceiling
            return (delays + count - 1) / count;
        }

        bool shouldResendGatherCommand(const MyWorker &worker,
                                       const PositionObservationMetadata &positionMetadata,
                                       unsigned int &expectedDelay)
        {
            expectedDelay = expectedGatherDelay(positionMetadata);
            if (expectedDelay == 0) return true;

            // If we can predict the order timer value at arrival, check if it is better or worse than the observed results on this patch
            if (worker->orderProcessTimer != -1)
            {
                int orderProcessTimerAtArrival = worker->orderProcessTimer - BWAPI::Broodwar->getLatencyFrames() - 11 + 1;
                while (orderProcessTimerAtArrival < 0)
                {
                    orderProcessTimerAtArrival += 9;
                }

#if OPTIMALPOSITIONS_DEBUG
                if (expectedDelay >= orderProcessTimerAtArrival)
                {
                    CherryVis::log(worker->id) << "Not resending at " << positionMetadata << " as expected delay " << expectedDelay
                                               << " is no better than expected order process timer at arrival " << orderProcessTimerAtArrival;
                }
#endif

                return expectedDelay < orderProcessTimerAtArrival;
            }

            // The order timer will be randomized at arrival, so resend if the metadata indicates we on average would benefit
#if OPTIMALPOSITIONS_DEBUG
            if (expectedDelay >= 5)
            {
                CherryVis::log(worker->id) << "Not resending at " << positionMetadata << " as expected delay " << expectedDelay
                                           << " is no better than average delay";
            }
#endif

            return expectedDelay < 5;
        }

        bool shouldResendGatherCommand(const MyWorker &worker,
                                       const std::map<int, PositionObservationMetadata> &positionMetadata,
                                       bool &mayGetUnitBusy)
        {
            // The logic is just checking if successes outweigh failures
            // In reality we don't really see failures so this doesn't matter too much
            unsigned int successes = 0;
            unsigned int failures = 0;
            for (const auto &[_, metadata] : positionMetadata)
            {
                successes += metadata.successes;
                failures += metadata.failures;
            }

            mayGetUnitBusy = positionMetadata.contains(BWAPI::Broodwar->getLatencyFrames()) &&
                    !positionMetadata.contains(BWAPI::Broodwar->getLatencyFrames() + 1);

            return successes >= failures;
        }

        void optimizeArrival(const MyWorker &worker,
                             const Resource &resource,
                             WorkerGatherStatus &workerStatus,
                             std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositions,
                             const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (workerStatus.secondResentPosition || workerStatus.resentOnSchedule()) return;

            auto handleOrderProcessTimerReset = [&](unsigned int expectedDelay)
            {
                int framesFromCommandToReset = OrderProcessTimer::framesToNextReset() - BWAPI::Broodwar->getLatencyFrames();
                if (framesFromCommandToReset > 0 && framesFromCommandToReset < (12 + expectedDelay))
                {
                    // Send a command to take effect on the reset frame if it is coming soon
                    // Otherwise just let it take its course
                    if (framesFromCommandToReset < 5)
                    {
                        workerStatus.resendCommandOnFrame = currentFrame + framesFromCommandToReset;
#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Scheduled gather command for approach optimization on frame "
                                                   << workerStatus.resendCommandOnFrame;
                        CherryVis::log(resource->id) << "Scheduled gather command for approach optimization on frame "
                                                     << workerStatus.resendCommandOnFrame;
#endif

                    }
                    return true;
                }

                return false;
            };

            if (!workerStatus.resentPosition)
            {
                auto optimalGatherPositionIt = optimalGatherPositions.find(*currentPosition);
                unsigned int expectedDelay = 0;
                if (optimalGatherPositionIt != optimalGatherPositions.end() &&
                    shouldResendGatherCommand(worker, optimalGatherPositionIt->second, expectedDelay))
                {
                    if (handleOrderProcessTimerReset(expectedDelay))
                    {
                    }
                    else if (worker->gather(resource->getBwapiUnitIfVisible()))
                    {
                        workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Resending gather command for approach optimization at position "
                                                   << optimalGatherPositionIt->second;
                        CherryVis::log(resource->id) << "Resending gather command for approach optimization at position "
                                                     << optimalGatherPositionIt->second;
#endif
                    }
                    else
                    {
                        workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                                   << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
                    }
                }

                return;
            }

            auto resendPositionDataIt = optimalGatherPositions.find(*workerStatus.resentPosition);
            if (resendPositionDataIt == optimalGatherPositions.end()) return;

            auto &resendPositionData = resendPositionDataIt->second;

            auto optimalGatherPositionIt = resendPositionData.failurePositionMetadata.find(*currentPosition);
            bool mayGetUnitBusy = false;
            if (optimalGatherPositionIt != resendPositionData.failurePositionMetadata.end() &&
                shouldResendGatherCommand(worker, optimalGatherPositionIt->second, mayGetUnitBusy))
            {
                if (handleOrderProcessTimerReset(0))
                {
                }
                else if (mayGetUnitBusy)
                {
                    // Sending the command now will result in Unit_Busy, so schedule it for the next frame
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);
                    workerStatus.secondResentPosition = currentPosition;
                }
                else if (worker->gather(resource->getBwapiUnitIfVisible()))
                {
                    workerStatus.secondResentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Resending second gather command for approach optimization";
                    CherryVis::log(resource->id) << "Resending second gather command for approach optimization";
#endif

                }
                else
                {
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "Failed to send second gather command for approach optimization for " << worker->id << " @ "
                               << worker->getTilePosition() << ": " << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send second gather command for approach optimization; last error "
                                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(resource->id) << "Failed to send second gather command for approach optimization; last error "
                                                 << BWAPI::Broodwar->getLastError();
#endif
                }
            }
        }
        */

/*
        void planResends(const Resource &resource,
                         const MyWorker &worker,
                         const std::shared_ptr<const PositionAndVelocity> &currentPosition,
                         WorkerGatherStatus &workerStatus)
        {


            if (workerStatus.resendsPlanned) return;

            // Plan the resends once we find a position we have metadata about
            auto &optimalGatherPositions = optimalGatherPositionsFor(resource);
            auto currentMetadataIt = optimalGatherPositions.find(*currentPosition);
            if (currentMetadataIt == optimalGatherPositions.end()) return;

            auto currentPositionDeltaToNormalPath = currentMetadataIt->second.deltaToNormalPathOptimalPosition;

            workerStatus.resendsPlanned = true;

            auto explorePosition = [](const PositionObservationMetadata *metadata)
            {
                if (!metadata->hasPositionToTry) return false;
                if (WorkerMiningOptimization::isExploring()) return true;

                // Also explore the single most common optimal position even if we aren't exploring
                return metadata->deltaToNormalPathOptimalPosition == 0 && metadata->noSecondResendObservations.empty();
            };

            auto planPosition = [&](const PositionObservationMetadata *metadata, int secondResendIndex = -1)
            {
                workerStatus.plannedResendPosition = std::make_shared<PositionAndVelocity>(metadata->pos);

                // If exploring, find the next resend position that hasn't been tried yet
                if (explorePosition(metadata))
                {
                    if (!metadata->noSecondResendObservations.empty())
                    {
                        for (const auto &secondResendMetadata : metadata->secondResendMetadata)
                        {
                            if (secondResendMetadata.observations.empty())
                            {
                                workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(secondResendMetadata.pos);
                                return;
                            }
                        }
#if OPTIMALPOSITIONS_DEBUG
                        Log::Get() << "ERROR: Position marked for trying but no untried position was found: " << *metadata
                                   << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                    }
                    return;
                }

                // If a specific second resend position was specified, find it
                if (secondResendIndex != -1)
                {
                    for (const auto &secondResendMetadata : metadata->secondResendMetadata)
                    {
                        if (secondResendMetadata.deltaToFirstResend == secondResendIndex)
                        {
                            workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(secondResendMetadata.pos);
                            return;
                        }
                    }

#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "ERROR: Could not find second resend index " << secondResendIndex << ": " << *metadata
                               << "; worker id " << worker->id << " @ " << worker->getTilePosition();
#endif
                    return;
                }

                // Set the second resend position to use if applicable
                auto secondResendPositionMetadata = metadata->optimalSecondResendPositionMetadata();
                if (secondResendPositionMetadata)
                {
                    workerStatus.plannedSecondResendPosition = std::make_shared<PositionAndVelocity>(secondResendPositionMetadata->pos);
                }
            };

#if OPTIMALPOSITIONS_DEBUG
            auto dbg = [&](const std::string &label = "")
            {
                std::ostringstream out;
                out << "Planned gather command(s): ";
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
                if (!label.empty())
                {
                    out << " (" << label << ")";
                }

                if (workerStatus.plannedResendPosition)
                {
                    auto resentPositionDataIt = optimalGatherPositions.find(*workerStatus.plannedResendPosition);
                    if (resentPositionDataIt != optimalGatherPositions.end())
                    {
                        auto &resentPositionData = resentPositionDataIt->second;

                        auto secondResendData = resentPositionData.secondResendMetadataFor(workerStatus.plannedSecondResendPosition.get());
                        auto &observations = secondResendData ? secondResendData->observations : resentPositionData.noSecondResendObservations;
                        if (!observations.empty())
                        {
                            out << " expected delay " << observations.mostCommonArrivalDelay();
                        }
                    }
                }
                CherryVis::log(worker->id) << out.str();
            };
#endif

            // Do an initial scan to find the best option in the absence of order timer resets
            // If we find there will be an order timer reset, we do a second scan to figure out how best to optimize that
            auto currentMetadata = &currentMetadataIt->second;
            bool needOrderProcessTimerOptimization = false;
            while (true)
            {
                // Exploring this position
                if (explorePosition(currentMetadata))
                {
                    planPosition(currentMetadata);

#if OPTIMALPOSITIONS_DEBUG
                    dbg("exploring");
#endif
                    break;
                }

                if (!currentMetadata->followingHasPositionToTry || !WorkerMiningOptimization::isExploring())
                {
                    if (currentMetadata->bestDelta < currentMetadata->bestFollowingPositionDelta)
                    {
                        // Check if an order process timer reset will affect this position
                        int framesToResendAppliedFrame = BWAPI::Broodwar->getLatencyFrames()
                                                         + currentMetadata->deltaToNormalPathOptimalPosition
                                                         - currentPositionDeltaToNormalPath;

                        auto secondResendPositionMetadata = currentMetadata->optimalSecondResendPositionMetadata();
                        if (secondResendPositionMetadata) framesToResendAppliedFrame += secondResendPositionMetadata->deltaToFirstResend;

                        int framesToReset = OrderProcessTimer::framesToNextReset(BWAPI::Broodwar->getFrameCount() + framesToResendAppliedFrame);
                        if (framesToReset <= 0 || framesToReset >= 12) // no reset to worry about
                        {
                            // If we can predict the order timer value at arrival, check if it is better or worse than the expected result here
                            if (worker->orderProcessTimer != -1)
                            {
                                int orderProcessTimerAtArrival =
                                        worker->orderProcessTimer - BWAPI::Broodwar->getLatencyFrames() - 10 + currentPositionDeltaToNormalPath;
                                while (orderProcessTimerAtArrival < 0)
                                {
                                    orderProcessTimerAtArrival += 9;
                                }

                                if (currentMetadata->bestDelta > orderProcessTimerAtArrival)
                                {
#if OPTIMALPOSITIONS_DEBUG
                                    dbg((std::ostringstream()
                                            << "order timer " << orderProcessTimerAtArrival
                                            << " better than best delta " << currentMetadata->bestDelta).str());
#endif

                                    break;
                                }
                            }

                            planPosition(currentMetadata);
#if OPTIMALPOSITIONS_DEBUG
                            dbg();
#endif
                            break;
                        }

                        needOrderProcessTimerOptimization = true;
                        break;
                    }
                }

                if (!currentMetadata->next) break;
                currentMetadataIt = optimalGatherPositions.find(*currentMetadata->next);
                if (currentMetadataIt == optimalGatherPositions.end()) break;
                currentMetadata = &currentMetadataIt->second;
            }
            
            // If we need to optimize the order process timer reset, scan through again and find the best option
            if (needOrderProcessTimerOptimization)
            {
                currentMetadata = &currentMetadataIt->second;
                int bestMiningStartDelta = INT_MAX;
                PositionObservationMetadata *bestPosition = nullptr;
                int bestSecondResendIndex = 0;
                while (true)
                {
                    // For each resend position here, compute when we expect to start mining
                    // This may be known or an estimate depending on whether the order process timer reset is allowed to apply at the patch
                    auto handlePositionForOrderProcessTimerReset = [&](const SecondResendPositionObservationMetadata *secondResendMetadata)
                    {
                        auto &observations = (secondResendMetadata ? secondResendMetadata->observations : currentMetadata->noSecondResendObservations);
                        if (observations.empty()) return;

                        // Ensure the worker arrives to the patch on time
                        int arrivalDelay = observations.mostCommonArrivalDelay();
                        if (arrivalDelay < 0) return;

                        int framesToResendAppliedFrame = BWAPI::Broodwar->getLatencyFrames()
                                                         + currentMetadata->deltaToNormalPathOptimalPosition
                                                         - currentPositionDeltaToNormalPath
                                                         + (secondResendMetadata ? secondResendMetadata->deltaToFirstResend : 0);

                        int noResetMiningDelta = currentMetadata->deltaToNormalPathOptimalPosition
                                                 + (secondResendMetadata ? secondResendMetadata->deltaToFirstResend : 0)
                                                 + arrivalDelay;

                        int framesToReset = OrderProcessTimer::framesToNextReset(BWAPI::Broodwar->getFrameCount() + framesToResendAppliedFrame);
                        int miningDelta =
                                (framesToReset <= 0 || framesToReset >= 12)
                                ? noResetMiningDelta
                                : (noResetMiningDelta - arrivalDelay + 4);

                        if (miningDelta <= bestMiningStartDelta)
                        {
                            bestMiningStartDelta = miningDelta;
                            bestPosition = currentMetadata;
                            bestSecondResendIndex = secondResendMetadata ? secondResendMetadata->deltaToFirstResend : -1;
                        }
                    };
                    handlePositionForOrderProcessTimerReset(nullptr);
                    for (const auto &secondResendMetadata : currentMetadata->secondResendMetadata)
                    {
                        handlePositionForOrderProcessTimerReset(&secondResendMetadata);
                    }

                    if (!currentMetadata->next) break;
                    currentMetadataIt = optimalGatherPositions.find(*currentMetadata->next);
                    if (currentMetadataIt == optimalGatherPositions.end()) break;
                    currentMetadata = &currentMetadataIt->second;
                }

                if (bestPosition)
                {
                    planPosition(bestPosition, bestSecondResendIndex);
#if OPTIMALPOSITIONS_DEBUG
                    dbg("order timer reset");
                }
                else
                {
                    CherryVis::log(worker->id) << "Not resending anything because of pending order process timer reset";
#endif
                }
            }
        }
            */

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

        double computeExpectedDelta(int normalPathCommandFrame,
                                    const PositionObservationMetadata &positionMetadata,
                                    int deltaToFirstResend,
                                    const ResendPositionObservations &observations)
        {
            double expectedMiningDelay = observations.expectedMiningDelay(
                    false,
                    normalPathCommandFrame + positionMetadata.deltaToNormalPathOptimalPosition + deltaToFirstResend);

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
            double expectedDelta = computeExpectedDelta(normalPathCommandFrame, positionMetadata, deltaToFirstResend, observations);
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
                    if (workerStatus.worker->gather(resourceBwapiUnit))
                    {
                        workerStatus.secondResentPosition = currentPosition;
#if OPTIMALPOSITIONS_DEBUG
                    }
                    else
                    {
                        Log::Get() << "Failed to send replanned second gather command for "
                                   << workerStatus.worker->id << " @ " << workerStatus.worker->getTilePosition() << ": "
                                   << BWAPI::Broodwar->getLastError();
                        CherryVis::log(workerStatus.worker->id) << "Failed to send replanned second gather command; last error "
                                                   << BWAPI::Broodwar->getLastError();
#endif
                    }
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
            double expectedDelta = computeExpectedDelta(normalPathCommandFrame,
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
//            handleStartOfMiningPatchSwitch(workerStatus, resource, tenDistancePositions, takeoverResendPositions);

            CherryVis::log(worker->id) << "targeting different patch; resending order";
            Log::Get() << "ERROR: patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition() << " switched patch";

            worker->gather(resourceBwapiUnit);
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
                if (worker->gather(resourceBwapiUnit))
                {
                    if (!workerStatus.resentPosition)
                    {
                        workerStatus.resentPosition = currentPosition;
                        workerStatus.resendCommandOnFrame = -2;
                    }
                    else if (!workerStatus.secondResentPosition)
                    {
                        workerStatus.secondResentPosition = currentPosition;
                        workerStatus.resendCommandOnFrame = -2;
                    }
#if OPTIMALPOSITIONS_DEBUG
                }
                else
                {
                    Log::Get() << "Failed to send scheduled gather command for "
                               << worker->id << " @ " << worker->getTilePosition() << ": "
                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send scheduled gather command; last error "
                                               << BWAPI::Broodwar->getLastError();
#endif
                }

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending gather command on schedule";
#endif
            }
            return;
        }

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
                    std::shared_ptr<const PositionAndVelocity> &resentPosition)
            {
                if (!plannedPosition) return; // nothing planned for this position
                if (resentPosition) return; // already resent
                if ((*plannedPosition) != (*currentPosition)) return; // not at the position yet

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending for " << *plannedPosition;
#endif

                if (worker->gather(resourceBwapiUnit))
                {
                    resentPosition = currentPosition;
#if OPTIMALPOSITIONS_DEBUG
                }
                else
                {
                    Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                    CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
                }
            };

            handlePlannedResend(workerStatus.plannedResendPosition, workerStatus.resentPosition);
            handlePlannedResend(workerStatus.plannedSecondResendPosition, workerStatus.secondResentPosition);

            // Remove this position from the expected path
            if (!workerStatus.expectedPath.empty()) workerStatus.expectedPath.pop_front();
        }

//
//        if (workerStatus.resentPosition)
//        {
//            if (workerStatus.secondResentPosition) return;
//
//            auto metadataIt = optimalPositions.find(*workerStatus.resentPosition);
//            if (metadataIt != optimalPositions.end())
//            {
//                auto secondResentIt = metadataIt->second.secondResendMetadata.find(*currentPosition);
//                if (secondResentIt != metadataIt->second.secondResendMetadata.end()
//                    && secondResentIt->second.deltaToFirstResend != BWAPI::Broodwar->getLatencyFrames()
//                    && secondResentIt->second.observations.empty())
//                {
//                    if (worker->gather(resourceBwapiUnit))
//                    {
//                        workerStatus.secondResentPosition = currentPosition;
//                    }
//                    else
//                    {
//#if OPTIMALPOSITIONS_DEBUG
//                        Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
//                                   << BWAPI::Broodwar->getLastError();
//                        CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
//                        CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
//#endif
//                    }
//                }
//            }
//        }
//        else
//        {
//            auto metadataIt = optimalPositions.find(*currentPosition);
//            if (metadataIt != optimalPositions.end() && metadataIt->second.hasUntriedPosition())
//            {
//                if (worker->gather(resourceBwapiUnit))
//                {
//                    workerStatus.resentPosition = currentPosition;
//                }
//                else
//                {
//#if OPTIMALPOSITIONS_DEBUG
//                    Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
//                               << BWAPI::Broodwar->getLastError();
//                    CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
//                    CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
//#endif
//                }
//            }
//        }

        /*
        // Validate the observed path matches planned resends
        if (workerStatus.resendsPlanned)
        {
            // TODO
        }

        planResends(resource, worker, currentPosition, workerStatus);

        // Send commands we have pre-planned
        if (workerStatus.resendsPlanned)
        {
            auto handlePlannedResend = [&](
                    const std::shared_ptr<const PositionAndVelocity> &plannedPosition,
                    std::shared_ptr<const PositionAndVelocity> &resentPosition)
            {
                if (!plannedPosition) return; // nothing planned for this position
                if (resentPosition) return; // already resent
                if (!plannedPosition->equals(*currentPosition)) return; // not at the position yet

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending for " << *plannedPosition;
#endif

                if (worker->gather(resourceBwapiUnit))
                {
                    resentPosition = currentPosition;
                }
                else
                {
#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                    CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
                }
            };

            handlePlannedResend(workerStatus.plannedResendPosition, workerStatus.resentPosition);
            handlePlannedResend(workerStatus.plannedSecondResendPosition, workerStatus.secondResentPosition);
            return;
        }
*/

/*

        // If we have already submitted our second resend, we have nothing left to do
        if (workerStatus.secondResentPosition) return;

        // TODO: Consider positions where the resend gets us to the patch faster/slower
        auto handleOrderProcessTimerReset = [&]()
        {
            int framesFromCommandToReset = OrderProcessTimer::framesToNextReset() - BWAPI::Broodwar->getLatencyFrames();
            if (framesFromCommandToReset <= 0 || framesFromCommandToReset >= 12) return false; // no reset to worry about



            if (framesFromCommandToReset > 0 && framesFromCommandToReset < 12)
            {
                // Send a command to take effect on the reset frame if it is coming soon
                // Otherwise just let it take its course
                if (framesFromCommandToReset < 5)
                {
                    workerStatus.resendCommandOnFrame = currentFrame + framesFromCommandToReset;
#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Scheduled gather command for approach optimization on frame "
                                               << workerStatus.resendCommandOnFrame;
                    CherryVis::log(resource->id) << "Scheduled gather command for approach optimization on frame "
                                                 << workerStatus.resendCommandOnFrame;
                }
                else
                {
                    CherryVis::log(worker->id) << "Not resending at " << *currentPosition << " due to pending order timer reset";
                    CherryVis::log(resource->id) << "Not resending at " << *currentPosition << " due to pending order timer reset";
#endif
                }
                return true;
            }

            return false;
        };


        // Logic for when we are looking for the first position to resend the command
        if (!workerStatus.resentPosition)
        {
            // See if we have any metadata here
            auto optimalGatherPositionIt = optimalGatherPositions.find(*currentPosition);
            if (optimalGatherPositionIt == optimalGatherPositions.end()) return;


            // Position evaluator
            bool exploring = true;
            auto shouldUsePosition = [&](const PositionObservationMetadata &metadata)
            {
                // Always use a position if we are exploring something on it
                if (metadata.hasPositionToTry) return true;

                // Always allow a following position to explore
                if (metadata.followingHasPositionToTry) return false;

                // Otherwise use this position if it gives the best delta compared to its following positions
                if (metadata.bestDelta < metadata.bestFollowingPositionDelta)
                {
                    exploring = false;
                    return true;
                }

                return false;
            };

            // Check if this is a position we want to use
            auto optimalGatherPositionIt = optimalGatherPositions.find(*currentPosition);
            if (optimalGatherPositionIt != optimalGatherPositions.end() && shouldUsePosition(optimalGatherPositionIt->second))
            {
                if (!exploring && !optimalGatherPositionIt->second.requiresSecondResend() && handleOrderProcessTimerReset())
                {
                    // We only consider order process timer resets here if this is the only resend we would normally issue
                    // Otherwise we consider order process timer resets when handling the second resend
                }
                else if (worker->gather(resource->getBwapiUnitIfVisible()))
                {
                    workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Resending for " << optimalGatherPositionIt->second
                                               << (exploring ? " (exploring)" : "");
#endif
                }
                else
                {
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                    CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
                }
            }
#if OPTIMALPOSITIONS_DEBUG
            else if (optimalGatherPositionIt != optimalGatherPositions.end())
            {
                CherryVis::log(worker->id) << "Not choosing position " << optimalGatherPositionIt->second;
            }
#endif
            return;
        }

        // Get the data for the first resend position
        auto resentPositionIt = optimalGatherPositions.find(*workerStatus.resentPosition);
        if (resentPositionIt == optimalGatherPositions.end()) // shouldn't happen
        {
            Log::Get() << "ERROR: Couldn't find resent position in map";
            return;
        }
        auto &resentPositionData = resentPositionIt->second;

        // If the position doesn't require a second resend, return here
        if (!resentPositionData.requiresSecondResend()) return;

        // Second resend position evaluator
        bool exploring = true;
        auto shouldUseSecondResendPosition = [&](const SecondResendPositionObservationMetadata *metadata)
        {
            if (!metadata) return false;

            // Always use a position if we are exploring it
            if (metadata->observations.empty()) return true;

            // Never use a position if we have explored it and want to explore a later one
            if (resentPositionData.hasPositionToTry) return false;

            // Use the position if it is the one that gives the best delta
            exploring = false;
            for (const auto &[delta, _] : metadata->observations)
            {
                if (delta == (resentPositionData.bestDelta - resentPositionData.deltaToNormalPathOptimalPosition - metadata->deltaToFirstResend))
                {
                    return true;
                }
            }
            return false;
        };

        // Check if this position matches a second resend position we want to use
        auto secondResendPositionMetadata = resentPositionData.secondResendMetadataFor(*currentPosition);
        if (shouldUseSecondResendPosition(secondResendPositionMetadata))
        {
            if (!exploring && handleOrderProcessTimerReset())
            {}
            else if (worker->gather(resource->getBwapiUnitIfVisible()))
            {
                workerStatus.secondResentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Resending for " << resentPositionData << " : " << *currentPosition
                                           << (exploring ? " (exploring)" : "");
#endif
            }
            else
            {
                workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "Failed to send second gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                           << BWAPI::Broodwar->getLastError();
                CherryVis::log(worker->id) << "Failed to send second gather command; last error " << BWAPI::Broodwar->getLastError();
                CherryVis::log(resource->id) << "Failed to send second gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
            }
        }
*/

/*
        auto &optimalGatherPositions = optimalGatherPositionsFor(resource);
        auto &tenDistancePositions = tenDistancePositionsFor(resource);
        auto &takeoverResendPositions = takeoverPositionsFor(resource);
        auto &workerStatus = gatherStatusFor(worker, resource);

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            workerStatus.reset();
            return;
        }

        auto currentPosition = std::make_shared<PositionAndVelocity>(worker);

        // Clear worker status if it wasn't processed last frame
        if (workerStatus.lastProcessedFrame != (currentFrame - 1))
        {
            workerStatus.reset();
        }
        workerStatus.lastProcessedFrame = currentFrame;

        // Track the worker's visited positions
        workerStatus.positionHistory.emplace_back(currentPosition);

        // Don't touch the worker if it is transitioning to mine
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals) return;

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
            handleStartOfMiningPatchSwitch(workerStatus, resource, tenDistancePositions, takeoverResendPositions);

            CherryVis::log(worker->id) << "targeting different patch; resending order";
            Log::Get() << "patch @ " << resource->tile << "; worker " << worker->id << " @ " << worker->getTilePosition() << " switched patch";

            worker->gather(resourceBwapiUnit);
            workerStatus.positionHistory.clear();
            workerStatus.takeoverMode = 2;
            workerStatus.lastProcessedFrame = currentFrame;
            return;
        }

        // Resend the gather command if it has been scheduled for this frame
        if (workerStatus.resendCommandOnFrame == currentFrame)
        {
            worker->gather(resourceBwapiUnit);

#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(worker->id) << "Resending gather command on schedule";
            CherryVis::log(resource->id) << "Resending gather command on schedule";
#endif
            return;
        }

        // Handle case where another worker is assigned to the patch
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && (currentFrame - otherWorker->lastStartedMining) < 100)
        {
            // Keep track of whether the worker has passed a 10-distance position
            if (!workerStatus.passed10DistancePosition && tenDistancePositions.contains(*currentPosition))
            {
                workerStatus.passed10DistancePosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(worker->id) << "Will reach 10-distance position in LF+1 from here";
                CherryVis::log(resource->id) << "Will reach 10-distance position in LF+1 from here";
#endif
            }

            // Compute the optimal frame to take over from the other worker

            // We need to add an extra frame if the worker taking over might have its orders processed first
            int addedFrame = 1;
            if (otherWorker->orderProcessIndex > worker->orderProcessIndex)
            {
                addedFrame = 0;
            }

            // Without order timer resets, we can compute the exact takeover frame
            int takeOverFrame = otherWorker->lastStartedMining + 81 + addedFrame;

            // Compute the frame of the order timer reset prior to the take over frame
            int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(takeOverFrame);
            if (previousOrderTimerReset == takeOverFrame) previousOrderTimerReset -= 150;

            // If the order timer reset during mining, adjust our take over frame
            // We always assume the worst-case scenario (needing to wait a full cycle after the mining timer expires)
            // Because the order timer is at 6 when mining ends without a reset, we only have to wait two extra frames
            if (previousOrderTimerReset >= otherWorker->lastStartedMining)
            {
                takeOverFrame = std::max(otherWorker->lastStartedMining + 83, previousOrderTimerReset + 8) + addedFrame;
            }

            // Now compute when we need to issue mining commands
            // Besides issuing a mining command for the takeover frame, we also want to issue a command if the order timer resets
            int commandFrameForTakeOver = takeOverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
            int commandFrameForReset = previousOrderTimerReset - BWAPI::Broodwar->getLatencyFrames();

            // If the takeover frame comes first, delay sending the order so it takes effect when the order timer resets instead
            // This is to avoid situations where the second worker's command takes effect too soon, causing it to switch to a different patch
            if (commandFrameForReset > commandFrameForTakeOver)
            {
                commandFrameForTakeOver = commandFrameForReset;
            }

            // Compute the number of frames until the next command we have to send
            // We ignore order process timer resets if we are far enough away from the patch that it will not affect us
            auto distToPatch = resource->getDistance(worker);
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

#if TAKEOVER_DEBUG
            CherryVis::log(worker->id)
                    << "Timing for takeover from " << otherWorker->id << ": "
                    << "otherStarted=" << otherWorker->lastStartedMining << "; "
                    << "takeOverFrame=" << takeOverFrame << "; "
                    << "previousOrderTimerReset=" << previousOrderTimerReset << "; "
                    << "commandFrameForTakeOver=" << commandFrameForTakeOver << "; "
                    << "commandFrameForReset=" << commandFrameForReset << "; "
                    << "framesToNextCommand=" << framesToNextCommand << "; "
                    << "addedFrame=" << addedFrame << "; "
                    << "distToPatch=" << distToPatch << "; "
                    << "passed10Distance=" << (workerStatus.passed10DistancePosition != nullptr) << "; "
                    << "resentPosition=" << (workerStatus.resentPosition != nullptr) << "; "
                    << "takeoverMode=" << workerStatus.takeoverMode;
#endif

            // Logic for when the next command is in the future
            if (framesToNextCommand > 0)
            {
                // Reset some state in case we needed to resend because of the order timer
                workerStatus.resentPosition = nullptr;
                workerStatus.secondResentPosition = nullptr;
                workerStatus.takeoverMode = 0;

                // Once the worker is close to the patch, resend commands every 2 frames to avoid the worker switching patches
                if (framesToNextCommand % 2 == 0 && (workerStatus.passed10DistancePosition || distToPatch == 0))
                {
                    worker->gather(resourceBwapiUnit);
                }

                return;
            }

            switch (workerStatus.takeoverMode)
            {
                case 0:
                {
                    // Try to use normal approach optimization, which applies if the worker is not able to reach the patch by the takeover frame
                    if (distToPatch > 0)
                    {
                        optimizeArrival(worker, resource, workerStatus, optimalGatherPositions, currentPosition);
                        if (workerStatus.resentPosition || workerStatus.resentOnSchedule()) return;
                    }

                    // intentional fall-through
                }
                case 1:
                {
                    if (distToPatch == 0 && !workerStatus.resentPosition && !workerStatus.resentOnSchedule())
                    {
                        workerStatus.takeoverMode = 2;
                        worker->gather(resource->getBwapiUnitIfVisible());

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(worker->id) << "Resending gather command for takeover optimization, as have arrived at patch";
                        CherryVis::log(resource->id) << "Resending gather command for takeover optimization, as have arrived at patch";
#endif
                        break;
                    }

                    auto takeoverPositionValid = [&](const PositionObservationMetadata &takeoverPositionData)
                    {
#if OPTIMALPOSITIONS_DEBUG
                        if (takeoverPositionData.failures > 0)
                        {
                            CherryVis::log(worker->id) << "Rejecting for takeover optimization: " << takeoverPositionData;
                            CherryVis::log(resource->id) << "Rejecting for takeover optimization: " << takeoverPositionData;
                        }
#endif

                        // TODO: Revisit the logic after performing some analysis
                        return takeoverPositionData.failures == 0;
                    };

                    // Try to use takeover optimization
                    // This differs from approach optimization as it just tries to ensure the worker will reach the patch on time, not what the order
                    // process timer is
                    if (!workerStatus.resentPosition && !workerStatus.resentOnSchedule())
                    {
                        // Use the first valid position we come to
                        auto takeoverPositionIt = takeoverResendPositions.find(*currentPosition);
                        if (takeoverPositionIt != takeoverResendPositions.end() && takeoverPositionValid(takeoverPositionIt->second))
                        {
                            workerStatus.takeoverMode = 1;

                            if (worker->gather(resource->getBwapiUnitIfVisible()))
                            {
                                workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                                CherryVis::log(worker->id) << "Resending gather command for takeover optimization at position " << takeoverPositionIt->second;
                                CherryVis::log(resource->id) << "Resending gather command for takeover optimization at position "
                                                             << takeoverPositionIt->second;
#endif
                            }
                            else
                            {
                                workerStatus.resendCommandOnFrame = (currentFrame + 1);

#if OPTIMALPOSITIONS_DEBUG
                                Log::Get() << "Failed to send gather command for takeover optimization for " << worker->id << " @ "
                                           << worker->getTilePosition() << ": " << BWAPI::Broodwar->getLastError();
                                CherryVis::log(worker->id) << "Failed to send gather command for takeover optimization; last error "
                                                           << BWAPI::Broodwar->getLastError();
                                CherryVis::log(resource->id) << "Failed to send gather command for takeover optimization; last error "
                                                             << BWAPI::Broodwar->getLastError();
#endif
                            }
                        }
                        else if (workerStatus.passed10DistancePosition)
                        {
                            // As we've passed the 10-distance position, we had better send a command here
                            // We probably just lack the data needed to know which position to send from
                            workerStatus.takeoverMode = 1;
                            worker->gather(resource->getBwapiUnitIfVisible());
                            workerStatus.resentPosition = currentPosition;

#if OPTIMALPOSITIONS_DEBUG
                            CherryVis::log(worker->id) << "Resending as we have reached 10 distance and haven't observed a good position yet";
                            CherryVis::log(resource->id) << "Resending as we have reached 10 distance and haven't observed a good position yet";
#endif
                        }
                    }
                    break;
                }
                case 2:
                {
                    // Worker is at patch and a command has been sent, so no further orders needed
                    return;
                }
            }

            return;
        }

        // Single worker approach optimization
        optimizeArrival(worker, resource, workerStatus, optimalGatherPositions, currentPosition);

        */
    }
}