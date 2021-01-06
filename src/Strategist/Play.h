#pragma once

#include "Common.h"
#include "Squad.h"
#include "ProductionGoal.h"

#define PRIORITY_EMERGENCY 1
#define PRIORITY_WORKERS 2
#define PRIORITY_DEPOTS 3
#define PRIORITY_BASEDEFENSE 4              // Generally early-game defenders
#define PRIORITY_MAINARMYBASEPRODUCTION 5   // What we consider to be the minimum allowable production for our main army
#define PRIORITY_NORMAL 6
#define PRIORITY_MAINARMY 7
#define PRIORITY_LOWEST 8

class Play;

struct PlayUnitRequirement
{
    int count;
    BWAPI::UnitType type;
    BWAPI::Position position;
    bool allowFromVanguardCluster;
    const std::function<bool(const NavigationGrid::GridNode &gridNode)> gridNodePredicate;

    PlayUnitRequirement(int count,
                        BWAPI::UnitType type,
                        BWAPI::Position position,
                        const std::function<bool(const NavigationGrid::GridNode &gridNode)> gridNodePredicate = nullptr,
                        bool allowFromVanguardCluster = true)
            : count(count)
            , type(type)
            , position(position)
            , allowFromVanguardCluster(allowFromVanguardCluster)
            , gridNodePredicate(std::move(gridNodePredicate)) {}
};

struct PlayStatus
{
    bool complete = false;
    std::vector<PlayUnitRequirement> unitRequirements = {};
    std::vector<MyUnit> removedUnits = {};
    std::shared_ptr<Play> transitionTo = nullptr;
};

class Play
{
public:
    std::string label;
    PlayStatus status;
    std::map<BWAPI::UnitType, int> assignedIncompleteUnits;

    explicit Play(std::string label) : label(std::move(label)) {};

    virtual ~Play() = default;

    // Whether this play should receive any unassigned combat units
    [[nodiscard]] virtual bool receivesUnassignedUnits() const { return false; }

    // Returns the play's squad, if it has one
    virtual std::shared_ptr<Squad> getSquad() { return nullptr; }

    // Add a unit to the play. By default, this adds it to the play's squad, if it has one.
    virtual void addUnit(const MyUnit &unit);

    // Remove a unit from the play. By default, this removes it from the play's squad, if it has one.
    virtual void removeUnit(const MyUnit &unit);

    // Runs at the start of the Strategist's frame and updates the play status.
    virtual void update() {}

    // Runs at the end of the Strategist's frame and allows the play to order units or upgrades from the producer.
    virtual void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) {}

    // Runs at the end of the Strategist's frame and allows the play to reserve minerals at a specific future frame.
    virtual void addMineralReservations(std::vector<std::pair<int, int>> &mineralReservations) {}

    // Called when a play is being disbanded (either removed completely or transitioned to a different play).
    // It is the play's responsibility to call either removedUnitCallback or movableUnitCallback for all units that have been assigned
    // to it via addUnit (and not removed earlier through status.removedUnits).
    virtual void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                         const std::function<void(const MyUnit)> &movableUnitCallback);
};
