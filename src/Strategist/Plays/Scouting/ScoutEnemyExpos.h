#pragma once

#include "Play.h"

class ScoutEnemyExpos : public PlayItem
{
public:
    ScoutEnemyExpos()
            : PlayItem("ScoutEnemyExpos")
            , scout(nullptr)
            , targetBase(nullptr)
            , usingSearchPath(true)
            , workerScoutHasDied(false)
            {}

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    void addUnit(const OwnedUnit &gameUnit) override;

    void removeUnit(const OwnedUnit &gameUnit) override;

private:
    OwnedUnit scout;
    Base *targetBase;
    bool usingSearchPath;
    bool workerScoutHasDied;
};
