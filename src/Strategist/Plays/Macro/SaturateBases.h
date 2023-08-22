#pragma once

#include "Play.h"

// Ensures all of our bases are saturated with workers.
class SaturateBases : public Play
{
public:
    SaturateBases(int workerLimit = 75) : Play("SaturateBases"), workerLimit(workerLimit) {}

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    int workerLimit;
};
