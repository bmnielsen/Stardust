#pragma once

#include "Common.h"
#include "Squad.h"
#include "ProductionGoal.h"

#define PRIORITY_EMERGENCY 1
#define PRIORITY_WORKERS 2
#define PRIORITY_DEPOTS 3
#define PRIORITY_BASEDEFENSE 4
#define PRIORITY_NORMAL 5
#define PRIORITY_MAINARMY 6

class Play;

struct PlayUnitRequirement
{
    int count;
    BWAPI::UnitType type;
    BWAPI::Position position;

    PlayUnitRequirement(int count, BWAPI::UnitType type, BWAPI::Position position) : count(count), type(type), position(position) {}
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
    PlayStatus status;
    std::map<BWAPI::UnitType, int> assignedIncompleteUnits;

    virtual ~Play() = default;

    // Gets a label for this play for use in instrumentation
    [[nodiscard]] virtual const char *label() const = 0;

    // Whether this play should receive any unassigned combat units
    [[nodiscard]] virtual bool receivesUnassignedUnits() const { return false; }

    // Returns the play's squad, if it has one
    virtual std::shared_ptr<Squad> getSquad() { return nullptr; }

    // Add a unit to the play. By default, this adds it to the play's squad, if it has one.
    virtual void addUnit(MyUnit unit);

    // Remove a unit from the play. By default, this removes it from the play's squad, if it has one.
    virtual void removeUnit(MyUnit unit);

    // Runs at the start of the Strategist's frame and updates the play status.
    virtual void update() {}

    // Runs at the end of the Strategist's frame and allows the play to order units or upgrades from the producer.
    virtual void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) {}

    // Runs at the end of the Strategist's frame and allows the play to reserve minerals at a specific future frame.
    virtual void addMineralReservations(std::vector<std::pair<int, int>> &mineralReservations) {}
};
