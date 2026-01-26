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

            auto startTime = std::chrono::high_resolution_clock::now();
            long long lastLogOutput = 0;
            long long lastSaved = 0;

            while (!startPositions.empty())
            {
                auto &current = startPositions.front();

                auto prepareResult = simWorker->prepareGatherPath(
                        BWAPI::PrepareGatherPathOptions(current.pos, current.patch->getBWIndex(), initialState.state));
                if (!prepareResult)
                {
                    Log::Get() << "ERROR: Failed to prepare gather path for patch " << current.patch->getTilePosition();
                    return;
                }
                if (prepareResult->returnPathStartPosition != current.pos)
                {
                    Log::Get() << "ERROR: Prepared gather path has incorrect start position; patch @ " << current.patch->getTilePosition();
                    EXPECT_EQ(prepareResult->returnPathStartPosition, current.pos);
                    return;
                }

                explore(current, prepareResult);
                startPositions.pop_front();
                processed++;

                long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - startTime).count();

                // Output status every 5 seconds
                if (elapsed - lastLogOutput >= 5)
                {
                    double processedPerSecond = (double)processed / (double)elapsed;
                    std::chrono::hh_mm_ss remaining{std::chrono::seconds((int)std::round((double)startPositions.size() / processedPerSecond))};

                    Log::Get() << "Processed " << processed << " start position(s) in " << elapsed << " second(s); "
                               << startPositions.size() << " remaining (" << remaining << ")";
                    lastLogOutput = elapsed;
                }

                // Save the map data every minute
                if (elapsed - lastSaved >= 60)
                {
                    Serialization::writeMapData(mapData);
                    lastSaved = elapsed;
                }
            }

            Log::Get() << "Done; processed " << processed << " start position(s) in "
                       << std::chrono::hh_mm_ss(std::chrono::high_resolution_clock::now() - startTime);

            executed = true;
        }

    private:
        const ExploreStartPositionsModuleOptions &options;
        std::deque<StartPositionState> startPositions;
        bool executed = false;
        unsigned long processed = 0;

        void initializeStartPositions();

        void explore(StartPositionState &startPosition, std::unique_ptr<BWAPI::PrepareGatherPathResult> &preparedGatherPath);
    };
}
