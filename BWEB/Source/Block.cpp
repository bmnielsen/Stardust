#include "Block.h"
#include "BWEB.h"

using namespace std;
using namespace BWAPI;
using namespace BWEB::Map;

namespace BWEB::Blocks
{
	namespace {
		vector<Block> allBlocks;
		map<const BWEM::Area *, int> typePerArea;

		bool canAddBlock(const TilePosition here, const int width, const int height)
		{
			// Check if a block of specified size would overlap any bases, resources or other blocks
			for (auto x = here.x - 1; x < here.x + width + 1; x++) {
				for (auto y = here.y - 1; y < here.y + height + 1; y++) {
					TilePosition t(x, y);
					if (!t.isValid() || !mapBWEM.GetTile(t).Buildable() || Map::isOverlapping(t) || Map::isReserved(t))
						return false;
				}
			}
			return true;
		}

		void insertBlock(Race race, TilePosition here, int width, int height)
		{
			Block newBlock(width, height, here);
			BWEM::Area const* a = mapBWEM.GetArea(here);

			const auto &offset = [&](int x, int y) {
				return here + TilePosition(x, y);
			};

			if (race == Races::Protoss) {
				if (height == 2) {
					// Pylon and medium
					if (width == 5) {
						newBlock.insertSmall(offset(0, 0));
						newBlock.insertMedium(offset(2, 0));
					}
					else return;
				}

				else if (height == 4) {
					// Pylon and 2 medium
					if (width == 5) {
						newBlock.insertSmall(here);
						newBlock.insertSmall(offset(0, 2));
						newBlock.insertMedium(offset(2, 0));
						newBlock.insertMedium(offset(2, 2));
					}
					else
						return;
				}

				else if (height == 5) {
					// Gate and 2 Pylons
					if (width == 4) {
						newBlock.insertLarge(here);
						newBlock.insertSmall(offset(0, 3));
						newBlock.insertSmall(offset(2, 3));
					}
					else return;
				}

				else if (height == 6) {
					// 4 Gates and 3 Pylons
					if (width == 10) {
						newBlock.insertSmall(offset(4, 0));
						newBlock.insertSmall(offset(4, 2));
						newBlock.insertSmall(offset(4, 4));
						newBlock.insertLarge(here);
						newBlock.insertLarge(offset(0, 3));
						newBlock.insertLarge(offset(6, 0));
						newBlock.insertLarge(offset(6, 3));
					}
					else if (width == 18) {
						newBlock.insertSmall(offset(8, 0));
						newBlock.insertSmall(offset(8, 2));
						newBlock.insertSmall(offset(8, 4));
						newBlock.insertLarge(here);
						newBlock.insertLarge(offset(0, 3));
						newBlock.insertLarge(offset(4, 0));
						newBlock.insertLarge(offset(4, 3));
						newBlock.insertLarge(offset(10, 0));
						newBlock.insertLarge(offset(10, 3));
						newBlock.insertLarge(offset(14, 0));
						newBlock.insertLarge(offset(14, 3));
					}
					else return;
				}

				else  if (height == 8) {
					// 4 Gates and 4 Pylons
					if (width == 8) {
						newBlock.insertSmall(offset(0, 3));
						newBlock.insertSmall(offset(2, 3));
						newBlock.insertSmall(offset(4, 3));
						newBlock.insertSmall(offset(6, 3));
						newBlock.insertLarge(here);
						newBlock.insertLarge(offset(4, 0));
						newBlock.insertLarge(offset(0, 5));
						newBlock.insertLarge(offset(4, 5));
					}
					else return;
				}
				else return;
			}

			if (race == Races::Terran) {
				if (height == 2) {
					if (width == 3) {
						newBlock.insertMedium(here);
					}
					else return;
				}
				else if (height == 4) {
					if (width == 3) {
						newBlock.insertMedium(here);
						newBlock.insertMedium(offset(0, 2));
					}
					else if (width == 6) {
						newBlock.insertMedium(here);
						newBlock.insertMedium(offset(0, 2));
						newBlock.insertMedium(offset(3, 0));
						newBlock.insertMedium(offset(3, 2));
					}
					else return;
				}
				else if (height == 5) {
					if (width == 6 && (!a || typePerArea[a] + 1 < 16)) {
						newBlock.insertLarge(here);
						newBlock.insertSmall(offset(4, 1));
						newBlock.insertMedium(offset(0, 3));
						newBlock.insertMedium(offset(3, 3));

						if (a)
							typePerArea[a] += 1;
					}
					else return;
				}
				else if (height == 6) {
					if (width == 10 && (!a || typePerArea[a] + 4 < 16)) {
						newBlock.insertLarge(here);
						newBlock.insertLarge(offset(4, 0));
						newBlock.insertLarge(offset(0, 3));
						newBlock.insertLarge(offset(4, 3));
						newBlock.insertSmall(offset(8, 1));
						newBlock.insertSmall(offset(8, 4));

						if (a)
							typePerArea[a] += 4;
					}
					else return;
				}
				else return;
			}

			if (race == Races::Zerg) {
				if (width == 2 && height == 2) {
					addOverlap(here, 2, 2);
					newBlock.insertSmall(here);
				}
				if (width == 3 && height == 4) {
					addOverlap(here, 3, 4);
					newBlock.insertMedium(here);
					newBlock.insertMedium(offset(0, 2));
				}
				if (width == 8 && height == 5) {
					addOverlap(here, 8, 5);
					newBlock.insertLarge(offset(0, 2));
					newBlock.insertLarge(offset(4, 2));
					newBlock.insertSmall(offset(6, 0));
					newBlock.insertMedium(here);
					newBlock.insertMedium(offset(3, 0));
				}
			}

			allBlocks.push_back(newBlock);
			addOverlap(here, width, height);
		}

