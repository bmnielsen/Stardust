#pragma once

#include "Common.h"
#include "Unit.h"

#include <bwem.h>

class Base
{
public:

    //enum class Owner { None, Me, Enemy, Ally };

    BWAPI::Player owner;                                      // Who owns the base
    Unit resourceDepot;                              // The resource depot for the base, may be null
    int ownedSince;                                 // Frame the base last changed ownership
    int lastScouted;                                // When we have last seen this base
    bool spiderMined;                                // Do we suspect this base to have a spider mine blocking it
    bool requiresMineralWalkFromEnemyStartLocations; // Does this base require mineral walking for the enemy to reach it
    BWAPI::Position mineralLineCenter; // Approximate center of the mineral line
    BWAPI::Unit workerDefenseRallyPatch; // Mineral patch where workers should rally when doing worker defense
    std::set<BWAPI::TilePosition> mineralLineTiles; // All tiles considered to be part of the mineral line

    Base(BWAPI::TilePosition _tile, const BWEM::Base *_bwemBase);

    [[nodiscard]] const BWAPI::TilePosition &getTilePosition() const { return tile; }

    [[nodiscard]] BWAPI::Position getPosition() const { return BWAPI::Position(tile) + BWAPI::Position(64, 48); }

    [[nodiscard]] const BWEM::Area *getArea() const { return bwemBase->GetArea(); }

    [[nodiscard]] size_t mineralPatchCount() const { return bwemBase->Minerals().size(); }

    [[nodiscard]] std::vector<BWAPI::Unit> mineralPatches() const;

    [[nodiscard]] std::vector<BWAPI::Unit> geysers() const;

    [[nodiscard]] std::vector<BWAPI::Unit> refineries() const;

    [[nodiscard]] int minerals() const;

    [[nodiscard]] int gas() const;

    [[nodiscard]] bool isStartingBase() const;

    [[nodiscard]] bool isInMineralLine(BWAPI::TilePosition pos) const;

private:

    BWAPI::TilePosition tile;
    const BWEM::Base *bwemBase;

    void analyzeMineralLine();
};
