#pragma once

#include "Play.h"
#include "MyWorker.h"
#include "Squads/AttackBaseSquad.h"

/* Takes the next expansion. */
class TakeExpansion : public PlayItem
{
public:
    Base *baseLoc;
    int enemyValue;

    TakeExpansion(Base *baseLoc, int enemyValue);

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    virtual bool cancellable();

    BWAPI::TilePosition depotPosition;

protected:
    OwnedWorker builder;
    bool buildCannon;
    std::shared_ptr<AttackBaseSquad> squad;
};
