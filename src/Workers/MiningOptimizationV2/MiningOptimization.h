#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"

namespace MiningOptimization
{
    // Loads the data files needed to optimize for a new game
    void initialize();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);
}
