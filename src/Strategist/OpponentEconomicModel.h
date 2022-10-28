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

    void opponentUnitCreated(BWAPI::UnitType type, int id, int estimatedCreationFrame, bool creationFrameKnown = false);

    void opponentUnitDestroyed(BWAPI::UnitType type, int id, int frameDestroyed = -1);

    void opponentResearched(BWAPI::TechType type, int frameStarted = -1);

    void opponentUpgraded(BWAPI::UpgradeType type, int level, int frameStarted);

    // The worst-case number of the given unit the opponent can have at the given frame
    // First element of pair is the number of units the opponent currently has
    // Second element of pair is the number of units the opponent could have in total if they spent all resources on that unit type
    // If the frame is not specified, uses the current frame
    std::pair<int, int> worstCaseUnitCount(BWAPI::UnitType type, int frame = -1);

    // The minimum number of production facilities the enemy currently has of the given type
    int minimumProducerCount(BWAPI::UnitType producerType);

    // Whether the enemy has built at least one of the given type of unit
    bool hasBuilt(BWAPI::UnitType type);

    // The earliest frame the enemy could start building the given unit type
    int earliestUnitProductionFrame(BWAPI::UnitType type);
}
