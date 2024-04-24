#pragma once

#include "Common.h"
#include "MyUnit.h"
#include "Resource.h"

namespace WorkerOrderTimer
{
    void initialize();

    void write();

    void update();

    // Resends gather orders to optimize the worker order timer. Returns whether an order was sent to the worker.
    bool optimizeStartOfMining(const MyUnit &worker, const Resource &resource);

    // Resends return or gather orders to optimize delivery of resources at the depot. Returns whether an order was sent to the worker.
    bool optimizeReturn(const MyUnit &worker, const Resource &resource, const Unit &depot);

    void writeInstrumentation();
}
