#pragma once

#include "Play.h"

/* Ensures all of our bases are saturated with workers. */
class SaturateBases : public PlayItem
{
public:
    explicit SaturateBases(int workerLimit = 75) : PlayItem("SaturateBases"), workerLimit(workerLimit), workersPerPatch(2) {}

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    void setWorkersPerPatch(int _workersPerPatch) { workersPerPatch = _workersPerPatch; }

private:
    int workerLimit;
    int workersPerPatch;
};
