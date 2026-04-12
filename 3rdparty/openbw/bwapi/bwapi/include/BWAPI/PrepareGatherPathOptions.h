#pragma once

#include "ExactPosition.h"

// Forwards
namespace bwgame
{
    struct state;
}

namespace BWAPI
{
    struct PrepareGatherPathOptions
    {
        PrepareGatherPathOptions(ExactPosition startPosition);
        PrepareGatherPathOptions(ExactPosition startPosition, const std::unique_ptr<bwgame::state> &startingState);
        ~PrepareGatherPathOptions();

        // Prepares a return path by having the worker gather at the given patch
        PrepareGatherPathOptions &prepareReturnFrom(size_t _patchUnitIndex);

        ExactPosition startPosition;
        const std::unique_ptr<bwgame::state> &startingState;
        bool prepareReturn = false;
        size_t patchUnitIndex = 0;
    };
}
