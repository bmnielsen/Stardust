#pragma once

#include "Play.h"
#include "Squads/DefendBaseSquad.h"

class DefendBase : public Play
{
public:
    Base *base;

    explicit DefendBase(Base *base);

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

protected:
    std::shared_ptr<DefendBaseSquad> squad;

    BWAPI::TilePosition pylonLocation;
    std::deque<BWAPI::TilePosition> cannonLocations;

    MyUnit pylon;
    std::vector<MyUnit> cannons;

    std::vector<Unit> enemyThreats;

    int desiredCannons();
};
