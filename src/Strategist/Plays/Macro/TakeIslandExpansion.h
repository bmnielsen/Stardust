#pragma once

#include "TakeExpansion.h"
#include "MyWorker.h"

/* Takes the next expansion. */
class TakeIslandExpansion : public TakeExpansion
{
public:
    explicit TakeIslandExpansion(Base *baseLoc, bool canCancel = true, bool transferWorkers = true);

    void update() override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    void addUnit(const OwnedUnit &gameUnit) override;

    void removeUnit(const OwnedUnit &gameUnit) override;

    bool cancellable() override;

    int framesToClearBlocker();

private:
    bool canCancel;
    OwnedUnit shuttle;
    int workerTransferState; // 0 = picking up workers, 1 = unloading workers, 2 = done
    std::vector<OwnedWorker> workerTransfer;
};
