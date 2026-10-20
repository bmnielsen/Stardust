#pragma once

#include "Common.h"
#include "Base.h"
#include "MyWorker.h"

namespace WorkerMgr
{
    void initialize();

    void onUnitDestroy(const Unit &gameUnit);

    void onMineralPatchDestroyed(const Resource &mineralPatch);

    void updateAssignments();

    void issueOrders();

    /* Whether the given worker unit can currently be reassigned to non-gathering duties */
    bool isAvailableForReassignment(const OwnedWorker &gameUnit, bool allowCarryMinerals, bool allowMining);

    OwnedWorker getClosestReassignableWorker(BWAPI::Position position, bool allowCarryMinerals, int *bestTravelTime = nullptr);

    size_t getBaseWorkerCount(Base *baseLoc);

    std::vector<OwnedWorker> getBaseWorkers(Base *baseLoc);

    int baseMineralWorkerCount(Base *baseLoc);

    void reserveBaseWorkers(std::vector<OwnedWorker> &workers, Base *baseLoc);

    void reserveWorker(const OwnedWorker &gameUnit);

    void releaseWorker(const OwnedWorker &gameUnit);

    /* How many mineral patches are currently available for assignment */
    int availableMineralAssignments(Base *baseLoc = nullptr, int workersPerPatch = 2);

    /* How many gas slots are currently available for assignment */
    int availableGasAssignments(Base *baseLoc = nullptr);

    void setDesiredGasWorkerDelta(int gasWorkerDelta);

    int mineralWorkers();

    std::pair<int, int> gasWorkers();

    int reassignableMineralWorkers();

    int reassignableGasWorkers();

    int idleWorkerCount();

    std::map<Resource, std::set<OwnedWorker>> &mineralsAndAssignedWorkers();

    /* Gets all workers assigned to the resource. Does not check if any have died since their status was last updated. */
    std::set<OwnedWorker> getWorkersAssignedTo(const Resource &resource);

    /* Given a patch and a worker, returns the other worker assigned to it, if it exists. */
    OwnedWorker getOtherWorkerMining(const Resource &resource, const OwnedWorker &wrk);

    void setWorkerMineralPatch(const OwnedWorker &wrk, const Resource &resource, Base *baseLoc);
}
