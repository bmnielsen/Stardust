#pragma once

#include "TakeExpansion.h"

// Takes the next expansion.
class TakeIslandExpansion : public TakeExpansion
{
public:
    explicit TakeIslandExpansion(Base *base);

    void update() override;

    void disband(const std::function<void(const MyUnit&)> &removedUnitCallback,
                 const std::function<void(const MyUnit&)> &movableUnitCallback) override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void addUnit(const MyUnit &unit) override;

    void removeUnit(const MyUnit &unit) override;

    bool cancellable();

private:
    MyUnit shuttle;
    int workerTransferState; // 0 = picking up workers, 1 = unloading workers, 2 = done
    std::vector<MyUnit> workerTransfer;
};
