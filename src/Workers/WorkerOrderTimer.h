#pragma once

#include "Common.h"
#include "MyUnit.h"

namespace WorkerOrderTimer
{
    void initialize();

    void write();

    void update();

    // Resends gather orders to optimize the worker order timer. Returns whether an order was sent to the worker.
    bool optimizeMineralWorker(const MyUnit &worker, BWAPI::Unit resource);
}
