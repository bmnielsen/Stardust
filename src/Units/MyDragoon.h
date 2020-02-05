#pragma once

#include "MyUnit.h"

class MyDragoon : public MyUnit
{
public:
    explicit MyDragoon(BWAPI::Unit unit);

    int getLastAttackStartedAt() const { return lastAttackStartedAt; }

private:
    BWAPI::Position lastPosition;
    int lastAttackStartedAt;
    int potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck
    int lastUnstickFrame;       // frame we last sent a command to unstick the unit

    void typeSpecificUpdate() override;

    bool isReady() const override;

    bool unstick() override;

    void attackUnit(BWAPI::Unit target) override;

    bool shouldKite(BWAPI::Unit target);

    void kiteFrom(BWAPI::Position position);
};