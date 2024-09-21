#include "WorkerMiningOptimization.h"

#include "Workers.h"
#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"
#include "FileTools.h"
#include "CsvTools.h"
#include "Units.h"

#if INSTRUMENTATION_ENABLED
#define TAKEOVER_DEBUG false
#define OPTIMALPOSITIONS_DEBUG true
#endif

#define SAVED_POSITIONS_ONLY false

namespace WorkerMiningOptimization
{
    namespace
    {
        struct OptimalGatherPositionMetadata
        {
        public:
            PositionAndVelocity pos;

            // How many times we have observed this as being the optimal position when not resending the order
            unsigned int observations = 0;

            // How many times the order was resent at this position and was optimal
            unsigned int optimalResends = 0;

            // How many times the order was resent at this position and was nonoptimal
            unsigned int nonoptimalResends = 0;

            // Details of the optimal positions arising from the nonoptimal resends
            std::map<PositionAndVelocity, std::map<int, OptimalGatherPositionMetadata>> nonoptimalResendOptimalPositions;

            void trackOptimalObservation()
            {
                if (atObservationCap()) return;
                observations++;
            }

            void trackOptimalResend()
            {
                if (atObservationCap()) return;
                optimalResends++;
            }

            void trackNonoptimalResend(const PositionAndVelocity &optimalPosition,
                                       const std::shared_ptr<PositionAndVelocity> &secondResendPosition,
                                       int resentDelta,
                                       int secondResentDelta)
            {
                if (atObservationCap()) return;

                // A special case is if the worker actually arrived at the patch faster because of the resend
                // This doesn't actually help us, since the worker has to wait for the order process timer to reach 0, but
                // we actually get the same performance as we expected (as if the path hadn't changed)
                // So we just treat this as an optimal resend
                if (resentDelta > 0)
                {
                    optimalResends++;
                    return;
                }

                nonoptimalResends++;

                auto &optimalPositionMetadata = nonoptimalResendOptimalPositions[optimalPosition][-resentDelta];
                if (!optimalPositionMetadata.atObservationCap())
                {
                    if (!secondResendPosition)
                    {
                        optimalPositionMetadata.trackOptimalObservation();
                    }
                    else if (secondResentDelta == 0)
                    {
                        optimalPositionMetadata.trackOptimalResend();
                    }
                }

                if (secondResendPosition && secondResentDelta != 0)
                {
                    auto &secondResendPositionMetadata = nonoptimalResendOptimalPositions[*secondResendPosition][-resentDelta];
                    if (!secondResendPositionMetadata.atObservationCap())
                    {
                        (secondResentDelta > 0 ? secondResendPositionMetadata.optimalResends : secondResendPositionMetadata.nonoptimalResends)++;
                    }
                }
            }
            
            [[nodiscard]] unsigned int expectedDelay() const
            {
                // Gather the aggregated delays across all nonoptimal resends
                unsigned int count = optimalResends;
                unsigned int delays = 0;
                for (const auto &[_, delayAndMetadata] : nonoptimalResendOptimalPositions)
                {
                    for (const auto &[delay, metadata] : delayAndMetadata)
                    {
                        count += metadata.optimalResends + metadata.nonoptimalResends;

                        // If the delay is exactly LF after, the unit will be busy and this will incur an extra frame of delay
                        int effectiveDelay = delay + ((delay == BWAPI::Broodwar->getFrameCount()) ? 1 : 0);
                        delays += metadata.optimalResends * effectiveDelay + metadata.nonoptimalResends * (effectiveDelay + 5);
                    }
                }

                if (count == 0) return 0;

                // Integer division with ceiling
                return (delays + count - 1) / count;
            }

        private:
            [[nodiscard]] bool atObservationCap() const
            {
                // Set a cap on how many observations we track to reduce computation time
                // Beyond a certain point additional observations are not going to have any impact anyway
                return (observations + optimalResends + nonoptimalResends) >= 1000;
            }
        };

        std::ostream &operator<<(std::ostream &os, const OptimalGatherPositionMetadata &optimalGatherPositionMetadata)
        {
            os << optimalGatherPositionMetadata.pos
               << " (o=" << optimalGatherPositionMetadata.observations
               << " opt=" << optimalGatherPositionMetadata.optimalResends
               << " not=" << optimalGatherPositionMetadata.nonoptimalResends
               << ")";

            return os;
        }

