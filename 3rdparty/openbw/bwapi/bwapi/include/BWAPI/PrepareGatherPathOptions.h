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
        PrepareGatherPathOptions(ExactPosition startPosition, size_t patchUnitIndex);
        PrepareGatherPathOptions(ExactPosition startPosition, size_t patchUnitIndex, const std::unique_ptr<bwgame::state> &startingState);
        ~PrepareGatherPathOptions();

        ExactPosition startPosition;
        size_t patchUnitIndex;
        const std::unique_ptr<bwgame::state> &startingState;
    };
}
