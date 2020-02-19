#pragma once

#include "Common.h"

#include <bwem.h>
#include "Choke.h"
#include "PathFinding.h"
#include "Unit.h"

class MyUnitImpl : public UnitImpl
{
public:
    explicit MyUnitImpl(BWAPI::Unit unit);

    ~MyUnitImpl() override = default;

    void update(BWAPI::Unit unit) override;

    void issueMoveOrders();

    bool moveTo(BWAPI::Position position);

    virtual void attackUnit(Unit target);

    [[nodiscard]] virtual bool isReady() const { return true; };

    virtual bool unstick();

    void move(BWAPI::Position position, bool force = false);

    void attack(BWAPI::Unit target);

    void rightClick(BWAPI::Unit target);

    void gather(BWAPI::Unit target);

    void returnCargo();

    bool build(BWAPI::UnitType type, BWAPI::TilePosition tile);

    bool train(BWAPI::UnitType type);

    bool upgrade(BWAPI::UpgradeType type);

    void stop();

    void cancelConstruction();

    void load(BWAPI::Unit cargo);

    void unloadAll();

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

typedef std::shared_ptr<MyUnitImpl> MyUnit;

std::ostream &operator<<(std::ostream &os, const MyUnitImpl &unit);
