#pragma once

#include "Common.h"
#include "Unit.h"
#include "Resource.h"

#include <bwem.h>

class Base
{
    friend class Outsider; // On Outsider we change the ring bases into mineral-onlies

public:
    Base (const Base&) = delete;
    Base &operator=(const Base&) = delete;

    //enum class Owner { None, Me, Enemy, Ally };

    BWAPI::Player owner;                                // Who owns the base
    Unit resourceDepot;                                 // The resource depot for the base, may be null
    int ownedSince;                                     // Frame the base last changed ownership
    int lastScouted;                                    // When we have last seen this base
    bool blockedByEnemy;                                // Do we suspect this base to be blocked by a hidden enemy unit
    bool requiresMineralWalkFromEnemyStartLocations;    // Does this base require mineral walking for the enemy to reach it
    bool island;                                        // Whether this base is ground-connected to any main base
    BWAPI::Position mineralLineCenter;                  // Approximate center of the mineral line
    Resource workerDefenseRallyPatch;                   // Mineral patch where workers should rally when doing worker defense
    std::set<BWAPI::TilePosition> mineralLineTiles;     // All tiles considered to be part of the mineral line
    BWAPI::Unit blockingNeutral;                        // A neutral unit that must be cleared before building the nexus

    int minerals;   // Current total count of minerals remaining
    int gas;        // Current total count of gas remaining

    Base(BWAPI::TilePosition tile, const BWEM::Base *bwemBase);

    Base(BWAPI::TilePosition tile, const BWEM::Area *bwemArea, std::vector<Resource> mineralPatches, std::vector<Resource> geysers);

    [[nodiscard]] const BWAPI::TilePosition &getTilePosition() const { return tile; }

    [[nodiscard]] BWAPI::Position getPosition() const { return center; }

    [[nodiscard]] const BWEM::Area *getArea() const { return bwemArea; }

    [[nodiscard]] size_t mineralPatchCount() const { return _mineralPatches.size(); }

    [[nodiscard]] size_t geyserCount() const { return _geysersOrRefineries.size(); }

    [[nodiscard]] const std::vector<Resource> &mineralPatches() const { return _mineralPatches; }

    [[nodiscard]] const std::vector<Resource> &geysersOrRefineries() const { return _geysersOrRefineries; }

    [[nodiscard]] bool isStartingBase() const;

    [[nodiscard]] bool isInMineralLine(BWAPI::TilePosition pos) const;

    [[nodiscard]] bool hasGeyserOrRefineryAt(BWAPI::TilePosition geyserTopLeft) const;

    [[nodiscard]] bool gasRequiresFourWorkers(BWAPI::TilePosition geyserTopLeft) const;

    void update();

private:

    BWAPI::TilePosition tile;
    BWAPI::Position center;
    const BWEM::Area *bwemArea;

    std::vector<Resource> _mineralPatches;
    std::vector<Resource> _geysersOrRefineries;

    void analyzeMineralLine();
};
