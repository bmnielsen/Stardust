#pragma once

#include "MyUnit.h"

class MyDragoon : public MyUnitImpl
{
public:
    explicit MyDragoon(BWAPI::Unit unit);

    [[nodiscard]] int getLastAttackStartedAt() const { return lastAttackStartedAt; }

    [[nodiscard]] int getPotentiallyStuckSince() const { return potentiallyStuckSince; }

private:
    BWAPI::Position lastPosition;
    int lastAttackStartedAt;
    int potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck

    void typeSpecificUpdate() override;

    [[nodiscard]] bool isReady() const override;

    bool unstick() override;

    void attackUnit(const Unit &target, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets) override;
};