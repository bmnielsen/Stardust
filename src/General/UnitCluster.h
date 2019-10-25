#pragma once

#include "Common.h"
#include "Unit.h"
#include "CombatSimResult.h"

class UnitCluster
{
public:
    BWAPI::Position center;
    BWAPI::Unit vanguard;
    std::set<BWAPI::Unit> units;

    explicit UnitCluster(BWAPI::Unit unit)
            : center(unit->getPosition())
            , vanguard(unit)
    {
        units.insert(unit);
    }

    virtual ~UnitCluster() = default;

    void addUnit(BWAPI::Unit unit);

    void removeUnit(BWAPI::Unit unit);

    std::set<BWAPI::Unit>::iterator removeUnit(std::set<BWAPI::Unit>::iterator unitIt);

    void updatePositions(BWAPI::Position targetPosition);

    std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>>
    selectTargets(std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);

    virtual void execute(std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>> &unitsAndTargets, BWAPI::Position targetPosition);

    CombatSimResult
    runCombatSim(std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>> &unitsAndTargets, std::set<std::shared_ptr<Unit>> &targets);

protected:
    static std::shared_ptr<Unit> ChooseMeleeTarget(BWAPI::Unit attacker, std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);

    static std::shared_ptr<Unit> ChooseRangedTarget(BWAPI::Unit attacker, std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);
};
