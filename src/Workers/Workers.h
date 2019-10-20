#pragma once

#include "Common.h"

namespace Workers
{
    void onUnitDestroy(BWAPI::Unit unit);

    void onUnitRenegade(BWAPI::Unit unit);

    void updateAssignments();

    void issueOrders();

    // Whether the given worker unit can currently be reassigned to build something
    bool isAvailableForReassignment(BWAPI::Unit unit);

    void reserveWorker(BWAPI::Unit unit);

    void releaseWorker(BWAPI::Unit unit);

    // How many mineral patches are currently available for assignment
    int availableMineralAssignments();

    int availableGasAssignments();

    void setDesiredGasWorkers(int gasWorkers);

    int desiredGasWorkers();

    int mineralWorkers();

    int gasWorkers();
}
