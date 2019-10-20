#pragma once

#include "Common.h"

// Used to store data about build locations
class Building
{
public:
    BWAPI::TilePosition tile;           // The position
    int poweredAt;      // At what frame will the location be powered
    int workerFrames;   // Approximately how many frames the builder will take to get to this location

    // TODO: State required by the builder

    // Constructor
    // We always decide on the position and builder unit before storing the building
    // If something happens to invalidate them before the building is started, we will create a new building
    Building(BWAPI::UnitType type, BWAPI::TilePosition tile, BWAPI::Unit builder);

    BWAPI::Position getPosition() const;

    bool constructionStarted() const;

    int expectedFramesUntilCompletion() const;

    // TODO: Stuff like handling things blocking construction, picking a new location, cancelling, etc.
};
