#pragma once

#include "Common.h"

// Used to store data about buildings that are either under construction or about to be constructed
class Building
{
public:
    BWAPI::UnitType     type;       // The type of the building
    BWAPI::TilePosition tile;       // The position of the building
    BWAPI::Unit         unit;       // The building itself
    BWAPI::Unit         builder;    // The unit that will build the building

    // TODO: State required by the builder

    // Constructor
    // We always decide on the position and builder unit before storing the building
    // If something happens to invalidate them before the building is started, we will create a new building
    Building(BWAPI::UnitType type, BWAPI::TilePosition tile, BWAPI::Unit builder);
    
    BWAPI::Position getPosition() const;

    // TODO: Stuff like handling things blocking construction, picking a new location, cancelling, etc.
};
