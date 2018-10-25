#include "BWEB.h"

namespace BWEB
{
	bool Map::overlapsAnything(const TilePosition here, const int width, const int height, bool ignoreBlocks)
	{
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					continue;
				if (overlapGrid[x][y] > 0)
					return true;
			}
		}
		return false;
	}

	bool Map::isWalkable(const TilePosition here)
	{
		int cnt = 0;
		const auto start = WalkPosition(here);
		for (auto x = start.x; x < start.x + 4; x++) {
			for (auto y = start.y; y < start.y + 4; y++) {
				if (!WalkPosition(x, y).isValid())
					return false;
				if (!Broodwar->isWalkable(WalkPosition(x, y)))
					cnt++;
			}
		}
		return cnt <= 1;
	}

	int Map::tilesWithinArea(BWEM::Area const * area, const TilePosition here, const int width, const int height)
	{
		auto cnt = 0;
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					return false;

				// Make an assumption that if it's on a chokepoint geometry, it belongs to the area provided
				if (mapBWEM.GetArea(t) == area /*|| !mapBWEM.GetArea(t)*/)
					cnt++;
			}
		}
		return cnt;
	}

	int Utils::tilesWithinArea(BWEM::Area const * area, const TilePosition here, const int width, const int height)
	{
		return BWEB::Map::Instance().tilesWithinArea(area, here, width, height);
	}
}