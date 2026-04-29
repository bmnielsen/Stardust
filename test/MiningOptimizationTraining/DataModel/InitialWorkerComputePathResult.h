#pragma once

#include <BWAPI/ExactPosition.h>
#include <optional>

namespace MiningOptimizationTraining
{
    struct InitialWorkerComputePathResult
    {
        int actionFrame;
        bool actionAtArrival;
        int postActionDelay;
        BWAPI::ExactPosition nextPathStartPosition;
        std::optional<int> orderProcessTimerResetValue;
    };
}
