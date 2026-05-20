#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "MiningOptimizationV2/DataModel/MapData.h"

#include <optional>
#include "Base.h"

namespace WorkerOrderTimer
{
    void initialize();

    void gameEnd();

    void update();

    // Optimizes the start of mining
    bool optimizeStartOfMining(Base *base, std::vector<std::tuple<MyWorker, MyUnit, Resource>> &workersAndDepotsAndResources);
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Performs the initial worker split, assigning each initial worker to a patch
    std::map<MyWorker, std::tuple<Resource, Resource, std::optional<MiningOptimization::InitialSplitData>>> initialWorkerSplit();

    // Gets the average rotation time for a single worker gathering from the resource
    std::optional<int> averageRotationTimeFor(const Resource &resource);
}
