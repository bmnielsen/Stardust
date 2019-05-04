#pragma once

#include "ProductionGoal.h"

class ContinuousProduction : public UnitProductionGoal
{
public:
    ContinuousProduction(BWAPI::UnitType type) : type(type) {}

    BWAPI::UnitType unitType() { return type; }
    bool isFulfilled() { return false; }
    int countToProduce() { return -1; }
    int producerLimit() { return -1; }

private:
    BWAPI::UnitType type;
};
