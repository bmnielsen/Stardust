#pragma once

#include <vector>
#include "BWAPI.h"
#include "Grid.h"

namespace UAlbertaBot
{
class The;

class GridSafeAirPath : public Grid
{
private:
    The & the;
    std::vector<BWAPI::TilePosition> sortedTilePositions;

	void compute(const BWAPI::TilePosition & start, int limit);

public:
    GridSafeAirPath();
    GridSafeAirPath(const BWAPI::TilePosition & start);
    GridSafeAirPath(const BWAPI::TilePosition & start, int limit);

    const std::vector<BWAPI::TilePosition> & getSortedTiles() const;
    void computeAir(const BWAPI::TilePosition & start, int limit);
};
}