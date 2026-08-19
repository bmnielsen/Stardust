#pragma once

#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"

namespace MiningOptimizationTraining
{
    void runInitializeMapData(BWTest &test, PathExplorationModuleOptions options);

    void runExplorePaths(BWTest &test, PathExplorationModuleOptions options);

    void runInitializeOrderProcessTimerResetValues(const Maps::MapMetadata &map, bool skipIfMapDataExists = false);

    void runInitialSplitExploration(const Maps::MapMetadata &map);

    void transform(const std::string &mapHash);

    void exportDataFile(const std::string &openbwMapHash, const std::string &mapHash);
}
