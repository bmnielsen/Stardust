#pragma once

#include "ExactPosition.h"

// Forwards
namespace bwgame
{
    struct state;
}

namespace BWAPI
{
    // The result of preparing a gather path
    struct PrepareGatherPathResult
    {
        PrepareGatherPathResult(int returnPathStartFrame,
                                ExactPosition returnPathStartPosition,
                                std::unique_ptr<bwgame::state> returnPathState);
        ~PrepareGatherPathResult();

        // The frame of the state copy at the start of the return path
        int returnPathStartFrame;

        // The start position of the return path, expected to be the same as the position given in the options
        ExactPosition returnPathStartPosition;

        // A pointer to the BW state at the start of the return path
        std::unique_ptr<bwgame::state> returnPathState;
    };
}
