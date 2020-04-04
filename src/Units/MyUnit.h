#pragma once

#include "Common.h"

#include <bwem.h>
#include "Choke.h"
#include "PathFinding.h"
#include "Unit.h"

class MyUnitImpl;

typedef std::shared_ptr<MyUnitImpl> MyUnit;

class MyUnitImpl : public UnitImpl
{
public:
    explicit MyUnitImpl(BWAPI::Unit unit);

    ~MyUnitImpl() override = default;

    void update(BWAPI::Unit unit) override;

    [[nodiscard]] bool isBeingManufacturedOrCarried() const override;

    void moveTo(BWAPI::Position position, bool direct = false);

    void issueMoveOrders();

    virtual void attackUnit(const Unit &target, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets);

    [[nodiscard]] virtual bool isReady() const { return true; };

    virtual bool unstick();

    [[nodiscard]] int getUnstickUntil() const { return unstickUntil; };

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

    struct MoveCommand
    {
        BWAPI::Position targetPosition;
        bool direct;

        MoveCommand(BWAPI::Position targetPosition, bool direct) : targetPosition(targetPosition), direct(direct) {}
    };

    // The move command to be issued this frame.
    std::unique_ptr<MoveCommand> moveCommand;

    // The final target position of the current move. If invalid, the unit is not performing a move.
    BWAPI::Position targetPosition;

    // For grid-based moves, the position of the grid node currently being moved towards.
    // For choke-based moves, the position of the choke currently being moved towards.
    // For direct moves, is set to targetPosition when the move command is issued.
    BWAPI::Position currentlyMovingTowards;

    // The remaining choke points between the unit and its target.
    std::deque<const BWEM::ChokePoint *> chokePath;

    // The current navigation grid being used for a move. May or may not apply all the way to targetPosition. For example, if a worker needs to
    // do mineral walking, it may use a navigation grid to get to the mineral walk choke, then a different navigation grid to get from there to
    // the target.
    NavigationGrid *grid;

    // The current grid node occupied by the unit in the above grid.
    const NavigationGrid::GridNode *gridNode;

    // The last frame where we sent a move command to the unit.
    int lastMoveFrame;

    // If we have sent a command to the unit to unstick it, when we should next send a normal command again.
    int unstickUntil;

    void initiateMove();

    virtual void resetMoveData();

    void moveToNextWaypoint();

    void updateMoveWaypoints();

    bool unstickMoveUnit();

    void resetGrid();

    void updateChokePath(const BWEM::Area *unitArea);

    virtual bool mineralWalk(const Choke *choke) { return false; }
};

std::ostream &operator<<(std::ostream &os, const MyUnitImpl &unit);
