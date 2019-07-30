#pragma once

#include "Play.h"

// Ensures all of our bases are saturated with workers.
class SaturateBases : public Play
{
public:
    const char *label() const { return "SaturateBases"; }

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);
};
