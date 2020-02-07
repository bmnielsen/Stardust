#include "GridSafeAirPath.h"

#include "MapTools.h"
#include "The.h"

using namespace UAlbertaBot;

GridSafeAirPath::GridSafeAirPath()
    : the(The::Root())
	, Grid()
{
}

GridSafeAirPath::GridSafeAirPath(const BWAPI::TilePosition & start)
    : the(The::Root())
    , Grid(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight(), -1)
{
    computeAir(start, 256 * 256 + 1);
}

// Compute the map only up to the given distance limit.
// Tiles beyond the limit are "unreachable".
GridSafeAirPath::GridSafeAirPath(const BWAPI::TilePosition & start, int limit)
    : the(The::Root())
    , Grid(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight(), -1)
{
    computeAir(start, limit);
}

const std::vector<BWAPI::TilePosition> & GridSafeAirPath::getSortedTiles() const
{
    return sortedTilePositions;
}

// Computes grid[x][y] = Manhattan ground distance from the starting tile to (x,y),
// up to the given limiting distance (and no farther, to save time).
// Uses BFS, since the map is quite large and DFS may cause a stack overflow
void GridSafeAirPath::computeAir(const BWAPI::TilePosition & start, int limit)
{
	const size_t LegalActions = 4;
	const int actionX[LegalActions] = { 1, -1, 0, 0 };
	const int actionY[LegalActions] = { 0, 0, 1, -1 };

	// the fringe for the BFS we will perform to calculate distances
    std::vector<BWAPI::TilePosition> fringe;
    fringe.reserve(width * height);
    fringe.push_back(start);

    grid[start.x][start.y] = 0;
	sortedTilePositions.push_back(start);

    for (size_t fringeIndex=0; fringeIndex<fringe.size(); ++fringeIndex)
    {
        const BWAPI::TilePosition & tile = fringe[fringeIndex];

		int currentDist = grid[tile.x][tile.y];
		if (currentDist >= limit)
		{
			continue;
		}

        // The legal actions define which tiles are nearest neighbors of this one.
        for (size_t a=0; a<LegalActions; ++a)
        {
            BWAPI::TilePosition nextTile(tile.x + actionX[a], tile.y + actionY[a]);

			if (nextTile.isValid() &&
                grid[nextTile.x][nextTile.y] == -1 &&
                the.airAttacks.at(nextTile) == 0)
            {
				fringe.push_back(nextTile);
				grid[nextTile.x][nextTile.y] = currentDist;
                sortedTilePositions.push_back(nextTile);
			}
        }
    }
}
