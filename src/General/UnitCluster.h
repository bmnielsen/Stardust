#pragma once

#include "Common.h"
#include "Unit.h"

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
    virtual void execute(std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);

protected:
    static std::shared_ptr<Unit> ChooseMeleeTarget(BWAPI::Unit attacker, std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);
    static std::shared_ptr<Unit> ChooseRangedTarget(BWAPI::Unit attacker, std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);
};
