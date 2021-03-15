#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

// Takes the next expansion.
class TakeExpansion : public Play
{
public:
    Base *base;
    int enemyValue;

    TakeExpansion(Base *base, int enemyValue);

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

    virtual bool cancellable();

    BWAPI::TilePosition depotPosition;

protected:
    MyUnit builder;
    bool buildCannon;
    std::shared_ptr<AttackBaseSquad> squad;
};
