#include "MineralLockingOptimization.h"

namespace MineralLockingOptimization
{
    void initialize() {}

    void gameEnd() {}

    void update() {}

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit) return;

        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            worker->gather(resourceBwapiUnit);
        }
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource) {}
}
