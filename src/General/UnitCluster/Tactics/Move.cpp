#include "UnitCluster.h"

#include "Map.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    void normalMove(std::set<OwnedUnit> &units, BWAPI::Position targetPosition)
    {
        for (const auto &gameUnit : units)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(gameUnit->ident) << "Move to target: Moving to " << BWAPI::WalkPosition(targetPosition);
#endif
            gameUnit->moveTo(targetPosition);
        }
    }
}

void UnitCluster::move(BWAPI::Position targetPosition)
{
    /* Hook to allow the map-specific override to perform the move */
    if (Map::mapSpecificOverride()->clusterMove(*this, targetPosition)) return;
    if (!vanguard) return;

    auto isMovable = [](const OwnedUnit &gameUnit)
    {
        /* If the unit is stuck, unstick it */
        if (gameUnit->unstick()) return false;

        /* If the unit is not ready (i.e. is already in the middle of an attack), don't touch it */
        if (!gameUnit->isReady()) return false;

        /* Cannons don't move */
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) return false;

        return true;
    };

    auto inLeafOrNarrowArea = [](const OwnedUnit &gameUnit)
    {
        auto tilePos = gameUnit->getTilePosition();
        return Map::isInNarrowChoke(tilePos)
            || Map::isInLeafArea(tilePos)
            || Map::walkableWidth(tilePos.x, tilePos.y) < 4;
    };

    /* Never try to flock if: */
    /* - This is not the vanguard cluster */
    /* - There is only one unit */
    /* - The cluster is all air units (vanguard will only be air if this is the case) */
    /* - The vanguard is in a leaf or narrow area */
    if (!isVanguardCluster || units.size() == 1 || vanguard->isFlying || inLeafOrNarrowArea(vanguard))
    {
        std::set<OwnedUnit> movableUnits;
        for (const auto &gameUnit : units)
        {
            if (isMovable(gameUnit)) movableUnits.insert(gameUnit);
        }
        normalMove(movableUnits, targetPosition);
        return;
    }

    /* Split the units into those that can flock and those that can't */
    std::set<OwnedUnit> flockUnits;
    std::set<OwnedUnit> noFlockUnits;
    for (const auto &gameUnit : units)
    {
        if (!isMovable(gameUnit)) continue;

        /* Flying units and units in leaf or narrow areas don't flock */
        if (gameUnit->isFlying || inLeafOrNarrowArea(gameUnit))
        {
            noFlockUnits.insert(gameUnit);
            continue;
        }

        /* Otherwise flock unless the path to the vanguard unit goes through a narrow choke */
        if (Pathfinder::SeparatingNarrowChoke(gameUnit->prevPos,
                                               vanguard->prevPos,
                                               gameUnit->type,
                                               Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea))
        {
            noFlockUnits.insert(gameUnit);
        }
        else
        {
            flockUnits.insert(gameUnit);
        }
    }

    normalMove(noFlockUnits, targetPosition);
    if (!moveAsBall(targetPosition, flockUnits))
    {
        normalMove(flockUnits, targetPosition);
    }
}
