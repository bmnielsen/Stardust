#pragma once

#include "Common.h"

#include <bwem.h>
#include "Choke.h"

class MyUnit
{
public:
    MyUnit(BWAPI::Unit unit);

    void update();

    bool moveTo(BWAPI::Position position, bool avoidNarrowChokes = false);
    int  distanceToMoveTarget() const;

    virtual void attackUnit(BWAPI::Unit target);

    virtual bool isReady() const { return true; };
    virtual bool isStuck() const { return unit->isStuck(); };

    void move(BWAPI::Position position);
    void attack(BWAPI::Unit target);
    void rightClick(BWAPI::Unit target);
    void gather(BWAPI::Unit target);
    void returnCargo();
    bool build(BWAPI::UnitType type, BWAPI::TilePosition tile);
    void stop();

protected:
    BWAPI::Unit     unit;

    bool issuedOrderThisFrame;

    BWAPI::Position                     targetPosition;
    BWAPI::Position                     currentlyMovingTowards;
    std::deque<const BWEM::ChokePoint*> waypoints;
    int                                 lastMoveFrame;

    virtual void typeSpecificUpdate() {}

    virtual void resetMoveData();
    void updateMoveWaypoints();
    virtual void moveToNextWaypoint();
    virtual bool mineralWalk(const Choke * choke = nullptr) { return false; }
};
