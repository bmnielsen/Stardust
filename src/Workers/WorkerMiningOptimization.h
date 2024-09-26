#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"

namespace WorkerMiningOptimization
{
    void initialize();

    void flushObservations();

    void write();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);
}
