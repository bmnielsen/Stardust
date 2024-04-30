#pragma once

#include "Play.h"

// Ensures all of our bases are saturated with workers.
class SaturateBases : public Play
{
public:
    explicit SaturateBases(int workerLimit = 75) : Play("SaturateBases"), workerLimit(workerLimit), workersPerPatch(2) {}

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void setWorkersPerPatch(int _workersPerPatch) { workersPerPatch = _workersPerPatch; }

private:
    int workerLimit;
    int workersPerPatch;
};
