#pragma once

#include "Common.h"
#include "Resource.h"
#include "MyWorker.h"

namespace WorkerMiningInstrumentation
{
    void initialize(const std::function<std::map<Resource, std::set<MyWorker>> &()> &getMineralsAndAssignedWorkersOverride = nullptr);

    void update();

    void writeInstrumentation();

    std::pair<double, double> getEfficiency(int fromFrame = -1, int toFrame = -1);
}
