#pragma once

#include "Common.h"

// Does simple economic modelling of the opponent to try to figure out what we need to protect against
// Currently only works for Protoss opponents on one base
namespace OpponentEconomicModel
{
    // Whether the model is currently enabled
    // It currently only works for Protoss opponents on one base, so will switch to false when this is no longer the case
    bool enabled(int frame = 0);

    void initialize();

    void update();

    void opponentUnitCreated(BWAPI::UnitType type, int id, int estimatedCreationFrame);

    void opponentUnitDestroyed(BWAPI::UnitType type, int id);

    void opponentResearched(BWAPI::TechType type);

    // The worst-case number of the given unit the opponent can have at the given frame
    // If the frame is not specified, uses the current frame
    int worstCaseUnitCount(BWAPI::UnitType type, int frame = -1);

    // The minimum number of production facilities the enemy currently has of the given type
    int minimumProducerCount(BWAPI::UnitType producer);

    // The earliest frame the enemy could start building the given unit type
    int earliestUnitProductionFrame(BWAPI::UnitType type);
}
