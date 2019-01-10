#pragma once

#include "ProductionGoal.h"

class ContinuousWorkerProduction : public UnitProductionGoal
{
public:
    BWAPI::UnitType unitType() { return BWAPI::Broodwar->self()->getRace().getWorker(); }
    bool isFulfilled();
    int countToProduce();
    int producerLimit() { return 1; }
};
