#pragma once

#include "Common.h"
#include "Base.h"
#include "MyWorker.h"

namespace Workers
{
    void initialize();

    void onUnitDestroy(const Unit &unit);

    void onMineralPatchDestroyed(const Resource &mineralPatch);

    void updateAssignments();

    void issueOrders();

    // Whether the given worker unit can currently be reassigned to non-gathering duties
    bool isAvailableForReassignment(const MyWorker &unit, bool allowCarryMinerals);

    MyWorker getClosestReassignableWorker(BWAPI::Position position, bool allowCarryMinerals, int *bestTravelTime = nullptr);

    size_t getBaseWorkerCount(Base *base);

    std::vector<MyWorker> getBaseWorkers(Base *base);

    int baseMineralWorkerCount(Base *base);

    void reserveBaseWorkers(std::vector<MyWorker> &workers, Base *base);

    void reserveWorker(const MyWorker &unit);

    void releaseWorker(const MyWorker &unit);

    // How many mineral patches are currently available for assignment
    int availableMineralAssignments(Base *base = nullptr, int workersPerPatch = 2);

    // How many gas slots are currently available for assignment
    int availableGasAssignments(Base *base = nullptr);

    void setDesiredGasWorkerDelta(int gasWorkerDelta);

    int mineralWorkers();

    std::pair<int, int> gasWorkers();

    int reassignableMineralWorkers();

    int reassignableGasWorkers();

    int idleWorkerCount();

    std::map<Resource, std::set<MyWorker>> &mineralsAndAssignedWorkers();

    MyWorker getOtherWorkerMining(const Resource &resource, const MyWorker &worker);
}
