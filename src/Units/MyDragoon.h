#pragma once

#include "MyUnit.h"

class MyDragoon : public MyUnitImpl
{
public:
    explicit MyDragoon(BWAPI::Unit gameUnit);

    void update(BWAPI::Unit gameUnit) override;

    [[nodiscard]] int getLastAttackStartedAt() const { return lastAttackStartedAt; }

    [[nodiscard]] int getNextAttackPredictedAt() const { return nextAttackPredictedAt; }

    [[nodiscard]] int getPotentiallyStuckSince() const { return potentiallyStuckSince; }

private:
    int lastAttackStartedAt;
    int nextAttackPredictedAt;
    int potentiallyStuckSince;  // frame the unit might have been stuck since, or 0 if it isn't stuck
    int stopGlitchGuardUntil;   // frame until which a pending Attack_Unit needs the stop() guard (S: BB-port)

    [[nodiscard]] bool isReady() const override;

    bool unstick() override;

    void attackUnit(const Unit &target,
                    std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
                    bool clusterAttacking,
                    int enemyAoeRadius) override;
};