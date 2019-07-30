#pragma once

#include "Common.h"
#include "Squad.h"
#include "ProductionGoal.h"

class Play;

struct PlayUnitRequirement
{
    int count;
    BWAPI::UnitType type;
    BWAPI::Position position;
};

struct PlayStatus
{
    bool complete = false;
    std::vector<PlayUnitRequirement> unitRequirements = {};
    std::vector<BWAPI::Unit> removedUnits = {};
    std::shared_ptr<Play> transitionTo = nullptr;
};

class Play
{
public:
    PlayStatus status;

    // Gets a label for this play for use in instrumentation
    virtual const char *label() const = 0;

    // Whether this play should receive any unassigned combat units
    virtual bool receivesUnassignedUnits() const { return false; }

    // Returns the play's squad, if it has one
    virtual std::shared_ptr<Squad> getSquad() { return nullptr; }

    // Add a unit to the play. By default, this adds it to the play's squad, if it has one.
    virtual void addUnit(BWAPI::Unit unit);

    // Remove a unit from the play. By default, this removes it from the play's squad, if it has one.
    virtual void removeUnit(BWAPI::Unit unit);

    // Runs at the start of the Strategist's frame and updates the play status.
    virtual void update() {}

    // Runs at the end of the Strategist's frame and allows the play to order units or upgrades from the producer.
    virtual void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) {}
};
