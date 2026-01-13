#pragma once

#include "ExactPosition.h"

namespace BWAPI
{
    struct PrepareGatherPathOptions
    {
        ExactPosition startPosition;
        size_t patchUnitIndex;
    };
}
