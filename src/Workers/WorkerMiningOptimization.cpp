#include "WorkerMiningOptimization.h"

#include "Workers.h"
#include "WorkerOrderTimer.h"

#define USE_OLD_LOGIC true

namespace WorkerMiningOptimization
{
    void initialize()
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::initialize();
        return;
#endif

        // TODO
    }

    void write()
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::write();
        return;
#endif

        // TODO
    }

    // Optimizes the start of mining, returning whether an order was sent to the worker.
    void optimizeStartOfMining(const MyWorker &worker, const Resource &resource)
    {
#if USE_OLD_LOGIC
        WorkerOrderTimer::optimizeStartOfMining(worker, resource);
        return;
#endif

        // TODO

//        auto bwapiUnit = resource->getBwapiUnitIfVisible();

        // If another worker is currently mining this patch, try to time it so this worker takes over at the optimal frame
        MyWorker otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && otherWorker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
        {
            // Timing for mining:
            // - Mining timer starts at 75 frames (https://github.com/OpenBW/openbw/blob/d5fe2306ecb08efdea877a7f4117b178292137cb/bwgame.h#L4380)
            // - Worker receives minerals the frame after both the mining timer and worker order timer reach 0
            // - Another worker can start mining on the next frame
            //
            // If there has been no order timer reset, the worker will receive the minerals 5 frames after the mining timer reaches 0
            //
            // If there has been an order timer reset, the worst-case is the largest of 8 frames after the mining timer reaches 0 or 8 frames after
            // the order timer reset occurred

            // Normally the worker completes mining 7 frames after the mining timer
//            int framesToWorstCaseCompletion = otherWorker->bwapiUnit->getOrderTimer() + 7;
//
//            int framesSinceLastOrderTimerReset = (currentFrame - 8) % 150;
        }
    }

    // Optimizes returning a resource, returning whether an order was sent to the worker.
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot)
    {
        // TODO
    }
}