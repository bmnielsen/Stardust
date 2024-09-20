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
            unsigned long observations = 0;

            // How many times the order was resent at this position
            unsigned long attempts = 0;

            // How many frames we have waited to mine when this was not the optimal position
            unsigned long waitFrames = 0;

            std::shared_ptr<PositionAndVelocity> lfBeforePosition;

            void trackObservation()
            {
                if (atObservationCap()) return;
                observations++;
            }

            void trackOptimalResend()
            {
                if (atObservationCap()) return;
                attempts++;
            }

            void trackNonoptimalResend(size_t delta)
            {
                if (atObservationCap()) return;
                attempts++;

                // Wait frames depend on where the wrong position was in the history
                // Sending the command too late results in a loss equal to the number of frames late
                // Sending the command early causes an extra order timer cycle
                waitFrames += (delta + 900) % 9;
            }

        private:
            [[nodiscard]] bool atObservationCap() const
            {
                // Set a cap on how many observations we track to reduce file size and ensure we don't exceed the size of unsigned long
                // Beyond a certain point additional observations are not going to have any impact anyway
                return (observations + attempts) >= 10000;
            }
        };

        std::ostream &operator<<(std::ostream &os, const OptimalGatherPositionMetadata &optimalGatherPositionMetadata)
        {
            os << optimalGatherPositionMetadata.pos
               << " (o=" << optimalGatherPositionMetadata.observations
               << " a=" << optimalGatherPositionMetadata.attempts
               << " fl=" << optimalGatherPositionMetadata.waitFrames
               << ")";

            return os;
        }

        bool shouldResendGatherCommand(const MyWorker &worker, const OptimalGatherPositionMetadata &positionMetadata, int frameDelta = 0)
        {
            // We always attempt at least once, since we otherwise have no data to go on
            if (positionMetadata.attempts == 0) return true;

            // If we can predict the order timer value at arrival, check if it is better or worse than the observed results on this patch
            if (worker->orderProcessTimer != -1 && OrderProcessTimer::framesToNextReset() > (BWAPI::Broodwar->getLatencyFrames() + 11 + frameDelta))
            {
                int orderProcessTimerAtArrival = worker->orderProcessTimer - BWAPI::Broodwar->getLatencyFrames() - 11 - frameDelta + 1;
                while (orderProcessTimerAtArrival < 0) orderProcessTimerAtArrival += 9;

                bool result = positionMetadata.waitFrames < (positionMetadata.attempts * orderProcessTimerAtArrival);

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
            bool result = positionMetadata.waitFrames < (positionMetadata.attempts * 4);

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

            // Used to mark that the worker should have the gather command resent on this specific frame
            int resendCommandOnFrame;

            WorkerGatherStatus() : lastProcessedFrame(-2), resendCommandOnFrame(-2) {}

            void reset()
            {
                lastProcessedFrame = -2;
                positionHistory.clear();
                resentPosition = nullptr;
                resendCommandOnFrame = -2;
            }
        };

        std::map<Resource, std::map<PositionAndVelocity, OptimalGatherPositionMetadata>> resourceToOptimalGatherPositions;
        std::map<Resource, std::map<PositionAndVelocity, PositionAndVelocity>> resourceToLFBeforeOptimalGatherPositions;
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
        resourceToLFBeforeOptimalGatherPositions.clear();
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
                        if (line.size() < 6) break;

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

                            std::shared_ptr<PositionAndVelocity> lfBeforePos;
                            if (line.size() > 6)
                            {
                                if (PositionAndVelocity::isValidString(line[6]))
                                {
                                    lfBeforePos = std::make_shared<PositionAndVelocity>(PositionAndVelocity::fromString(line[6]));
                                }
                                else
                                {
                                    Log::Get() << "Invalid lf-before position string at line " << lineNumber << "; ignoring: " << line[6];
                                }
                            }

                            resourceToOptimalGatherPositions[resource].emplace(
                                    pos,
                                    OptimalGatherPositionMetadata{
                                        pos,
                                        std::stoul(line[3]),
                                        std::stoul(line[4]),
                                        std::stoul(line[5]),
                                        lfBeforePos});
                            count++;

                            if (lfBeforePos)
                            {
                                resourceToLFBeforeOptimalGatherPositions[resource].emplace(*lfBeforePos, pos);
                            }
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
                         << optimalOrderPosition.second.attempts << ";"
                         << optimalOrderPosition.second.waitFrames << ";";
                    if (optimalOrderPosition.second.lfBeforePosition)
                    {
                        file << *optimalOrderPosition.second.lfBeforePosition;
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
        auto &lfBeforeOptimalGatherPositions = resourceToLFBeforeOptimalGatherPositions[resource];
        auto &workerStatus = workerGatherStatuses[worker];

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
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            CherryVis::log(worker->id) << "targeting different patch; resending order";
            worker->gather(resourceBwapiUnit);
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
        // Note that in some cases using distance == 0 is not correct, if the worker is taking a janky path that goes parallel with the patch
        // This isn't easy to detect though with our current logic, and doesn't affect many paths or patches, so we live with it for now
        if (resource->getDistance(worker) == 0)
        {
#if !SAVED_POSITIONS_ONLY
            if (workerStatus.positionHistory.size() >= (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
                auto optimalPositionIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
                auto &optimalPosition = **optimalPositionIt;

                // Track an observation only if we didn't resend an order
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

                    optimalPositionData->second.trackObservation();

                    // Register the position of the worker at latency frames before the optimal position
                    // We use this data in the two-worker handoff case to avoid Unit_Busy
                    if (workerStatus.positionHistory.size() >= ((BWAPI::Broodwar->getLatencyFrames() * 2) + 12))
                    {
                        auto lfBeforeIt = optimalPositionIt + BWAPI::Broodwar->getLatencyFrames();
                        optimalPositionData->second.lfBeforePosition = *lfBeforeIt;
                        lfBeforeOptimalGatherPositions[**lfBeforeIt] = optimalPositionData->first;
                    }
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
                            // Find the resend position in the positions history so we can compute the delta from the optimal position
                            for (auto it = workerStatus.positionHistory.rbegin(); it != workerStatus.positionHistory.rend(); it++)
                            {
                                if ((*it) == workerStatus.resentPosition)
                                {
                                    auto delta = std::distance(it, optimalPositionIt);
                                    resendPositionData->second.trackNonoptimalResend(delta);

#if OPTIMALPOSITIONS_DEBUG
                                    CherryVis::log(resource->id)
                                            << "Tracked nonoptimal resend on position " << *workerStatus.resentPosition
                                            << "; delta: " << delta
                                            << "; frame loss: " << ((delta + 900) % 9);
                                    CherryVis::log(worker->id)
                                            << "Tracked nonoptimal resend on position " << *workerStatus.resentPosition
                                            << "; delta: " << delta
                                            << "; frame loss: " << ((delta + 900) % 9);
#endif
                                }
                            }
                        }
                    }
                }
            }
#endif

            // Only need the data until the first arrival frame
            workerStatus.reset();
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

        // Handle case where another worker is assigned to the patch
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && (currentFrame - otherWorker->lastStartedMining) < 100)
        {
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
            if (currentFrame <= commandFrameForReset && (commandFrameForTakeOver - commandFrameForReset) > 3)
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
                << "distToPatch=" << resource->getDistance(worker);
#endif

            // Logic for when the next command is in the future
            if (framesToNextCommand >= 0)
            {
                // Issue commands every 4 frames, unless we are at a position that is latency frames away from an optimal gather position
                // Sending a command at such a position will result in the optimal approach command failing with Unit_Busy
                if (framesToNextCommand % 4 == 0)
                {
                    bool sendCommand = true;
                    auto lfBeforeIt = lfBeforeOptimalGatherPositions.find(*currentPosition());
                    if (lfBeforeIt != lfBeforeOptimalGatherPositions.end())
                    {
                        auto optimalGatherPositionIt = optimalGatherPositions.find(lfBeforeIt->second);
                        if (optimalGatherPositionIt != optimalGatherPositions.end())
                        {
                            sendCommand = shouldResendGatherCommand(worker, optimalGatherPositionIt->second, BWAPI::Broodwar->getLatencyFrames());
                        }
                    }

                    if (sendCommand)
                    {
                        worker->gather(resourceBwapiUnit);
                    }
                }
                return;
            }

            // Fall through to normal optimization - if we are still approaching the patch it may send a new command to optimize mining at arrival
            // There is one specific case that is not handled - if the approach optimization tries to send a gather command exactly LF after the
            // previous one, BWAPI will give a Unit_Busy error
            // However if our optimization is working correctly, workers should never arrive too late to the patch anyway, so I'm not going to spend
            // effort on this now
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

        // Check if this worker is at an optimal position to resend the gather order
        if (!workerStatus.resentPosition)
        {
            auto here = currentPosition();
            auto optimalGatherPositionIt = optimalGatherPositions.find(*here);
            if (optimalGatherPositionIt != optimalGatherPositions.end() && shouldResendGatherCommand(worker, optimalGatherPositionIt->second))
            {
                // Check if there will be an order timer reset that affects the timing
                int framesFromCommandToReset = OrderProcessTimer::framesToNextReset() - BWAPI::Broodwar->getLatencyFrames();
                if (framesFromCommandToReset > 0 && framesFromCommandToReset < 12)
                {
                    // Send a command to take effect on the reset frame if it is coming soon
                    // Otherwise just let it take its course
                    if (framesFromCommandToReset < 5)
                    {
                        workerStatus.resendCommandOnFrame = currentFrame + framesFromCommandToReset;
                    }
                }

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
                    Log::Get() << "Failed to send gather command for " << worker->id << ": " << BWAPI::Broodwar->getLastError();
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