#pragma once

#include "Common.h"

namespace Scout
{
    enum ScoutingMode
    {
        None, Location, Full
    };

    void update();

    void setScoutingMode(ScoutingMode scoutingMode);
}
