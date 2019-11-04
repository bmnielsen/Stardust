#pragma once

#include "Common.h"

#include <bwem.h>
#include "Choke.h"
#include "PathFinding.h"

class MyUnit
{
public:
    explicit MyUnit(BWAPI::Unit unit);

    virtual ~MyUnit() = default;

    void update();

    void issueMoveOrders();

    bool moveTo(BWAPI::Position position);

    virtual void attackUnit(BWAPI::Unit target);

    virtual bool isReady() const { return true; };

    virtual bool isStuck() const { return unit->isStuck(); };

    void move(BWAPI::Position position, bool force = false);

    void attack(BWAPI::Unit target);

    void rightClick(BWAPI::Unit target);

    void gather(BWAPI::Unit target);

    void returnCargo();

    bool build(BWAPI::UnitType type, BWAPI::TilePosition tile);

    void stop();

protected:
    BWAPI::Unit unit;

    bool issuedOrderThisFrame;

    BWAPI::Position targetPosition;
    BWAPI::Position currentlyMovingTowards;
    NavigationGrid *grid;
    std::deque<const BWEM::ChokePoint *> chokePath;
    const NavigationGrid::GridNode *gridNode;
    int lastMoveFrame;

    virtual void typeSpecificUpdate() {}

    virtual void resetMoveData();

    virtual void moveToNextWaypoint();

    void updateMoveWaypoints();

    void unstickMoveUnit();

    void resetGrid();

    void updateChokePath(const BWEM::Area *unitArea);

    virtual bool mineralWalk(const Choke *choke) { return false; }
};
