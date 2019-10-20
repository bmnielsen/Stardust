#pragma once

#include "Play.h"

// Builds a depot at the natural expansion.
class TakeNaturalExpansion : public Play
{
public:
    TakeNaturalExpansion();

    const char *label() const override { return "TakeNaturalExpansion"; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    BWAPI::TilePosition depotPosition;
};
