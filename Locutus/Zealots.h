#pragma once

#include "Opening.h"

#include "ProductionGoal.h"

class Zealots : public Opening
{
public:
    std::vector<ProductionGoal *> & goals() { return _goals; };

private:
    std::vector<ProductionGoal *> _goals = {
        (ProductionGoal*)new UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe),
        (ProductionGoal*)new UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot)
    };
};
