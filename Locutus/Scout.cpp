#include "Scout.h"

#include "Map.h"

namespace Scout
{
#ifndef _DEBUG
    namespace
    {
#endif
        ScoutingMode scoutingMode;
        BWAPI::Unit  workerScout;
#ifndef _DEBUG
    }
#endif

    void update()
    {
        // If we have scouted the enemy location, stop scouting
        if (scoutingMode == ScoutingMode::Location)
        {

        }


        // Assign a scout if we need one



    }

    void setScoutingMode(ScoutingMode mode)
    {
        scoutingMode = mode;
    }
}