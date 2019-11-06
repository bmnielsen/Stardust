#pragma once

#include "Play.h"

// Takes the next expansion.
class TakeIslandExpansion : public Play
{
public:
    explicit TakeIslandExpansion(BWAPI::TilePosition depotPosition);

    const char *label() const override { return "TakeIslandExpansion"; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void addUnit(BWAPI::Unit unit) override;

    void removeUnit(BWAPI::Unit unit) override;

private:
    BWAPI::TilePosition depotPosition;
    BWAPI::Unit shuttle;
    BWAPI::Unit builder;
};
