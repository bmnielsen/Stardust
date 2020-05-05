#pragma once

#include "TakeExpansion.h"

// Takes the next expansion.
class TakeIslandExpansion : public TakeExpansion
{
public:
    explicit TakeIslandExpansion(BWAPI::TilePosition depotPosition);

    [[nodiscard]] const char *label() const override { return "TakeIslandExpansion"; }

    void update() override;

    void disband(const std::function<void(const MyUnit&)> &removedUnitCallback,
                 const std::function<void(const MyUnit&)> &movableUnitCallback) override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void addUnit(MyUnit unit) override;

    void removeUnit(MyUnit unit) override;

private:
    MyUnit shuttle;
};
