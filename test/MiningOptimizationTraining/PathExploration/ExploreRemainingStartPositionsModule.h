#pragma once

#include <BWAPI.h>

#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    struct PathToExplore
    {
        PathToExplore(BWAPI::Unit patch, BWAPI::ExactPosition startPosition, Path<ReturnArrivalData> &path)
                : patch(patch)
                , startPosition(startPosition)
                , path(path)
        {}

        BWAPI::Unit patch;
        BWAPI::ExactPosition startPosition;
        Path<ReturnArrivalData> &path;
    };

    struct ExploreRemainingStartPositionsModuleOptions : PathExplorationModuleOptions
    {
        BWAPI::TilePosition oneBase = BWAPI::TilePositions::Invalid;
        BWAPI::TilePosition onePatch = BWAPI::TilePositions::Invalid;
    };

    // Module that explores each start position left to explore in the map data
    class ExploreRemainingStartPositionsModule : public PathExplorationModule
    {
    public:
        explicit ExploreRemainingStartPositionsModule(const ExploreRemainingStartPositionsModuleOptions &options)
                : PathExplorationModule(options)
                , options(options)
        {}

    protected:

        bool initialize() override;
        void run() override;

    private:
        const ExploreRemainingStartPositionsModuleOptions &options;
        std::deque<PathToExplore> pathsToExplore;
        bool executed = false;

        void explore(PathToExplore &pathToExplore);
    };
}
