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
    for (auto unit : units)
    {
        auto &myUnit = Units::getMine(unit);

        // If the unit is stuck, unstick it
        if (myUnit.isStuck())
        {
            myUnit.stop();
            continue;
        }

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit.isReady()) continue;

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Regroup: Moving to " << BWAPI::WalkPosition(regroupPosition);
#endif
        myUnit.moveTo(regroupPosition);
    }
}
