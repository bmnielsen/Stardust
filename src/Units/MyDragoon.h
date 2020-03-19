#pragma once

#include "MyUnit.h"

class MyDragoon : public MyUnitImpl
{
public:
    explicit MyDragoon(BWAPI::Unit unit);

    void update(BWAPI::Unit unit) override;

    [[nodiscard]] int getLastAttackStartedAt() const { return lastAttackStartedAt; }

    [[nodiscard]] int getNextAttackPredictedAt() const { return nextAttackPredictedAt; }

    [[nodiscard]] int getPotentiallyStuckSince() const { return potentiallyStuckSince; }

private:
    int lastAttackStartedAt;
    int nextAttackPredictedAt;
    int potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck

    [[nodiscard]] bool isReady() const override;

    bool unstick() override;

    void attackUnit(const Unit &target, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets) override;
};