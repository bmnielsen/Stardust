#pragma once

#include "Play.h"

// Builds a depot at the natural expansion.
class TakeNaturalExpansion : public Play
{
public:
    TakeNaturalExpansion();

    const char *label() const { return "TakeNaturalExpansion"; }

    void update();

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

private:
    BWAPI::TilePosition depotPosition;
};
