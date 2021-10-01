#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class ElevatorRush : public Play
{
public:
    ElevatorRush();

    void update() override;

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void addUnit(const MyUnit &unit) override;

    void removeUnit(const MyUnit &unit) override;

protected:
    // This is set to true when we consider the elevator complete
    // We don't disband the play until all units are removed from the squad
    bool complete;

    BWAPI::TilePosition pylonTile;
    BWAPI::TilePosition roboTile;
    BWAPI::Position pickupPosition;
    BWAPI::Position dropPosition;

    MyUnit builder;
    MyUnit shuttle;
    std::shared_ptr<AttackBaseSquad> squad;
    std::set<MyUnit> transferQueue;
    std::set<MyUnit> transferring;
    std::set<MyUnit> transferred;
    int count;

    bool pickingUp; // Whether the shuttle is currently picking up or dropping off
};
