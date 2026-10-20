#pragma once

#include "MyUnit.h"

class MyCannon : public MyUnitImpl
{
public:
    explicit MyCannon(BWAPI::Unit gameUnit) : MyUnitImpl(gameUnit) {}

private:
    bool unstick() override { return false; };

    [[nodiscard]] bool isReady() const override;

    void attackUnit(const Unit &target,
                    std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
                    bool clusterAttacking,
                    int enemyAoeRadius) override;
};
