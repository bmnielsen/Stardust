#pragma once

#include "MyUnit.h"

class MyWorkerImpl;

typedef std::shared_ptr<MyWorkerImpl> MyWorker;

class MyWorkerImpl : public MyUnitImpl
{
public:
    bool carryingResource;                       // Whether the unit is carrying a resource
    int lastCarryingResourceChange;              // Frame when the unit last acquired or delivered a resource
    int lastStartedMining;                       // Frame when the unit last started mining with the order timer counting down
    int lastTransitionedToMiningOrder;           // Frame when the unit last switched to the MiningMinerals order
    int lastTransitionedToWaitForMineralsOrder;  // Frame when the unit last switched to the WaitForMinerals order

    // Where the worker spawned, if it spawned normally
    // If it appeared because of a trigger (e.g. in a test), it will be Positions::Invalid
    BWAPI::Position spawnPosition;

    int8_t horizontalSpeed8b;    // 8-bit integer representation of the unit's speed on the X axis
    int8_t verticalSpeed8b;      // 8-bit integer representation of the unit's speed on the Y axis
    uint8_t heading8b;           // 8-bit integer representation of the unit's heading

    explicit MyWorkerImpl(BWAPI::Unit unit);

    void update(BWAPI::Unit unit) override;

    bool gather(BWAPI::Unit target);

    bool returnCargo();

    static int8_t to8bSpeed(double value);
    static uint8_t to8bHeading(double value);

protected:
    void resetMoveData() override;

    bool mineralWalk(const Choke *choke) override;

private:
    BWAPI::Unit mineralWalkingPatch;
    const BWEM::Area *mineralWalkingTargetArea;
    BWAPI::Position mineralWalkingStartPosition;
    int nextAttackPredictedAt;
    BWAPI::Order previousOrder;

    std::set<int> gatherCommandFrames;

    void attackUnit(const Unit &target,
                    std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                    bool clusterAttacking,
                    int enemyAoeRadius) override;
};
