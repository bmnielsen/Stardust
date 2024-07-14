#pragma once

#include "MyUnit.h"

class MyWorkerImpl;

typedef std::shared_ptr<MyWorkerImpl> MyWorker;

class MyWorkerImpl : public MyUnitImpl
{
public:
    int carryingResource;               // Whether the unit is carrying a resource
    int lastDeliveredResource;          // Frame when the unit last delivered a resource
    int lastStartedMining;              // Frame when the unit last started mining

    int horizontalKiloSpeed;    // Integer representation of the unit's speed on the X axis, multiplied by 1000
    int verticalKiloSpeed;      // Integer representation of the unit's speed on the Y axis, multiplied by 1000
    int kiloHeading;            // Integer representation of the unit's heading, multiplied by 1000

    explicit MyWorkerImpl(BWAPI::Unit unit);

    void update(BWAPI::Unit unit) override;

protected:
    void resetMoveData() override;

    bool mineralWalk(const Choke *choke) override;

private:
    BWAPI::Unit mineralWalkingPatch;
    const BWEM::Area *mineralWalkingTargetArea;
    BWAPI::Position mineralWalkingStartPosition;
    int nextAttackPredictedAt;

    void attackUnit(const Unit &target,
                    std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                    bool clusterAttacking,
                    int enemyAoeRadius) override;
};
