#include "UnitCluster.h"

#include "Units.h"
#include "Map.h"

void UnitCluster::attack(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, BWAPI::Position targetPosition)
{
    // If this map has chokes that may need to be cleared, check if this cluster needs to do so to reach its target
    if (Map::mapSpecificOverride()->hasAttackClearableChokes())
    {
        // We don't bother if any enemy unit is already in range of a unit in the cluster
        bool anyUnitInRange = false;
        for (const auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.second && unitAndTarget.first->isInOurWeaponRange(unitAndTarget.second))
            {
                anyUnitInRange = true;
                break;
            }
        }

        if (!anyUnitInRange && Map::mapSpecificOverride()->clusterMove(*this, targetPosition))
        {
            return;
        }
    }

    // Micro each unit
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;

        // If the unit is stuck, unstick it
        if (myUnit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit->isReady()) continue;

        // Attack target
        if (unitAndTarget.second)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "Target: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->lastPosition);
#endif
            myUnit->attackUnit(unitAndTarget.second, unitsAndTargets);
        }
        else
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "No target: move to " << BWAPI::WalkPosition(targetPosition);
#endif
            myUnit->moveTo(targetPosition);
        }
    }
}
