#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class Elevator : public PlayItem
{
public:
    explicit Elevator(bool fromOurMain = false, BWAPI::UnitType unitType = BWAPI::UnitTypes::Protoss_Dragoon);

    void update() override;

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    [[nodiscard]] bool canReassignUnit(const OwnedUnit &gameUnit) const override;

    void addUnit(const OwnedUnit &gameUnit) override;

    void removeUnit(const OwnedUnit &gameUnit) override;

    /* Gets the pair of positions to use for an elevator into or out of a base */
    /* The first position is in the base, the second outside it */
    static std::pair<BWAPI::TilePosition, BWAPI::TilePosition> selectPositions(Base *baseLoc);

protected:
    bool fromOurMain;
    BWAPI::UnitType unitType;

    /* This is set to true when we consider the elevator complete */
    /* We don't disband the play until all units are removed from the squad */
    bool complete;

    BWAPI::Position pickupPosition;
    BWAPI::Position dropPosition;

    OwnedUnit shuttle;
    std::shared_ptr<AttackBaseSquad> squad;
    std::set<OwnedUnit> transferQueue;
    std::set<OwnedUnit> transferring;
    std::set<OwnedUnit> transferred;
    int count;
    int countAddedToSquad;

    bool pickingUp; // Whether the shuttle is currently picking up or dropping off
};
