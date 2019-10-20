#pragma once

#include "Common.h"

#include <bwem.h>

class Base
{
public:

    //enum class Owner { None, Me, Enemy, Ally };

    BWAPI::Player owner;                                      // Who owns the base
    BWAPI::Unit resourceDepot;                              // The resource depot for the base, may be null
    int ownedSince;                                 // Frame the base last changed ownership
    int lastScouted;                                // When we have last seen this base
    bool spiderMined;                                // Do we suspect this base to have a spider mine blocking it
    bool requiresMineralWalkFromEnemyStartLocations; // Does this base require mineral walking for the enemy to reach it
    BWAPI::Position mineralLineCenter; // Approximate center of the mineral line

    Base(BWAPI::TilePosition _tile, const BWEM::Base *_bwemBase);

    const BWAPI::TilePosition &getTilePosition() const { return tile; }

    const BWAPI::Position getPosition() const { return BWAPI::Position(tile) + BWAPI::Position(64, 48); }

    const BWEM::Area *getArea() const { return bwemBase->GetArea(); }

    size_t mineralPatchCount() const { return bwemBase->Minerals().size(); }

    std::vector<BWAPI::Unit> mineralPatches() const;

    std::vector<BWAPI::Unit> geysers() const;

    std::vector<BWAPI::Unit> refineries() const;

    int minerals() const;

    int gas() const;

    bool isStartingBase() const;

private:

    BWAPI::TilePosition tile;
    const BWEM::Base *bwemBase;

};
