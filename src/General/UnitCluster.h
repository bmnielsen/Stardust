#pragma once

#include "Common.h"
#include "MyUnit.h"
#include "CombatSimResult.h"

class UnitCluster
{
public:
    enum Activity
    {
        Default, Attacking, Regrouping //, Exploding, Flanking, PerformingRunBy
    };

    BWAPI::Position center;
    MyUnit vanguard;
    std::set<MyUnit> units;

    Activity currentActivity;
    int lastActivityChange;

    explicit UnitCluster(const MyUnit &unit)
            : center(unit->lastPosition)
            , vanguard(unit)
            , currentActivity(Activity::Default)
            , lastActivityChange(0)
            , area(unit->type.width() * unit->type.height())
    {
        units.insert(unit);
    }

    virtual ~UnitCluster() = default;

    void addUnit(const MyUnit &unit);

    void removeUnit(const MyUnit &unit);

    std::set<MyUnit>::iterator removeUnit(std::set<MyUnit>::iterator unitIt);

    void updatePositions(BWAPI::Position targetPosition);

    void setActivity(Activity newActivity);

    virtual void move(BWAPI::Position targetPosition);

    virtual void regroup(BWAPI::Position regroupPosition);

    std::vector<std::pair<MyUnit, Unit>>
    selectTargets(std::set<Unit> &targets, BWAPI::Position targetPosition);

    virtual void attack(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, BWAPI::Position targetPosition);

    CombatSimResult
    runCombatSim(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, std::set<Unit> &targets);

protected:
    static Unit ChooseMeleeTarget(const MyUnit &attacker, std::set<Unit> &targets, BWAPI::Position targetPosition);

    static Unit ChooseRangedTarget(const MyUnit &attacker, std::set<Unit> &targets, BWAPI::Position targetPosition);

    int area;
};
