#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "MiningOptimizationV2/DataModel/MapData.h"

namespace WorkerOrderTimer
{
    void initialize();

    void gameEnd();

    void update();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Performs the initial worker split, assigning each initial worker to a patch
    std::map<MyWorker, std::tuple<Resource, Resource, std::optional<MiningOptimization::InitialSplitData>>> initialWorkerSplit();
}
