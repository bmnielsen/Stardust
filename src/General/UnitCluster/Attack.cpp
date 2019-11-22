#include "UnitCluster.h"

#include "Units.h"

/*
 * Cluster attack
 *
 * This is a "normal" attack; special maneuvers like flanking will be done elsewhere.
 *
 * Currently just uses myUnit.attackUnit.
 *
 * TODO: Use boids for unit positioning
 * - Kiting
 * - Surrounds
 * - Advancing the front line if they are blocking the second line from firing
 * - Retreating individual hurt units
 */

void UnitCluster::attack(std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>> &unitsAndTargets, BWAPI::Position targetPosition)
{
    // Micro each unit
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = Units::getMine(unitAndTarget.first);

        // If the unit is stuck, unstick it
        if (myUnit.isStuck())
        {
            myUnit.stop();
            continue;
        }

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit.isReady()) continue;

        // Attack target
        if (unitAndTarget.second)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first) << "Target: " << unitAndTarget.second->type << " @ "
                                                << BWAPI::WalkPosition(unitAndTarget.second->lastPosition);
#endif
            myUnit.attackUnit(unitAndTarget.second->unit);
        }
        else
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first) << "No target: move to " << BWAPI::WalkPosition(targetPosition);
#endif
            myUnit.moveTo(targetPosition);
        }
    }
}