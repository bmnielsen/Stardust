#include "UnitProductionGoal.h"

#include "Workers.h"
#include "Units.h"

int UnitProductionGoal::countToProduce()
{
    if (count == -1 && type.isWorker())
    {
        // Hard cap of 75 workers
        if (Units::countAll(type) >= 75) return 0;

        // Currently just produces until there are no available resources at owned bases
        // TODO: decide whether this goal or something else should make expansion decisions
        return std::max(0, Workers::availableMineralAssignments() - Units::countIncomplete(type));
    }

    if (count == -1) return -1;

    return std::max(0, count - Units::countAll(type));
}