        bool shouldResendGatherCommand(const MyWorker &worker,
                                       const OptimalGatherPositionMetadata &positionMetadata,
                                       unsigned int &expectedDelay,
                                       int frameDelta = 0)
        {
            expectedDelay = positionMetadata.expectedDelay();
            if (expectedDelay == 0) return true;

            // If we can predict the order timer value at arrival, check if it is better or worse than the observed results on this patch
            if (worker->orderProcessTimer != -1)
            {
                int orderProcessTimerAtArrival = worker->orderProcessTimer - BWAPI::Broodwar->getLatencyFrames() - 11 - frameDelta + 1;
                while (orderProcessTimerAtArrival < 0) orderProcessTimerAtArrival += 9;

                bool result = expectedDelay < orderProcessTimerAtArrival;

#if OPTIMALPOSITIONS_DEBUG
                if (!result)
                {
                    CherryVis::log(worker->id) << "Not resending gather command at position " << positionMetadata
                        << "; orderProcessTimerAtArrival=" << orderProcessTimerAtArrival;
                }
#endif

                return result;
            }

            // The order timer will be randomized at arrival, so resend if the metadata indicates we on average would benefit
            bool result = expectedDelay < 5;

#if OPTIMALPOSITIONS_DEBUG
            if (!result)
            {
                CherryVis::log(worker->id) << "Not resending gather command at position " << positionMetadata;
            }
#endif

            return result;
        }

        struct WorkerGatherStatus
        {
            // The last frame this worker was optimized
            int lastProcessedFrame;

            // The recent positions this worker has visited while on its way to gather
            std::vector<std::shared_ptr<PositionAndVelocity>> positionHistory;

            // The position at which the gather command was resent, or nullptr if it hasn't been resent
            std::shared_ptr<PositionAndVelocity> resentPosition;

            // The position at which the gather command was resent again, or nullptr if it hasn't been resent
            std::shared_ptr<PositionAndVelocity> secondResentPosition;

            // Used to mark that the worker should have the gather command resent on this specific frame
            int resendCommandOnFrame;

            // Tracks whether the worker has passed an optimal resend position
            bool passedResendPosition;

            // Tracks whether the worker has sent a gather command to optimize takeover from another worker
            bool resentCommandForTakeover;

            WorkerGatherStatus() : lastProcessedFrame(-2), resendCommandOnFrame(-2), passedResendPosition(false), resentCommandForTakeover(false) {}

            void reset()
            {
                lastProcessedFrame = -2;
                positionHistory.clear();
                resentPosition = nullptr;
                secondResentPosition = nullptr;
                resendCommandOnFrame = -2;
                passedResendPosition = false;
                resentCommandForTakeover = false;
            }
        };

        std::map<Resource, std::map<PositionAndVelocity, OptimalGatherPositionMetadata>> resourceToOptimalGatherPositions;
        std::map<MyWorker, WorkerGatherStatus> workerGatherStatuses;

        std::string optimalGatherPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "gatherpositions_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }
    }

    void initialize()
    {
        resourceToOptimalGatherPositions.clear();
        workerGatherStatuses.clear();

        {
            std::ifstream file;
            file.open(optimalGatherPositionsFilename());
            if (file.good())
            {
                int lineNumber = 0;
                try
                {
                    // Read and parse each position
                    int count = 0;
                    while (true)
                    {
                        lineNumber++;
                        auto line = CsvTools::readNextLine(file);
                        if (line.size() < 7) break;

                        BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                        auto resource = Units::resourceAt(tile);
                        if (resource)
                        {
                            if (!PositionAndVelocity::isValidString(line[2]))
                            {
                                Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
                                continue;
                            }
                            auto pos = PositionAndVelocity::fromString(line[2]);

                            std::map<PositionAndVelocity, std::map<int, OptimalGatherPositionMetadata>> nonoptimalResendOptimalPositions;
                            for (const auto &nonoptimalResendOptimalPosition : CsvTools::tokenizeList(line[6]))
                            {
                                auto data = CsvTools::tokenizeList(nonoptimalResendOptimalPosition, ':');
                                if (data.size() < 5) continue;
                                if (!PositionAndVelocity::isValidString(data[0])) continue;
                                auto nonoptimalPos = PositionAndVelocity::fromString(data[0]);

                                nonoptimalResendOptimalPositions[nonoptimalPos].emplace(std::stoi(data[1]), OptimalGatherPositionMetadata{
                                        nonoptimalPos,
                                        (unsigned int)std::stoi(data[2]),
                                        (unsigned int)std::stoi(data[3]),
                                        (unsigned int)std::stoi(data[4])});
                            }

                            resourceToOptimalGatherPositions[resource].emplace(
                                    pos,
                                    OptimalGatherPositionMetadata{
                                        pos,
                                        (unsigned int)std::stoi(line[3]),
                                        (unsigned int)std::stoi(line[4]),
                                        (unsigned int)std::stoi(line[5]),
                                        nonoptimalResendOptimalPositions});
                            count++;
                        }
                    }

                    Log::Get() << "Read " << count << " optimal gather positions from " << optimalGatherPositionsFilename();
                }
                catch (std::exception &ex)
                {
                    Log::Get() << "Exception caught attempting to read optimal gather positions at line " << lineNumber << ": " << ex.what();
                }
            }
        }
    }

