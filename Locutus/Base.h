#pragma once

#include "Common.h"

#include <bwem.h>

class Base
{
public:

    BWAPI::Player owner;                                      // Owning player, may be neutral
    int           ownedSince;                                 // Frame the base last changed ownership
    int           lastScouted;                                // When we have last seen this base
    bool          spiderMined;                                // Do we suspect this base to have a spider mine blocking it
    bool          requiresMineralWalkFromEnemyStartLocations; // Does this base require mineral walking for the enemy to reach it

    Base(BWAPI::TilePosition _tile, const BWEM::Base * _bwemBase);

    const BWAPI::TilePosition & getTilePosition() const { return tile; };
    const BWAPI::Position & getPosition() const { return BWAPI::Position(tile) + BWAPI::Position(64,48); };

private:

    BWAPI::TilePosition tile;
    const BWEM::Base * bwemBase;

};
