#pragma once

#include "Common.h"

#include <bwem.h>
#include "Choke.h"
#include "PathFinding.h"
#include "Unit.h"

class MyUnit : public Unit
{
public:
    explicit MyUnit(BWAPI::Unit unit);

    virtual ~MyUnit() = default;

    void update(BWAPI::Unit unit) override;

    void issueMoveOrders();

    bool moveTo(BWAPI::Position position);

    virtual void attackUnit(BWAPI::Unit target);

    virtual bool isReady() const { return true; };

    virtual bool unstick();

    void move(BWAPI::Position position, bool force = false);

    void attack(BWAPI::Unit target);

    void rightClick(BWAPI::Unit target);

    void gather(BWAPI::Unit target);

    void returnCargo();

    bool build(BWAPI::UnitType type, BWAPI::TilePosition tile);

    void stop();

protected:
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

    bool unstickMoveUnit();

    void resetGrid();

    void updateChokePath(const BWEM::Area *unitArea);

    virtual bool mineralWalk(const Choke *choke) { return false; }
};
