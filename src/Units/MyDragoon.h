#pragma once

#include "MyUnit.h"

class MyDragoon : public MyUnit
{
public:
    MyDragoon(BWAPI::Unit unit);

    int getLastAttackStartedAt() const { return lastAttackStartedAt; }

private:
    BWAPI::Position lastPosition;
    int lastAttackStartedAt;
    int potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck

    void typeSpecificUpdate();
    bool isReady() const;
    bool isStuck() const;
    void attackUnit(BWAPI::Unit target);
    bool shouldKite(BWAPI::Unit target);
    void kiteFrom(BWAPI::Position position);
};