#pragma once

#include "Common.h"
#include "Resource.h"
#include "MyWorker.h"

namespace WorkerMiningInstrumentation
{
    void initialize(const std::function<std::map<Resource, std::set<MyWorker>> &()> &getMineralsAndAssignedWorkersOverride = nullptr);

    void update();

    void writeInstrumentation();

    std::map<Resource, std::pair<double, double>> getEfficiencyByPatch(int fromFrame = -1, int toFrame = -1);

    std::pair<double, double> getEfficiency(int fromFrame = -1, int toFrame = -1);
}
