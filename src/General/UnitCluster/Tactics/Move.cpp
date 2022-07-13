#include "UnitCluster.h"

#include "Map.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    void normalMove(std::set<MyUnit> &units, BWAPI::Position targetPosition)
    {
        for (const auto &unit : units)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Move to target: Moving to " << BWAPI::WalkPosition(targetPosition);
#endif
            unit->moveTo(targetPosition);
        }
    }
}

void UnitCluster::move(BWAPI::Position targetPosition)
{
    // Hook to allow the map-specific override to perform the move
    if (Map::mapSpecificOverride()->clusterMove(*this, targetPosition)) return;
    if (!vanguard) return;

    auto isMovable = [](const MyUnit &unit)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) return false;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) return false;

        // Cannons don't move
        if (unit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) return false;

        return true;
    };

    auto inLeafOrNarrowArea = [](const MyUnit &unit)
    {
        auto tile = unit->getTilePosition();
        return Map::isInNarrowChoke(tile)
            || Map::isInLeafArea(tile)
            || Map::walkableWidth(tile.x, tile.y) < 4;
    };

    // Never try to flock if:
    // - This is not the vanguard cluster
    // - There is only one unit
    // - The cluster is all air units (vanguard will only be air if this is the case)
    // - The vanguard is in a leaf or narrow area
    if (!isVanguardCluster || units.size() == 1 || vanguard->isFlying || inLeafOrNarrowArea(vanguard))
    {
        std::set<MyUnit> movableUnits;
        for (const auto &unit : units)
        {
            if (isMovable(unit)) movableUnits.insert(unit);
        }
        normalMove(movableUnits, targetPosition);
        return;
    }

    // Split the units into those that can flock and those that can't
    std::set<MyUnit> flockUnits;
    std::set<MyUnit> noFlockUnits;
    for (const auto &unit : units)
    {
        if (!isMovable(unit)) continue;

        // Flying units and units in leaf or narrow areas don't flock
        if (unit->isFlying || inLeafOrNarrowArea(unit))
        {
            noFlockUnits.insert(unit);
            continue;
        }

        // Otherwise flock unless the path to the vanguard unit goes through a narrow choke
        if (PathFinding::SeparatingNarrowChoke(unit->lastPosition,
                                               vanguard->lastPosition,
                                               unit->type,
                                               PathFinding::PathFindingOptions::UseNeighbouringBWEMArea))
        {
            noFlockUnits.insert(unit);
        }
        else
        {
            flockUnits.insert(unit);
        }
    }

    normalMove(noFlockUnits, targetPosition);
    if (!moveAsBall(targetPosition, flockUnits))
    {
        normalMove(flockUnits, targetPosition);
    }
}
