#pragma once
#include <set>
#include <BWAPI.h>
#include <bwem.h>

namespace BWEB::Blocks
{
	class Block
	{
		int w, h;
		BWAPI::TilePosition t;
		std::set <BWAPI::TilePosition> small, medium, large;
	public:
		Block() : w(0), h(0) {};
		Block(const int width, const int height, const BWAPI::TilePosition tile) {
			w = width, h = height, t = tile;
		}

		int width() const { return w; }
		int height() const { return h; }

		// Returns the top left tile position of this block
		BWAPI::TilePosition Location() const { return t; }

		// Returns the const set of tilepositions that belong to 2x2 (small) buildings
		const std::set<BWAPI::TilePosition>& SmallTiles() const { return small; }

		// Returns the const set of tilepositions that belong to 3x2 (medium) buildings
        const std::set<BWAPI::TilePosition>& MediumTiles() const { return medium; }

		// Returns the const set of tilepositions that belong to 4x3 (large) buildings
        const std::set<BWAPI::TilePosition>& LargeTiles() const { return large; }

		void insertSmall(const BWAPI::TilePosition here) { small.insert(here); }
		void insertMedium(const BWAPI::TilePosition here) { medium.insert(here); }
		void insertLarge(const BWAPI::TilePosition here) { large.insert(here); }
	};

	/// <summary> Initializes the building of every BWEB::Block on the map, call it only once per game. </summary>
	void findBlocks(BWAPI::Player);
	void findBlocks(BWAPI::Race);
	void findBlocks();

	/// <summary> Erases any blocks at the specified TilePosition. </summary>
	/// <param name="here"> The TilePosition that you want to delete any BWEB::Block that exists here. </param>
	void eraseBlock(BWAPI::TilePosition here);

	/// <summary> Returns a vector containing every Block </summary>
	std::vector<Block>& getBlocks();

	/// <summary> Returns the closest BWEB::Block to the given TilePosition. </summary>
	const Blocks::Block* getClosestBlock(BWAPI::TilePosition);
}
