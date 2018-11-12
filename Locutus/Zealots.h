#pragma once

#include "Opening.h"

class Zealots : public Opening
{
public:
    std::vector<ProductionGoal> & goals() { return _goals; };

private:
    std::vector<ProductionGoal> _goals = {
        ProductionGoal(ProductionGoal::Type::ContinuousProduction, BWAPI::UnitTypes::Protoss_Probe, 75),
        ProductionGoal(ProductionGoal::Type::ContinuousProduction, BWAPI::UnitTypes::Protoss_Zealot)
    };
};
