#pragma once

#include "Common.h"

class UnitCluster
{
public:
    BWAPI::Position center;
    BWAPI::Unit vanguard;
    std::set<BWAPI::Unit> units;

    UnitCluster(BWAPI::Unit unit)
        : center(unit->getPosition())
        , vanguard(unit)
    {
        units.insert(unit);
    }

    void addUnit(BWAPI::Unit unit);
    void removeUnit(BWAPI::Unit unit);
    void update(BWAPI::Position targetPosition);
};
