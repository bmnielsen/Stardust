#pragma once

#include "Opening.h"

#include "ProductionGoal.h"

class DarkTemplar : public Opening
{
public:
    std::vector<ProductionGoal *> & goals() { return _goals; };

private:
    std::vector<ProductionGoal *> _goals = {
        (ProductionGoal*)new UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe),
        (ProductionGoal*)new UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dark_Templar),
        (ProductionGoal*)new UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot)
    };
};
