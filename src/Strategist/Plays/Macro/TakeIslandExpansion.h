#pragma once

#include "Play.h"

// Takes the next expansion.
class TakeIslandExpansion : public Play
{
public:
    explicit TakeIslandExpansion(BWAPI::TilePosition depotPosition);

    [[nodiscard]] const char *label() const override { return "TakeIslandExpansion"; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void addUnit(MyUnit unit) override;

    void removeUnit(MyUnit unit) override;

private:
    BWAPI::TilePosition depotPosition;
    MyUnit shuttle;
    MyUnit builder;
};
