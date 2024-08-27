#include "WorkerMiningOptimization.h"

#include "Workers.h"
#include "OrderProcessTimer.h"
#include "PositionAndVelocity.h"
#include "FileTools.h"
#include "CsvTools.h"
#include "Units.h"

#define USE_OLD_LOGIC false

#if USE_OLD_LOGIC
#include "WorkerOrderTimer.h"
#endif

#if INSTRUMENTATION_ENABLED
#define TAKEOVER_DEBUG false
#define OPTIMALPOSITIONS_DEBUG true
#endif

// How many positions to compare before a position to determine path stability
#define LOOKBACK 10

namespace WorkerMiningOptimization
{
    namespace
    {
        std::map<Resource, std::set<std::pair<PositionAndVelocity, uint32_t>>> resourceToOptimalGatherPositions;
        std::map<MyWorker, std::vector<std::pair<int, PositionAndVelocity>>> workerPositionHistory;

        std::string gatherPathsFilename(bool writing = false)
        {
            return FileTools::getFilePath(
                    (std::ostringstream() << "gatherpositions_" << BWAPI::Broodwar->mapHash() << "_lf" << BWAPI::Broodwar->getLatencyFrames()).str(),
                    "json",
                    writing);
        }

    }

    void initialize()
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::initialize();
        return;
#endif

        resourceToOptimalGatherPositions.clear();
        workerPositionHistory.clear();

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
                        if (line.size() != 4) break;

                        BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                        auto resource = Units::resourceAt(tile);
                        if (resource)
                        {
                            resourceToOptimalGatherPositions[resource].emplace(
                                    PositionAndVelocity::fromString(line[2]),
                                    (uint32_t)std::stoi(line[3]));
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
#if USE_OLD_LOGIC
        WorkerOrderTimer::write();
        return;
#endif

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
                         << optimalOrderPosition.second << "\n";
                }
            }

            file.close();
            Log::Get() << "Wrote optimal gather positions to " << gatherPathsFilename(true);
        }
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const Resource &resource)
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::optimizeStartOfMining(worker, resource);
        return;
#endif

        auto &optimalGatherPositions = resourceToOptimalGatherPositions[resource];
        auto &positionHistory = workerPositionHistory[worker];
        auto positionHashBefore = [&positionHistory](const auto &positionIt)
        {
            if (positionIt == positionHistory.rend()) return std::make_pair((uint32_t)0, false);

            uint32_t hash = 5;
            for (auto it = (positionIt + 1); it != (positionIt + 6); it++)
            {
                if (it == positionHistory.rend()) return std::make_pair((uint32_t)0, false);
                it->second.addToHash(hash);
            }
            return std::make_pair(hash, true);
        };

        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            positionHistory.clear();
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
            positionHistory.clear();
            return;
        }

        // Clear position history if the last position was not on the previous frame
        if (!positionHistory.empty() && positionHistory.rbegin()->first != (currentFrame - 1))
        {
            positionHistory.clear();
        }

        // Record optimal position if we have just arrived at the patch
        int dist = resource->getDistance(worker);
        if (dist == 0 && positionHistory.size() >= (BWAPI::Broodwar->getLatencyFrames() + 11 + LOOKBACK))
        {
            auto optimalPosition = positionHistory.rbegin() + BWAPI::Broodwar->getLatencyFrames() + 10;
            auto hash = positionHashBefore(optimalPosition).first;

#if OPTIMALPOSITIONS_DEBUG
            auto result = optimalGatherPositions.emplace(optimalPosition->second, hash);
            if (result.second)
            {
                CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(resource->center)
                                 << ": Registered optimal position " << optimalPosition->second << ":" << hash;
                CherryVis::log(resource->id) << "Registered optimal position " << optimalPosition->second << ":" << hash;
            }

            for (auto it = positionHistory.rbegin(); it != positionHistory.rend(); it++)
            {
                if (it == optimalPosition) continue;

                auto hashHere = positionHashBefore(it);
                if (!hashHere.second) break;

                if (optimalGatherPositions.contains(std::make_pair(it->second, hashHere.first)))
                {
                    auto delta = std::distance(it, optimalPosition);
                    CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(resource->center)
                                     << ": Found a previously registered optimal position in the worker path at delta " << delta
                                     << "; optimal this trip: " << optimalPosition->second << ":" << hash
                                     << "; previously registered: " << it->second << ":" << hashHere.first;
                    CherryVis::log(resource->id)
                            << "Found a previously registered optimal position in the worker path at delta " << delta
                            << "; optimal this trip: " << optimalPosition->second << ":" << hash
                            << "; previously registered: " << it->second << ":" << hashHere.first;
                }
            }
#else
            optimalGatherPositions.emplace(optimalPosition->second, hash);
#endif
        }
        else if (dist > 0)
        {
            positionHistory.emplace_back(currentFrame, worker);
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

        // Check if this worker is at an optimal position to resend the gather order
        PositionAndVelocity currentPositionAndVelocity(worker);
        auto hash = positionHashBefore(positionHistory.rbegin());
        if (hash.second && optimalGatherPositions.contains(std::make_pair(currentPositionAndVelocity, hash.first)))
        {
            worker->gather(resourceBwapiUnit);
#if OPTIMALPOSITIONS_DEBUG
            CherryVis::log(worker->id) << "Resending gather command; at optimal position " << currentPositionAndVelocity << ":" << hash.first;
            CherryVis::log(resource->id) << "Resending gather command; at optimal position " << currentPositionAndVelocity << ":" << hash.first;
#endif
        }
    }

    // Optimizes returning a resource, returning whether an order was sent to the worker.
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot)
    {
        // TODO
    }
}