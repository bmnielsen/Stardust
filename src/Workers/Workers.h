#pragma once

#include "Common.h"
#include "Base.h"

namespace Workers
{
    void onUnitDestroy(BWAPI::Unit unit);

    void onUnitRenegade(BWAPI::Unit unit);

    void updateAssignments();

    void issueOrders();

    // Whether the given worker unit can currently be reassigned to non-gathering duties
    bool isAvailableForReassignment(BWAPI::Unit unit, bool allowCarryMinerals);

    BWAPI::Unit getClosestReassignableWorker(BWAPI::Position position, bool allowCarryMinerals, int *bestTravelTime = nullptr);

    void reserveWorker(BWAPI::Unit unit);

    void releaseWorker(BWAPI::Unit unit);

    // How many mineral patches are currently available for assignment
    int availableMineralAssignments(Base *base = nullptr);

    int availableGasAssignments();

    void setDesiredGasWorkers(int gasWorkers);

    int desiredGasWorkers();

    int mineralWorkers();

    int gasWorkers();
}
