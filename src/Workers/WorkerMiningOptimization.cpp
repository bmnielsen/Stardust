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

namespace WorkerMiningOptimization
{
    namespace
    {
        struct OptimalGatherPositionMetadata
        {
        public:
            unsigned long observations = 0;
            unsigned long optimal = 0;
            unsigned long nonoptimal = 0;
            unsigned long frameLosses = 0;

            void trackOptimalObservation()
            {
                if (atObservationCap()) return;

                observations++;
                optimal++;
            }

            void trackNonoptimalObservation(size_t delta)
            {
                if (atObservationCap()) return;

                observations++;
                nonoptimal++;

                // Frame losses depend on where the wrong position was in the history
                // Sending the command too late results in a loss equal to the number of frames late
                // Sending the command early causes an extra order timer cycle
                frameLosses += (delta + 900) % 9;
            }

        private:
            [[nodiscard]] bool atObservationCap() const
            {
                // Set a cap on how many observations we track to reduce file size and ensure we don't exceed the size of unsigned long
                // Beyond a certain point additional observations are not going to have any impact anyway
                return observations >= 9999;
            }
        };

        struct WorkerGatherStatus
        {
            // The last frame this worker was optimized
            int lastProcessedFrame;

            // The recent positions this worker has visited while on its way to gather
            std::vector<std::shared_ptr<PositionAndVelocity>> positionHistory;

            // If a gather command fails, we want to resend it on the next frame
            int lastGatherCommandFailure;

            // TODO: Metadata needed to determine optimal gather resend frame when a reset is going to happen

            WorkerGatherStatus() : lastProcessedFrame(-2), lastGatherCommandFailure(-2) {}

            void reset()
            {
                lastProcessedFrame = -2;
                positionHistory.clear();
                lastGatherCommandFailure = -2;
            }
        };

        std::map<Resource, std::map<PositionAndVelocity, OptimalGatherPositionMetadata>> resourceToOptimalGatherPositions;
        std::map<MyWorker, WorkerGatherStatus> workerGatherStatuses;