    void write()
    {
#if SAVED_POSITIONS_ONLY
        return;
#endif

        {
            std::ofstream file;
            file.open(optimalGatherPositionsFilename(true), std::ofstream::trunc);

            int count = 0;
            for (auto &resourceAndOptimalGatherPositions : resourceToOptimalGatherPositions)
            {
                for (auto &optimalOrderPosition : resourceAndOptimalGatherPositions.second)
                {
                    file << resourceAndOptimalGatherPositions.first->tile.x << ";"
                         << resourceAndOptimalGatherPositions.first->tile.y << ";"
                         << optimalOrderPosition.first << ";"
                         << optimalOrderPosition.second.observations << ";"
                         << optimalOrderPosition.second.optimalResends << ";"
                         << optimalOrderPosition.second.nonoptimalResends << ";";
                    std::string sep;
                    for (const auto &[pos, deltaAndMetadata] : optimalOrderPosition.second.nonoptimalResendOptimalPositions)
                    {
                        for (const auto &[delta, metadata] : deltaAndMetadata)
                        {
                            file << sep << pos << ":" << delta << ":"
                                 << metadata.observations << ":"
                                 << metadata.optimalResends << ":"
                                 << metadata.nonoptimalResends;
                        }
                        sep = ",";
                    }
                    file << "\n";
                    count++;
                }
            }

            file.close();
            Log::Get() << "Wrote " << count << " optimal gather positions to " << optimalGatherPositionsFilename(true);
        }
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const Resource &resource)
    {
        auto &optimalGatherPositions = resourceToOptimalGatherPositions[resource];
        auto &workerStatus = workerGatherStatuses[worker];

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            workerStatus.reset();
            return;
        }

        std::shared_ptr<PositionAndVelocity> currentPositionCache = nullptr;
        auto currentPosition = [&currentPositionCache, &worker]()
        {
            if (!currentPositionCache) currentPositionCache = std::make_shared<PositionAndVelocity>(worker);
            return currentPositionCache;
        };

        // Clear worker status if it wasn't processed last frame
        if (workerStatus.lastProcessedFrame != (currentFrame - 1))
        {
            workerStatus.reset();
        }
        workerStatus.lastProcessedFrame = currentFrame;