		void insertStartBlock(Race race, const TilePosition here, const bool h, const bool v)
		{
			const auto &offset = [&](int x, int y) {
				return here + TilePosition(x, y);
			};

			if (race == Races::Protoss || race == Races::Zerg) {
				if (h) {
					if (v) {
						Block newBlock(8, 5, here);
						addOverlap(here, 8, 5);
						newBlock.insertLarge(here);
						newBlock.insertLarge(offset(4, 0));
						newBlock.insertSmall(offset(6, 3));
						newBlock.insertMedium(offset(0, 3));
						newBlock.insertMedium(offset(3, 3));
						allBlocks.push_back(newBlock);
					}
					else {
						Block newBlock(8, 5, here);
						addOverlap(here, 8, 5);
						newBlock.insertLarge(offset(0, 2));
						newBlock.insertLarge(offset(4, 2));
						newBlock.insertSmall(offset(6, 0));
						newBlock.insertMedium(here);
						newBlock.insertMedium(offset(3, 0));
						allBlocks.push_back(newBlock);
					}
				}
				else {
					if (v)
					{
						Block newBlock(8, 5, here);
						addOverlap(here, 8, 5);
						newBlock.insertLarge(here);
						newBlock.insertLarge(offset(4, 0));
						newBlock.insertSmall(offset(0, 3));
						newBlock.insertMedium(offset(2, 3));
						newBlock.insertMedium(offset(5, 3));
						allBlocks.push_back(newBlock);
					}
					else {
						Block newBlock(8, 5, here);
						addOverlap(here, 8, 5);
						newBlock.insertLarge(offset(0, 2));
						newBlock.insertLarge(offset(4, 2));
						newBlock.insertSmall(here);
						newBlock.insertMedium(offset(2, 0));
						newBlock.insertMedium(offset(5, 0));
						allBlocks.push_back(newBlock);
					}
				}
			}
			else if (race == Races::Terran) {
				Block newBlock(6, 5, here);
				addOverlap(here, 6, 5);
				newBlock.insertLarge(here);
				newBlock.insertSmall(offset(4, 1));
				newBlock.insertMedium(offset(0, 3));
				newBlock.insertMedium(offset(3, 3));
				allBlocks.push_back(newBlock);
			}
		}
		void insertStartBlock(Player player, const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
		{
			insertStartBlock(player->getRace(), here, mirrorHorizontal, mirrorVertical);
		}
		void insertStartBlock(const TilePosition here, const bool mirrorHorizontal, const bool mirrorVertical)
		{
			insertStartBlock(Broodwar->self(), here, mirrorHorizontal, mirrorVertical);
		}

		void findStartBlock(Race race)
		{
			bool v;
			auto h = (v = false);

			TilePosition tileBest = TilePositions::Invalid;
			auto distBest = DBL_MAX;
			auto start = (Map::getMainTile() + (Map::getMainChoke() ? (TilePosition)Map::getMainChoke()->Center() : Map::getMainTile())) / 2;

			for (auto x = start.x - 6; x <= start.x + 10; x++) {
				for (auto y = start.y - 6; y <= start.y + 10; y++) {
					TilePosition tile(x, y);

					if (!tile.isValid() || mapBWEM.GetArea(tile) != Map::getMainArea())
						continue;

					auto blockCenter = Position(tile) + Position(128, 80);
					const auto dist = blockCenter.getDistance(Map::getMainPosition()) + log(blockCenter.getDistance((Position)Map::getMainChoke()->Center()));
					if (dist < distBest && ((race == Races::Protoss && canAddBlock(tile, 8, 5)) || (race == Races::Terran && canAddBlock(tile, 6, 5)) || (race == Races::Zerg && canAddBlock(tile, 8, 5)))) {
						tileBest = tile;
						distBest = dist;

						h = (blockCenter.x < Map::getMainPosition().x);
						v = (blockCenter.y < Map::getMainPosition().y);
					}
				}
			}

			// HACK: Fix for transistor, check natural area too
			if (Map::getMainChoke() == Map::getNaturalChoke()) {
				for (auto x = Map::getNaturalTile().x - 9; x <= Map::getNaturalTile().x + 6; x++) {
					for (auto y = Map::getNaturalTile().y - 6; y <= Map::getNaturalTile().y + 5; y++) {
						TilePosition tile(x, y);

						if (!tile.isValid())
							continue;

						auto blockCenter = Position(tile) + Position(128, 80);
						const auto dist = blockCenter.getDistance(Map::getMainPosition()) + blockCenter.getDistance(Position(Map::getMainChoke()->Center()));
						if (dist < distBest && ((race == Races::Protoss && canAddBlock(tile, 8, 5)) || (race == Races::Terran && canAddBlock(tile, 6, 5)))) {
							tileBest = tile;
							distBest = dist;

							h = (blockCenter.x < Map::getMainPosition().x);
							v = (blockCenter.y < Map::getMainPosition().y);
						}
					}
				}
			}

			// HACK: Fix for plasma, if we don't have a valid one, rotate and try a less efficient vertical one
			if (!tileBest.isValid()) {
				for (auto x = Map::getMainTile().x - 16; x <= Map::getMainTile().x + 20; x++) {
					for (auto y = Map::getMainTile().y - 16; y <= Map::getMainTile().y + 20; y++) {
						TilePosition tile(x, y);

						if (!tile.isValid() || mapBWEM.GetArea(tile) != Map::getMainArea())
							continue;

						auto blockCenter = Position(tile) + Position(80, 128);
						const auto dist = blockCenter.getDistance(Map::getMainPosition());
						if (dist < distBest && race == Races::Protoss && canAddBlock(tile, 5, 8)) {
							tileBest = tile;
							distBest = dist;
						}
					}
				}
				if (tileBest.isValid()) {
					Block newBlock(5, 8, tileBest);
					addOverlap(tileBest, 5, 8);
					newBlock.insertLarge(tileBest);
					newBlock.insertLarge(tileBest + TilePosition(0, 5));
					newBlock.insertSmall(tileBest + TilePosition(0, 3));
					newBlock.insertMedium(tileBest + TilePosition(2, 3));
					allBlocks.push_back(newBlock);
				}
			}

			else if (tileBest.isValid())
				insertStartBlock(race, tileBest, h, v);


			// HACK: Added a block that allows a good shield battery placement
			start = (TilePosition)Map::getMainChoke()->Center();
			distBest = DBL_MAX;
			for (auto x = start.x - 12; x <= start.x + 16; x++) {
				for (auto y = start.y - 12; y <= start.y + 16; y++) {
					TilePosition tile(x, y);

					if (!tile.isValid() || mapBWEM.GetArea(tile) != Map::getMainArea())
						continue;

					auto blockCenter = Position(tile) + Position(80, 32);
					const auto dist = (blockCenter.getDistance((Position)Map::getMainChoke()->Center()));
					if (dist < distBest && canAddBlock(tile, 5, 2)) {
						tileBest = tile;
						distBest = dist;

						h = (blockCenter.x < Map::getMainPosition().x);
						v = (blockCenter.y < Map::getMainPosition().y);
					}
				}
			}
			insertBlock(race, tileBest, 5, 2);
		}
		void findStartBlock(Player player)
		{
			findStartBlock(player->getRace());
		}
		void findStartBlock()
		{
			findStartBlock(Broodwar->self());
		}
	}

