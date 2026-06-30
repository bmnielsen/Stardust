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
        None, ContainStaticDefense, ContainChoke, StandGround, Flee
    };

    BWAPI::Position center;
    MyUnit vanguard;
    std::set<MyUnit> units;

    int vanguardDistToTarget;
    int vanguardDistToMain;
    double percentageToEnemyMain;

    int ballRadius;
    int lineRadius;

    int enemyAoeRadius;

    Activity currentActivity;
    SubActivity currentSubActivity;
    int lastActivityChange;

    std::deque<CombatSimResult> recentSimResults;
    std::deque<CombatSimResult> recentRegroupSimResults;

    bool isVanguardCluster;

    explicit UnitCluster(const MyUnit &unit);

    virtual ~UnitCluster() = default;

    UnitCluster (const UnitCluster&) = delete;
    UnitCluster &operator=(const UnitCluster&) = delete;

    void absorbCluster(const std::shared_ptr<UnitCluster> &other, BWAPI::Position targetPosition);

    void addUnit(const MyUnit &unit);

    std::set<MyUnit>::iterator removeUnit(std::set<MyUnit>::iterator unitIt, BWAPI::Position targetPosition);

    [[nodiscard]] bool hasUnitType(BWAPI::UnitType type) const;

    void updatePositions(BWAPI::Position targetPosition);

    void setActivity(Activity newActivity, SubActivity newSubActivity = SubActivity::None);

    void setSubActivity(SubActivity newSubActivity);

    [[nodiscard]] std::string getCurrentActivity() const;

    [[nodiscard]] std::string getCurrentSubActivity() const;

    virtual void move(BWAPI::Position targetPosition);

    virtual void regroup(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                         std::set<Unit> &enemyUnits,
                         std::set<MyUnit> &detectors,
                         const CombatSimResult &simResult,
                         BWAPI::Position targetPosition,
                         bool hasValidTarget);

    std::vector<std::pair<MyUnit, Unit>>
    selectTargets(std::set<Unit> &targetUnits, BWAPI::Position targetPosition, bool staticPosition = false);

    virtual void attack(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, BWAPI::Position targetPosition);

    void containStatic(std::set<Unit> &enemyUnits, BWAPI::Position targetPosition);

    void holdChoke(Choke *choke,
                   BWAPI::Position defendEnd,
                   std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets);

    void standGround(std::set<Unit> &enemyUnits, BWAPI::Position targetPosition);

    void flee(std::set<Unit> &enemyUnits);

    bool moveAsBall(BWAPI::Position targetPosition, std::set<MyUnit> &ballUnits) const;

    bool formArc(BWAPI::Position pivot, int desiredDistance);

    CombatSimResult runCombatSim(BWAPI::Position targetPosition,
                                 std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                                 std::set<Unit> &targets,
                                 std::set<MyUnit> &detectors,
                                 bool attacking = true,
                                 Choke *choke = nullptr);

    CombatSimResult& runCombatSim(std::deque<CombatSimResult> &simResults,
                                  BWAPI::Position targetPosition,
                                  std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                                  std::set<Unit> &targets,
                                  std::set<MyUnit> &detectors,
                                  bool attacking = true,
                                  Choke *choke = nullptr);

    void addInstrumentation(nlohmann::json &clusterArray) const;

    // This returns the number of consecutive frames the sim has agreed on its current value.
    // It also returns the total number of attack and regroup frames within the window.
    static int consecutiveSimResults(std::deque<CombatSimResult> &simResults,
                                     int *attack,
                                     int *regroup,
                                     int limit);

protected:
    int area;
};
