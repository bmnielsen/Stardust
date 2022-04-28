#pragma once

#include "MyUnit.h"

class MyCannon : public MyUnitImpl
{
public:
    explicit MyCannon(BWAPI::Unit unit) : MyUnitImpl(unit) {}

private:
    bool unstick() override { return false; };

    [[nodiscard]] bool isReady() const override;

    void attackUnit(const Unit &target,
                    std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                    bool clusterAttacking,
                    int enemyAoeRadius) override;
};
