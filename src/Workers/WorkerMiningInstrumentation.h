#pragma once

#include "Common.h"
#include "Resource.h"
#include "MyWorker.h"

namespace WorkerMiningInstrumentation
{
    void initialize(const std::function<std::map<Resource, std::set<MyWorker>> &()> &getMineralsAndAssignedWorkersOverride = nullptr);

    void update();

    void writeInstrumentation();

    double getEfficiency();
}