        // Record positions while we are approaching the patch
        // Update optimal position when we arrive at the patch
        // Note that in some cases using distance == 0 is not correct, as the worker can take a path that goes parallel to the patch
        // These are rare and not easily to unambiguously detect, so we are currently ignoring these cases
        if (resource->getDistance(worker) == 0)
        {
#if !SAVED_POSITIONS_ONLY
            if (workerStatus.positionHistory.size() >= (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                auto optimalPositionIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
                auto &optimalPosition = **optimalPositionIt;

                // Track an observation if we didn't resend an order
                if (!workerStatus.resentPosition)
                {
                    auto optimalPositionData = optimalGatherPositions.find(optimalPosition);
                    if (optimalPositionData == optimalGatherPositions.end())
                    {
                        optimalPositionData = optimalGatherPositions.emplace(optimalPosition, OptimalGatherPositionMetadata{optimalPosition}).first;

#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(resource->center)
                                         << ": Registered new position " << optimalPosition;
                        CherryVis::log(resource->id) << "Registered new position " << optimalPosition;
                        CherryVis::log(worker->id) << "Registered new position " << optimalPosition;
#endif
                    }
                    else
                    {
#if OPTIMALPOSITIONS_DEBUG
                        CherryVis::log(resource->id) << "Tracked observation on position " << optimalPosition;
                        CherryVis::log(worker->id) << "Tracked observation on position " << optimalPosition;
#endif
                    }

                    optimalPositionData->second.trackOptimalObservation();
                }
                else
                {
                    auto resendPositionData = optimalGatherPositions.find(*workerStatus.resentPosition);
                    if (resendPositionData != optimalGatherPositions.end()) // should always be true, otherwise how did we know to resend?
                    {
                        if (workerStatus.resentPosition->equals(optimalPosition))
                        {
                            // We resent and hit the optimal position, so track this
                            resendPositionData->second.trackOptimalResend();

#if OPTIMALPOSITIONS_DEBUG
                            CherryVis::log(resource->id) << "Tracked optimal resend on position " << *workerStatus.resentPosition;
                            CherryVis::log(worker->id) << "Tracked optimal resend on position " << *workerStatus.resentPosition;
#endif
                        }
                        else
                        {
                            // Find the deltas of the first and second resend positions in the positions history, which we use to measure
                            // the risk/reward of using a position
                            auto getDelta = [&](const std::shared_ptr<PositionAndVelocity> &position)
                            {
                                if (!position) return LONG_MAX;

                                for (auto it = workerStatus.positionHistory.rbegin(); it != workerStatus.positionHistory.rend(); it++)
                                {
                                    if ((*it) == position)
                                    {
                                        return std::distance(it, optimalPositionIt);
                                    }
                                }

                                return LONG_MAX;
                            };

                            auto resentDelta = getDelta(workerStatus.resentPosition);
                            if (resentDelta != LONG_MAX)
                            {
                                auto secondResentDelta = getDelta(workerStatus.secondResentPosition);
                                resendPositionData->second.trackNonoptimalResend(optimalPosition,
                                                                                 workerStatus.secondResentPosition,
                                                                                 (int)resentDelta,
                                                                                 (int)secondResentDelta);

#if OPTIMALPOSITIONS_DEBUG
                                CherryVis::log(resource->id)
                                        << "Tracked nonoptimal resend on position " << *workerStatus.resentPosition
                                        << "; delta: " << resentDelta
                                        << "; frame loss: " << ((resentDelta + 900) % 9);
                                CherryVis::log(worker->id)
                                        << "Tracked nonoptimal resend on position " << *workerStatus.resentPosition
                                        << "; delta: " << resentDelta
                                        << "; frame loss: " << ((resentDelta + 900) % 9);
#endif
                            }
                        }
                    }
                }
            }
#endif

            // Only need the data until the first arrival frame, but we mark that the worker has passed a resend position
            workerStatus.reset();
            workerStatus.passedResendPosition = true;
        }
        else
        {
//            if ((worker->lastDeliveredResource == (currentFrame - 3) || worker->lastDeliveredResource == (currentFrame - 4))
//                && !workerStatus.positionHistory.empty()
//                && (*workerStatus.positionHistory.rbegin())->equals(*currentPosition()))
//            {
//                worker->gather(resourceBwapiUnit);
//            }

            workerStatus.positionHistory.emplace_back(currentPosition());

/*
            if (worker->lastDeliveredResource == (currentFrame - 20))
            {
                std::vector<int> moved;
                for (int i = 1; i < workerStatus.positionHistory.size(); i++)
                {
                    if (!workerStatus.positionHistory[i]->equals(*workerStatus.positionHistory[i-1]))
                    {
                        moved.push_back(i);
                    }
                }

                std::ostringstream ss;
                for (auto m : moved)
                {
                    ss << ", " << m;
                }
                if (moved.size() < 15) Log::Get() << resource->tile << ": Moved " << moved.size() << ss.str();

//                Log::Get() << "Moved " << moved.size() << ss.str();
                CherryVis::log(worker->id) << "Moved " << moved.size() << ss.str();
                CherryVis::log(resource->id) << "Moved " << moved.size() << ss.str();
            }

            // Send a gather command immediately after delivering minerals
            // Sometimes workers wait a cycle before moving again, so this helps kick them into action
            if (worker->lastDeliveredResource == currentFrame)
            {
                worker->gather(resourceBwapiUnit);
                return;
            }
            */
        }

        // Our logic ensures mineral locking automatically except in some specific cases:
        // - worker has been released from combat, which can leave it with a gather order to a random patch used for kiting
        // - workers have been avoiding a no-go area and returning to mining as a group, so the timing gets messed up
        // - we don't have enough observed resend positions and get unlucky on the order timer
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            CherryVis::log(worker->id) << "targeting different patch; resending order";
            worker->gather(resourceBwapiUnit);
            workerStatus.reset();
            workerStatus.lastProcessedFrame = currentFrame;
            return;
        }

