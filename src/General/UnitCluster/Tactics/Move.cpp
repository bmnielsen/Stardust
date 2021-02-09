#include "UnitCluster.h"

#include "Map.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

void UnitCluster::move(BWAPI::Position targetPosition)
{
    // Hook to allow the map-specific override to perform the move
    if (Map::mapSpecificOverride()->clusterMove(*this, targetPosition)) return;

    // Determine if the cluster should flock
    // Criteria:
    // - Must be the vanguard cluster
    // - No units may be in a leaf area or narrow choke (as this makes it likely that they will get stuck on buildings or terrain)
    bool shouldFlock = isVanguardCluster;
    if (shouldFlock)
    {
        for (const auto &unit : units)
        {
            auto pos = unit->getTilePosition();
            if (Map::isInNarrowChoke(pos) || Map::isInLeafArea(pos))
            {
                shouldFlock = false;
                break;
            }
        }
    }

    if (shouldFlock && moveAsBall(targetPosition)) return;

    for (const auto &unit : units)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Move to target: Moving to " << BWAPI::WalkPosition(targetPosition);
#endif
        unit->moveTo(targetPosition);
    }
}
