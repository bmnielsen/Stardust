#pragma once

#include "Play.h"

class ScoutEnemyExpos : public Play
{
public:
    ScoutEnemyExpos()
            : Play("ScoutEnemyExpos")
            , scout(nullptr)
            , targetBase(nullptr)
            , usingSearchPath(true) {}

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

    void addUnit(const MyUnit &unit) override;

    void removeUnit(const MyUnit &unit) override;

private:
    MyUnit scout;
    Base *targetBase;
    bool usingSearchPath;
};
