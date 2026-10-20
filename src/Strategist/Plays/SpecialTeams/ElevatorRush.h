#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"
#include "MyWorker.h"

class ElevatorRush : public PlayItem
{
public:
    ElevatorRush();

    void update() override;

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    void addUnit(const OwnedUnit &gameUnit) override;

    void removeUnit(const OwnedUnit &gameUnit) override;

protected:
    /* This is set to true when we consider the elevator complete */
    /* We don't disband the play until all units are removed from the squad */
    bool complete;

    BWAPI::TilePosition pylonTile;
    BWAPI::TilePosition roboTile;
    BWAPI::Position pickupPosition;
    BWAPI::Position dropPosition;

    OwnedWorker builder;
    OwnedUnit shuttle;
    std::shared_ptr<AttackBaseSquad> squad;
    std::set<OwnedUnit> transferQueue;
    std::set<OwnedUnit> transferring;
    std::set<OwnedUnit> transferred;
    int count;

    bool pickingUp; // Whether the shuttle is currently picking up or dropping off
};
