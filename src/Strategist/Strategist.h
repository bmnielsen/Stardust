#pragma once

#include "Common.h"

#include "ProductionGoal.h"
#include "Play.h"
#include "StrategyEngine.h"

namespace Strategist
{
    enum class WorkerScoutStatus
    {
        Unstarted,                  // (initial state) We haven't started scouting yet
        LookingForEnemyBase,        // Our scout is trying to find the enemy base
        MovingToEnemyBase,          // Our scout knows which base is the enemy base, but hasn't reached it yet
        EnemyBaseScouted,           // Our scout has finished the first scout of the enemy base (enough to detect rushes)
        ScoutingBlocked,            // (final state) Our scout was prevented from entering the enemy base
        ScoutingFailed,             // (final state) Our scout was unable to scout the enemy base (couldn't find it or died quickly)
        ScoutingCompleted,          // (final state) Our scout scouted the enemy base and has now left or died
    };

    void update();

    void initialize();

    std::vector<ProductionGoal> &currentProductionGoals();

    std::vector<std::pair<int, int>> &currentMineralReservations();

    bool isEnemyContained();

    WorkerScoutStatus getWorkerScoutStatus();

    void setWorkerScoutStatus(WorkerScoutStatus status);

    void setOpening(std::vector<std::shared_ptr<Play>> openingPlays);

    void setStrategyEngine(std::unique_ptr<StrategyEngine> strategyEngine);
}
