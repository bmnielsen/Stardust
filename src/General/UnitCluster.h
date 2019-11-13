#pragma once

#include "Common.h"
#include "Unit.h"
#include "CombatSimResult.h"

class UnitCluster
{
public:
    enum Activity
    {
        Default, Attacking, Regrouping //, Exploding, Flanking, PerformingRunBy
    };

    BWAPI::Position center;
    BWAPI::Unit vanguard;
    std::set<BWAPI::Unit> units;

    Activity currentActivity;
    int lastActivityChange;

    explicit UnitCluster(BWAPI::Unit unit)
            : center(unit->getPosition())
            , vanguard(unit)
            , currentActivity(Activity::Default)
            , lastActivityChange(0)
            , area(unit->getType().width() * unit->getType().height())
    {
        units.insert(unit);
    }

    virtual ~UnitCluster() = default;

    void addUnit(BWAPI::Unit unit);

    void removeUnit(BWAPI::Unit unit);

    std::set<BWAPI::Unit>::iterator removeUnit(std::set<BWAPI::Unit>::iterator unitIt);

    void updatePositions(BWAPI::Position targetPosition);

    void setActivity(Activity newActivity);

    virtual void move(BWAPI::Position targetPosition);

    virtual void regroup(BWAPI::Position regroupPosition);

    std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>>
    selectTargets(std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);

    virtual void attack(std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>> &unitsAndTargets, BWAPI::Position targetPosition);

    CombatSimResult
    runCombatSim(std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>> &unitsAndTargets, std::set<std::shared_ptr<Unit>> &targets);

protected:
    static std::shared_ptr<Unit> ChooseMeleeTarget(BWAPI::Unit attacker, std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);

    static std::shared_ptr<Unit> ChooseRangedTarget(BWAPI::Unit attacker, std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition);

    int area;
};
