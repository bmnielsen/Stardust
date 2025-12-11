#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"

namespace MineralLockingOptimization
{
    void initialize();

    void gameEnd();

    void update();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);
}
