#pragma once

#include "Play.h"
#include "Squads/DefendBaseSquad.h"
#include "Squads/WorkerDefenseSquad.h"

class DefendBase : public Play
{
public:
    Base *base;
    int enemyValue;

    explicit DefendBase(Base *base, int enemyValue);

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

protected:
    std::shared_ptr<DefendBaseSquad> squad;
    std::shared_ptr<WorkerDefenseSquad> workerDefenseSquad;

    BWAPI::TilePosition pylonLocation;
    std::deque<BWAPI::TilePosition> cannonLocations;

    MyUnit pylon;
    std::vector<MyUnit> cannons;

    int desiredCannons();
};
