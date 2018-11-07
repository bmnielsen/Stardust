#pragma once

#include "Common.h"

namespace UnitUtil
{
    bool IsUndetected(BWAPI::Unit unit);

    BWAPI::Position PredictPosition(BWAPI::Unit unit, int frames);
}