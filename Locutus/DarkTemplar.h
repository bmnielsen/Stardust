#pragma once

#include "Opening.h"

#include "ContinuousWorkerProduction.h"
#include "ContinuousProduction.h"

class DarkTemplar : public Opening
{
public:
    std::vector<ProductionGoal *> & goals() { return _goals; };

private:
    std::vector<ProductionGoal *> _goals = {
        new ContinuousWorkerProduction(),
        new ContinuousProduction(BWAPI::UnitTypes::Protoss_Dark_Templar),
        new ContinuousProduction(BWAPI::UnitTypes::Protoss_Zealot)
    };
};
