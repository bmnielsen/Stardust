#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "DataModel/MapData.h"

#include <optional>

namespace MiningOptimization
{
    // Called on game start; loads the required data files and initializes data structures
    void initialize();

    // Called on each frame; performs bookkeeping on completed optimizations
    // This is called after Workers has processed all of the workers and possible called one of the optimization methods.
    void update();

    // Called on game end; writes any relevant instrumentation data
    void gameEnd();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Performs the initial worker split, assigning each initial worker to a patch
    std::map<MyWorker, std::tuple<Resource, Resource, std::optional<InitialSplitData>>> initialWorkerSplit();

    // Gets the average rotation time for a single worker gathering from the resource
    std::optional<int> averageRotationTimeFor(const Resource &resource);
}
