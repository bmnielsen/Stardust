#include "ContinuousWorkerProduction.h"

#include "Workers.h"
#include "Units.h"

bool ContinuousWorkerProduction::isFulfilled()
{
    return countToProduce() <= 0;
}

int ContinuousWorkerProduction::countToProduce()
{
    // Hard cap of 75 workers
    if (Units::countAll(unitType()) >= 75) return 0;

    // Currently just produces until there are no available resources at owned bases
    // TODO: decide whether this goal or something else should make expansion decisions
    return Workers::availableMineralAssignments() - Units::countIncomplete(unitType());
}
