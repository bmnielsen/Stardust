#pragma once

#include "MyUnit.h"

class MyCorsair : public MyUnitImpl
{
public:
    explicit MyCorsair(BWAPI::Unit gameUnit) : MyUnitImpl(gameUnit) {}

private:
    [[nodiscard]] bool isReady() const override;

    void attackUnit(const Unit &target,
                    std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
                    bool clusterAttacking,
                    int enemyAoeRadius) override;
};