        // Handle case where another worker is assigned to the patch
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && (currentFrame - otherWorker->lastStartedMining) < 100)
        {
            // Keep track of whether the worker has passed a resend position
            if (!workerStatus.passedResendPosition && optimalGatherPositions.contains(*currentPosition()))
            {
                workerStatus.passedResendPosition = true;
#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log(resource->id) << "Passed resend position " << *currentPosition();
                CherryVis::log(worker->id) << "Passed resend position " << *currentPosition();
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
            if (previousOrderTimerReset > otherWorker->lastStartedMining)
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
            // We send regular commands to avoid having the worker switch patches
            int framesToNextCommand;
            if (currentFrame <= commandFrameForReset && (commandFrameForTakeOver - commandFrameForReset) > 3
                && workerStatus.passedResendPosition)
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
                << "distToPatch=" << resource->getDistance(worker) << "; "
                << "passedResendPosition=" << workerStatus.passedResendPosition;
#endif

            // Logic for when the next command is in the future
            if (framesToNextCommand >= 0)
            {
                // Resend commands every 2 frames to avoid the worker switching patches, but wait until the worker has passed the optimal
                // resend frame on its approach to the patch, otherwise it won't arrive in time for direct takeover anyway
                if (framesToNextCommand % 2 == 0 && workerStatus.passedResendPosition)
                {
                    worker->gather(resourceBwapiUnit);
                    workerStatus.resentCommandForTakeover = true;
                }
                return;
            }

            // Fall through to normal optimization - if we are still approaching the patch it may send a new command to optimize mining at arrival
        }

        // Resend the gather command if it has been scheduled for this frame
        if (workerStatus.resendCommandOnFrame == currentFrame)
        {
            if (worker->gather(resourceBwapiUnit))
            {
                workerStatus.resentPosition = currentPosition();
            }

#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(worker->id) << "Resending gather command on schedule";
            CherryVis::log(resource->id) << "Resending gather command on schedule";
#endif
            return;
        }

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
                }
                return true;
            }

            return false;
        };

        // Check if this worker is at an optimal position to resend the gather order
        if (!workerStatus.resentPosition)
        {
            auto here = currentPosition();
            auto optimalGatherPositionIt = optimalGatherPositions.find(*here);
            unsigned int expectedDelay = 0;
            if (optimalGatherPositionIt != optimalGatherPositions.end() && shouldResendGatherCommand(worker, optimalGatherPositionIt->second, expectedDelay))
            {
                if (handleOrderProcessTimerReset(expectedDelay))
                {}
                else if (worker->gather(resourceBwapiUnit))
                {
                    workerStatus.resentPosition = here;

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log(worker->id) << "Resending gather command at position " << optimalGatherPositionIt->second;
                    CherryVis::log(resource->id) << "Resending gather command at position " << optimalGatherPositionIt->second;
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
        }
        else
        {
            auto resendPositionData = optimalGatherPositions[*workerStatus.resentPosition];

            auto here = currentPosition();
            auto optimalGatherPositionIt = resendPositionData.nonoptimalResendOptimalPositions.find(*here);
            if (optimalGatherPositionIt != resendPositionData.nonoptimalResendOptimalPositions.end())
            {
                if (handleOrderProcessTimerReset(0))
                {}
                else if (optimalGatherPositionIt->second.size() == 1 &&
                    optimalGatherPositionIt->second.begin()->first == BWAPI::Broodwar->getLatencyFrames())
                {
                    // Sending the command now will result in Unit_Busy, so schedule it for the next frame
                    // This is taken into account in the logic to determine whether to use this position
                    workerStatus.resendCommandOnFrame = (currentFrame + 1);
                    workerStatus.secondResentPosition = here;
                }
                else if (worker->gather(resourceBwapiUnit))
                {
                    workerStatus.secondResentPosition = here;
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
        }
    }

    // Optimizes returning a resource, returning whether an order was sent to the worker.
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
//        if (resource && depot->getDistance(worker) == 0 && worker->orderProcessTimer == (BWAPI::Broodwar->getLatencyFrames() - 2)
//            && OrderProcessTimer::framesToNextReset() > BWAPI::Broodwar->getLatencyFrames())
//        {
//            auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
//            if (resourceBwapiUnit)
//            {
//                worker->gather(resourceBwapiUnit);
//            }
//        }

        // TODO
    }
}