#pragma once

#include "MyUnit.h"

class MyDragoon : public MyUnitImpl
{
public:
    explicit MyDragoon(BWAPI::Unit unit);

    [[nodiscard]] int getLastAttackStartedAt() const { return lastAttackStartedAt; }

private:
    BWAPI::Position lastPosition;
    int lastAttackStartedAt;
    int potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck
    int lastUnstickFrame;       // frame we last sent a command to unstick the unit

    void typeSpecificUpdate() override;

    [[nodiscard]] bool isReady() const override;

    bool unstick() override;

    void attackUnit(Unit target) override;

    bool shouldKite(const Unit &target);

    void kiteFrom(BWAPI::Position position);
};