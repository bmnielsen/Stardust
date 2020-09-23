#pragma once

#include "MyUnit.h"

class MyWorker : public MyUnitImpl
{
public:
    explicit MyWorker(BWAPI::Unit unit);

protected:
    void resetMoveData() override;

    bool mineralWalk(const Choke *choke) override;

private:
    BWAPI::Unit mineralWalkingPatch;
    const BWEM::Area *mineralWalkingTargetArea;
    BWAPI::Position mineralWalkingStartPosition;

    void attackUnit(const Unit &target, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, bool clusterAttacking) override;
};
