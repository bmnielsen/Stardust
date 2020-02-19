#include "UnitCluster.h"

#include "Units.h"

/*
 * Cluster regrouping
 *
 * Currently just moves towards a regroup position computed elsewhere.
 *
 * TODO: Compute the regroup position in this file
 * TODO: Potentially split up in all directions to draw enemy away
 * TODO: Consider how a regrouping cluster should move (does it make sense to use flocking?)
 */

void UnitCluster::regroup(BWAPI::Position regroupPosition)
{
    for (const auto &unit : units)
    {
        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit->id) << "Regroup: Moving to " << BWAPI::WalkPosition(regroupPosition);
#endif
        unit->moveTo(regroupPosition);
    }
}
