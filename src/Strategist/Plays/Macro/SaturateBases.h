#pragma once

#include "Play.h"

// Ensures all of our bases are saturated with workers.
class SaturateBases : public Play
{
public:
    [[nodiscard]] const char *label() const override { return "SaturateBases"; }

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;
};
