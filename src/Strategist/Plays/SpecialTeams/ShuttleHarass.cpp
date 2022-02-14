#include "ShuttleHarass.h"

#include "Map.h"
#include "Players.h"
#include "Units.h"

namespace
{
    void executeShuttle(MyUnit shuttle)
    {
    }
}

void ShuttleHarass::update()
{
    // Always request shuttles so all unassigned shuttles get assigned to this play
    // This play is always at lowest priority with respect to other plays utilizing shuttles
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Shuttle, Map::getMyMain()->getPosition());

    // Micro each shuttle individually
    for (auto &shuttle : shuttles)
    {
        executeShuttle(shuttle);
    }
}