        std::string gatherPathsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "gatherpositions_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "json", writing);
        }
    }

    void initialize()
    {
        resourceToOptimalGatherPositions.clear();
        workerGatherStatuses.clear();

        {
            std::ifstream file;
            file.open(gatherPathsFilename());
            if (file.good())
            {
                try
                {
                    // Read and parse each position
                    while (true)
                    {
                        auto line = CsvTools::readNextLine(file);
                        if (line.size() != 7) break;

                        BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                        auto resource = Units::resourceAt(tile);
                        if (resource)
                        {
                            resourceToOptimalGatherPositions[resource].emplace(
                                    PositionAndVelocity::fromString(line[2]),
                                    OptimalGatherPositionMetadata{
                                        std::stoul(line[3]),
                                        std::stoul(line[4]),
                                        std::stoul(line[5]),
                                        std::stoul(line[6])});
                        }
                    }

                    Log::Get() << "Read optimal gather positions from " << gatherPathsFilename();
                }
                catch (std::exception &ex)
                {
                    Log::Get() << "Exception caught attempting to read optimal order positions: " << ex.what();
                }
            }
        }
    }

    void write()
    {
        {
            std::ofstream file;
            file.open(gatherPathsFilename(true), std::ofstream::trunc);

            for (auto &resourceAndOptimalGatherPositions : resourceToOptimalGatherPositions)
            {
                for (auto &optimalOrderPosition : resourceAndOptimalGatherPositions.second)
                {
                    file << resourceAndOptimalGatherPositions.first->tile.x << ","
                         << resourceAndOptimalGatherPositions.first->tile.y << ","
                         << optimalOrderPosition.first << ","
                         << optimalOrderPosition.second.observations << ","
                         << optimalOrderPosition.second.optimal << ","
                         << optimalOrderPosition.second.nonoptimal << ","
                         << optimalOrderPosition.second.frameLosses << "\n";
                }
            }

            file.close();
            Log::Get() << "Wrote optimal gather positions to " << gatherPathsFilename(true);
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
        int dist = resource->getDistance(worker);
        if (dist > 0)
        {
            workerStatus.positionHistory.emplace_back(currentPosition());
        }
        else if (workerStatus.positionHistory.size() >= (BWAPI::Broodwar->getLatencyFrames() + 11))
        {
            auto optimalPositionIt = workerStatus.positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
            auto &optimalPosition = **optimalPositionIt;

            // Register the optimal observation
            auto optimalPositionData = optimalGatherPositions.find(optimalPosition);
            if (optimalPositionData == optimalGatherPositions.end())
            {
                optimalPositionData = optimalGatherPositions.emplace(optimalPosition, OptimalGatherPositionMetadata{}).first;

#if OPTIMALPOSITIONS_DEBUG
                CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(resource->center)
                                 << ": Registered new optimal position " << optimalPosition;
                CherryVis::log(resource->id) << "Registered new optimal position " << optimalPosition;
#endif
            }

            optimalPositionData->second.trackOptimalObservation();

            // Register a nonoptimal observation on any other matched positions in the path
            for (auto it = workerStatus.positionHistory.rbegin(); it != workerStatus.positionHistory.rend(); it++)
            {
                if (it == optimalPositionIt) continue;

                auto optimalPositionMetadataIt = optimalGatherPositions.find(**it);
                if (optimalPositionMetadataIt != optimalGatherPositions.end())
                {
                    auto delta = std::distance(it, optimalPositionIt);
                    optimalPositionMetadataIt->second.trackNonoptimalObservation(delta);

#if OPTIMALPOSITIONS_DEBUG
                    CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(resource->center)
                                     << ": Found a previously registered optimal position in the worker path at delta " << delta
                                     << "; frame loss: " << ((delta + 900) % 9)
                                     << "; optimal this trip: " << optimalPosition
                                     << "; previously registered: " << **it;
                    CherryVis::log(resource->id)
                            << "Found a previously registered optimal position in the worker path at delta " << delta
                            << "; frame loss: " << ((delta + 900) % 9)
                            << "; optimal this trip: " << optimalPosition
                            << "; previously registered: " << **it;
#endif
                }
            }

            // Only need the data until the first arrival frame
            workerStatus.reset();
        }

        // Handle case where another worker is assigned to the patch
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && (currentFrame - otherWorker->lastStartedMining) < 100)
        {
            // Compute the optimal frame to take over from the other worker

            // We need to add an extra frame if the worker taking over has its orders processed first
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
                // Issue commands every 4 frames
                if (framesToNextCommand % 4 == 0)
                {
                    worker->gather(resourceBwapiUnit);
                }
                return;
            }

            // Fall through to normal optimization - if we are still approaching the patch it may send a new command to optimize mining at arrival
            // There is one specific case that is not handled - if the approach optimization tries to send a gather command exactly LF after the
            // previous one, BWAPI will give a Unit_Busy error
            // However if our optimization is working correctly, workers should never arrive too late to the patch anyway, so I'm not going to spend
            // effort on this now
        }

        // Resend the gather command if it failed to send last frame
        if (workerStatus.lastGatherCommandFailure == (currentFrame - 1))
        {
            worker->gather(resourceBwapiUnit);

#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(worker->id) << "Resending gather command as it failed to send last frame";
            CherryVis::log(resource->id) << "Resending gather command as it failed to send last frame";
#endif
            return;
        }

        // Check if this worker is at an optimal position to resend the gather order
        auto here = currentPosition();
        auto optimalGatherPositionIt = optimalGatherPositions.find(*here);
        if (optimalGatherPositionIt != optimalGatherPositions.end()
            && optimalGatherPositionIt->second.optimal > optimalGatherPositionIt->second.frameLosses)
        {
            if (!worker->gather(resourceBwapiUnit))
            {
                workerStatus.lastGatherCommandFailure = currentFrame;

#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "Failed to send gather command: " << BWAPI::Broodwar->getLastError();
                CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
            }
#if OPTIMALPOSITIONS_DEBUG
            else
            {
                CherryVis::log(worker->id) << "Resending gather command; at optimal position " << *here;
                CherryVis::log(resource->id) << "Resending gather command; at optimal position " << *here;
            }
#endif
        }
#if OPTIMALPOSITIONS_DEBUG
        else if (optimalGatherPositionIt != optimalGatherPositions.end())
        {
            CherryVis::log(worker->id) << "Not resending gather command; at optimal position " << *here
                << " but losses " << optimalGatherPositionIt->second.frameLosses
                << " exceed optimal observations " << optimalGatherPositionIt->second.optimal;
            CherryVis::log(resource->id) << "Not resending gather command; at optimal position " << *here
                                         << " but losses " << optimalGatherPositionIt->second.frameLosses
                                         << " exceed optimal observations " << optimalGatherPositionIt->second.optimal;
        }
#endif
    }

    // Optimizes returning a resource, returning whether an order was sent to the worker.
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot)
    {
        // TODO
    }
}