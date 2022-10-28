#pragma once

#include "MyUnit.h"

class MyCorsair : public MyUnitImpl
{
public:
    explicit MyCorsair(BWAPI::Unit unit) : MyUnitImpl(unit) {}

private:
    [[nodiscard]] bool isReady() const override;

    void attackUnit(const Unit &target,
                    std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                    bool clusterAttacking,
                    int enemyAoeRadius) override;
};
