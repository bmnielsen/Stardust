#pragma once

#include "Common.h"
#include "MyUnit.h"
#include "CombatSimResult.h"

class UnitCluster
{
public:
    // Remember to update name vectors in UnitCluster.cpp when changing these
    enum class Activity
    {
        Moving, Attacking, Regrouping //, Exploding, Flanking, PerformingRunBy
    };
    enum class SubActivity
    {
        None, ContainStaticDefense, ContainChoke, Flee
    };

    BWAPI::Position center;
    MyUnit vanguard;
    std::set<MyUnit> units;

    Activity currentActivity;
    SubActivity currentSubActivity;
    int lastActivityChange;

    std::deque<std::pair<CombatSimResult, bool>> recentSimResults;
    std::deque<std::pair<CombatSimResult, bool>> recentRegroupSimResults;

    explicit UnitCluster(const MyUnit &unit);

    virtual ~UnitCluster() = default;

    void addUnit(const MyUnit &unit);

    void removeUnit(const MyUnit &unit);

    std::set<MyUnit>::iterator removeUnit(std::set<MyUnit>::iterator unitIt);

    void updatePositions(BWAPI::Position targetPosition);

    void setActivity(Activity newActivity, SubActivity newSubActivity = SubActivity::None);

    void setSubActivity(SubActivity newSubActivity);

    virtual void move(BWAPI::Position targetPosition);

    virtual void regroup(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                         std::set<Unit> &enemyUnits,
                         const CombatSimResult &simResult,
                         BWAPI::Position targetPosition);

    std::vector<std::pair<MyUnit, Unit>>
    selectTargets(std::set<Unit> &targets, BWAPI::Position targetPosition);

    virtual void attack(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, BWAPI::Position targetPosition);

    void containBase(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                     std::set<Unit> &enemyUnits,
                     BWAPI::Position targetPosition);

    void holdChoke(Choke *choke,
                   std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                   BWAPI::Position targetPosition);

    CombatSimResult
    runCombatSim(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, std::set<Unit> &targets, bool attacking = true, Choke *choke = nullptr);

    void addSimResult(CombatSimResult &simResult, bool attack);

    void addRegroupSimResult(CombatSimResult &simResult, bool contain);

protected:
    static Unit ChooseMeleeTarget(const MyUnit &attacker, std::set<Unit> &targets, BWAPI::Position targetPosition);

    static Unit ChooseRangedTarget(const MyUnit &attacker, std::set<Unit> &targets, BWAPI::Position targetPosition);

    int area;
};
