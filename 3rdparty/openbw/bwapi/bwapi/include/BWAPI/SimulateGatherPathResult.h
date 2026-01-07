#pragma once

#include "ExactPosition.h"

#include <vector>

// Forwards
namespace bwgame
{
    struct state;
}

namespace BWAPI
{
    // The result of running a gather path simulation
    struct SimulateGatherPathResult
    {
        SimulateGatherPathResult(std::vector<ExactPosition> positions,
                                 ExactPosition actionPosition,
                                 ExactPosition nextPathStartPosition,
                                 uint64_t squaredSpeedEightFramesAlongNextPath,
                                 std::unique_ptr<bwgame::state> stateAtStartOfNextPath);
        ~SimulateGatherPathResult();

        // The positions encountered from the last resend (or start of the path if no resends were sent) to the arrival at the target
        std::vector<ExactPosition> positions;

        // The position of the worker when the action was performed (start of gather or delivery of resources)
        ExactPosition actionPosition;

        // The position of the worker at the start of its next path
        ExactPosition nextPathStartPosition;

        // The squared speed of the worker eight frames along its next path
        // If there was a collision, this is set to 0 even if the actual speed is slightly higher
        uint64_t squaredSpeedEightFramesAlongNextPath;

        // A pointer to the BW state at the start of the next path, returned only if requested in the options
        std::unique_ptr<bwgame::state> stateAtStartOfNextPath;
    };
}
