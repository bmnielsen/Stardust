#pragma once

#include <BWAPI.h>
#include <BWAPI/PrepareGatherPathOptions.h>
#include <BWAPI/PrepareGatherPathResult.h>

#include "PathExplorationModule.h"
#include "MiningOptimizationTraining/DataModel/Serialization.h"

#include <chrono>

namespace MiningOptimizationTraining
{
    struct StartPositionStateBase
    {
        BWAPI::ExactPosition pos;
        BWAPI::Unit patch = nullptr;
    };

    struct InitializeStartPosition : StartPositionStateBase {};
    struct ExploreStartPosition : StartPositionStateBase {};
    struct SimulateSpecificPath : StartPositionStateBase {};
    struct SimulateAllSubpixelsOfPosition : StartPositionStateBase {};
    struct ExploreInitialWorkerStartPosition : StartPositionStateBase {};

    struct ExploreStartPositionsModuleOptions : PathExplorationModuleOptions
    {
        BWAPI::TilePosition oneBase = BWAPI::TilePositions::Invalid;
        BWAPI::TilePosition onePatch = BWAPI::TilePositions::Invalid;
    };

    // Module that explores start positions, with how they are handled implemented in the template class
    template <typename StartPositionState>
    class ExploreStartPositionsModule : public PathExplorationModule
    {
    public:
        explicit ExploreStartPositionsModule(const ExploreStartPositionsModuleOptions &options)
                : PathExplorationModule(options)
                , options(options)
        {}

    protected:

        bool initialize() override
        {
            if (executed) return false;

            initializeStartPositions();

            Log::Get() << "Initialized; " << startPositions.size() << " start position(s) to explore";

            return true;
        }

        void run() override
        {
            if (executed) return;
            executed = true;

            auto initialCount = startPositions.size();
            auto startTime = std::chrono::high_resolution_clock::now();
            long long lastLogOutput = 0;
            long long lastSaved = 0;

            while (!startPositions.empty())
            {
                auto &current = startPositions.front();
                explore(current);
                startPositions.pop_front();
                processed++;

                long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - startTime).count();

                // Output status every 5 seconds
                if (elapsed - lastLogOutput >= 5)
                {
                    auto throughput = (double)processed / (double)elapsed;
                    auto burndown = (double)((long long)initialCount - (long long)startPositions.size()) / (double)elapsed;
                    std::string timeRemaining;
                    if (burndown < 0.0001)
                    {
                        timeRemaining = "N/A";
                    }
                    else
                    {
                        auto secondsLeft = (int)std::round((double)startPositions.size() / burndown);
                        if (secondsLeft >= 24 * 60 * 60)
                        {
                            timeRemaining = ">24hr";
                        }
                        else
                        {
                            timeRemaining = ((std::ostringstream() << std::chrono::hh_mm_ss{std::chrono::seconds(secondsLeft)}).str());
                        }
                    }

                    Log::Get() << std::fixed << std::setprecision(2)
                               << "Processed " << processed << " start position(s); "
                               << startPositions.size() << " remaining; "
                               << "throughput " << throughput << " pos/s; "
                               << timeRemaining << " remaining";

                    lastLogOutput = elapsed;
                }

                // Save the map data every five minutes
                if (elapsed - lastSaved >= 300)
                {
                    Serialization::writeMapData(mapData);
                    lastSaved = elapsed;
                }
            }

            long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - startTime).count();

            Log::Get() << "Done; processed " << processed << " start position(s) in " << elapsed << " second(s)";

            executed = true;
        }

        std::unique_ptr<BWAPI::PrepareGatherPathResult> prepareReturnPath(const StartPositionState &startPosition, const BWAPI::StateCopy &stateCopy)
        {
            auto prepareResult = simWorker->prepareGatherPath(
                    BWAPI::PrepareGatherPathOptions(startPosition.pos, stateCopy.state).prepareReturnFrom(startPosition.patch->getBWIndex()));
            if (!prepareResult)
            {
                Log::Get() << "ERROR: Failed to prepare gather path for patch " << startPosition.patch->getTilePosition()
                           << "; start position " << startPosition.pos
                           << "; state copy " << stateCopy.label;
                return nullptr;
            }
            if (prepareResult->returnPathStartPosition != startPosition.pos)
            {
                Log::Get() << "ERROR: Prepared gather path has incorrect start position; patch @ " << startPosition.patch->getTilePosition()
                           << "; start position " << startPosition.pos
                           << "; state copy " << stateCopy.label;
                return nullptr;
            }

            return prepareResult;
        }

    private:
        const ExploreStartPositionsModuleOptions &options;
        std::deque<StartPositionState> startPositions;
        bool executed = false;
        unsigned long processed = 0;

        void initializeStartPositions();

        void explore(StartPositionState &startPosition);
    };
}
