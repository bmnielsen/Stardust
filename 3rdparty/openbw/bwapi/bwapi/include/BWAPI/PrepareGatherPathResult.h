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
        PrepareGatherPathResult(int startFrame,
                                ExactPosition startPosition,
                                std::unique_ptr<bwgame::state> state);
        ~PrepareGatherPathResult();

        // The frame of the state copy at the start of the return path
        int startFrame;

        // The start position of the return path, expected to be the same as the position given in the options
        ExactPosition startPosition;

        // A pointer to the BW state at the start of the return path
        std::unique_ptr<bwgame::state> state;
    };
}
