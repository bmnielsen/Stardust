#pragma once

#include <vector>
#include "ExactPosition.h"

namespace BWAPI
{
    struct SimulateGatherPathResult
    {
        std::vector<ExactPosition> positions;
        ExactPosition actionPosition;
        ExactPosition nextPathStartPosition;
        uint64_t squaredSpeedEightFramesAlongNextPath;
    };
}
