#pragma once

#include "Common.h"

#include <set>
#include <bwem.h>

class Choke
{
public:
    Choke(const BWEM::ChokePoint* _choke);

    BWAPI::Position Center() const { return BWAPI::Position(choke->Center()) + BWAPI::Position(4, 4); }

    const BWEM::ChokePoint* choke;

    int width;

    bool isRamp;
    BWAPI::TilePosition highElevationTile;
    std::set<BWAPI::Position> probeBlockScoutPositions;  // Minimum set of positions we can put a probe to block an enemy worker scout from getting in
    std::set<BWAPI::Position> zealotBlockScoutPositions; // Minimum set of positions we can put a zealot to block an enemy worker scout from getting in

    bool requiresMineralWalk;
    BWAPI::Unit firstAreaMineralPatch;          // Mineral patch to use when moving towards the first area in the chokepoint's GetAreas()
    BWAPI::Position firstAreaStartPosition;     // Start location to move to that should give visibility of firstAreaMineralPatch
    BWAPI::Unit secondAreaMineralPatch;         // Mineral patch to use when moving towards the second area in the chokepoint's GetAreas()
    BWAPI::Position secondAreaStartPosition;    // Start location to move to that should give visibility of secondAreaMineralPatch

private:
    void computeRampHighGroundPosition();
    void computeScoutBlockingPositions(BWAPI::Position center, BWAPI::UnitType type, std::set<BWAPI::Position> & result);
};