	void eraseBlock(const TilePosition here)
	{
		for (auto it = allBlocks.begin(); it != allBlocks.end(); ++it) {
			auto&  block = *it;
			if (here.x >= block.Location().x && here.x < block.Location().x + block.width() && here.y >= block.Location().y && here.y < block.Location().y + block.height()) {
				allBlocks.erase(it);
				return;
			}
		}
	}

	void findBlocks()
	{
		findBlocks(Broodwar->self());
	}
	void findBlocks(Player player)
	{
		findBlocks(player->getRace());
	}
	void findBlocks(Race race)
	{
		findStartBlock(race);
		vector<int> heights, widths;
		multimap<double, TilePosition> tilesByPathDist;

		// Calculate distance for each tile to our natural choke, we want to place bigger blocks closer to the chokes
		for (int x = 0; x < Broodwar->mapWidth(); x++) {
			for (int y = 0; y < Broodwar->mapHeight(); y++) {
				TilePosition t(x, y);
				Position p(t);
				if (t.isValid() && Broodwar->isBuildable(t)) {
					double dist = Map::getNaturalChoke() ? p.getDistance(Position(Map::getNaturalChoke()->Center())) : p.getDistance(Map::getMainPosition());
					tilesByPathDist.insert(make_pair(dist, t));
				}
			}
		}

		// Setup block sizes per race
		if (race == Races::Protoss) {
			heights.insert(heights.end(), { 2, 4, 5, 6, 8 });
			widths.insert(widths.end(), { 4, 5, 8, 10, 18 });
		}
		else if (race == Races::Terran) {
			heights.insert(heights.end(), { 2, 4, 5, 6 });
			widths.insert(widths.end(), { 3, 6, 10 });
		}
		else if (race == Races::Zerg) {
			heights.insert(heights.end(), { 2, 4, 5 });
			widths.insert(widths.end(), { 2, 3, 8 });
		}

		// Iterate every tile
		for (int i = 20; i > 0; i--) {
			for (int j = 20; j > 0; j--) {

				if (find(heights.begin(), heights.end(), j) == heights.end() || find(widths.begin(), widths.end(), i) == widths.end())
					continue;

				for (auto &t : tilesByPathDist) {
					TilePosition tile(t.second);
					if (canAddBlock(tile, i, j)) {
						insertBlock(race, tile, i, j);
					}
				}
			}
		}
	}

	vector<Block>& getBlocks() {
		return allBlocks;
	}

	const Block* getClosestBlock(TilePosition here)
	{
		double distBest = DBL_MAX;
		const Block* bestBlock = nullptr;
		for (auto &block : allBlocks) {
			const auto tile = block.Location() + TilePosition(block.width() / 2, block.height() / 2);
			const auto dist = here.getDistance(tile);

			if (dist < distBest) {
				distBest = dist;
				bestBlock = &block;
			}
		}
		return bestBlock;
	}
}