#pragma once

#include "Common.h"
#include "Base.h"
#include "MyUnit.h"

namespace Workers
{
    void initialize();

    void onUnitDestroy(const Unit &unit);

    void onUnitDestroy(BWAPI::Unit unit);

    void updateAssignments();

    void issueOrders();

    // Whether the given worker unit can currently be reassigned to non-gathering duties
    bool isAvailableForReassignment(const MyUnit &unit, bool allowCarryMinerals);

    MyUnit getClosestReassignableWorker(BWAPI::Position position, bool allowCarryMinerals, int *bestTravelTime = nullptr);

    std::vector<MyUnit> getBaseWorkers(Base *base);

    int baseMineralWorkerCount(Base *base);

    void reserveBaseWorkers(std::vector<MyUnit> &workers, Base *base);

    void reserveWorker(const MyUnit &unit);

    void releaseWorker(const MyUnit &unit);

    // How many mineral patches are currently available for assignment
    int availableMineralAssignments(Base *base = nullptr);

    void addDesiredGasWorkers(int gasWorkers);

    int desiredGasWorkers();

    int mineralWorkers();

    std::pair<int, int> gasWorkers();
}
