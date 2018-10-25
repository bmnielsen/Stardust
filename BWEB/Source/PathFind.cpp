#include "BWEB.h"

using namespace std::placeholders;

namespace BWEB
{
	void Path::createWallPath(BWEB::Map& mapBWEB, BWEM::Map& mapBWEM, const TilePosition source, const TilePosition target, bool ignoreOverlap)
	{
		try {
			if (source == target) return;

			auto maxDist = source.getDistance(target);

			vector<const BWEM::Area *> bwemAreas;

			const auto validArea = [&](const TilePosition tile) {
				if (!mapBWEM.GetArea(tile) || find(bwemAreas.begin(), bwemAreas.end(), mapBWEM.GetArea(tile)) != bwemAreas.end())
					return true;
				return false;
			};

			const auto collision = [&](const TilePosition tile) {
				return !tile.isValid()
					|| (!ignoreOverlap && mapBWEB.overlapGrid[tile.x][tile.y] > 0)
					|| !mapBWEB.isWalkable(tile)
					|| mapBWEB.usedGrid[tile.x][tile.y] > 0
					|| mapBWEB.overlapsCurrentWall(tile) != UnitTypes::None
					|| tile.getDistance(target) > maxDist * 1.2;
			};

			TilePosition parentGrid[256][256];

			// This function requires that parentGrid has been filled in for a path from source to target
			const auto createPath = [&]() {
				tiles.push_back(target);
				TilePosition check = parentGrid[target.x][target.y];
				dist += Position(target).getDistance(Position(check));

				do {
					tiles.push_back(check);
					TilePosition prev = check;
					check = parentGrid[check.x][check.y];
					dist += Position(prev).getDistance(Position(check));
				} while (check != source);
				return;
			};

			auto const direction = []() {
				vector<TilePosition> vec{ { 0, 1 },{ 1, 0 },{ -1, 0 },{ 0, -1 } };
				return vec;
			}();

			for (auto &choke : mapBWEM.GetPath((Position)source, (Position)target)) {
				bwemAreas.push_back(choke->GetAreas().first);
				bwemAreas.push_back(choke->GetAreas().second);
			}
			bwemAreas.push_back(mapBWEM.GetArea(source));
			bwemAreas.push_back(mapBWEM.GetArea(target));

			std::queue<BWAPI::TilePosition> nodeQueue;
			nodeQueue.emplace(source);
			parentGrid[source.x][source.y] = source;

			// While not empty, pop off top the closest TilePosition to target
			while (!nodeQueue.empty()) {
				auto const tile = nodeQueue.front();
				nodeQueue.pop();

				for (auto const &d : direction) {
					auto const next = tile + d;

					if (next.isValid()) {
						// If next has parent or is a collision, continue
						if (parentGrid[next.x][next.y] != TilePosition(0, 0) || collision(next))
							continue;

						// Set parent here, BFS optimization
						parentGrid[next.x][next.y] = tile;

						// If at target, return path
						if (next == target) {
							createPath();
							return;
						}

						nodeQueue.emplace(next);
					}
				}
			}
		}
		catch (exception ex) {
			return;
		}
	}

	void Path::createUnitPath(BWEB::Map& mapBWEB, BWEM::Map& mapBWEM, const Position s, const Position t) {
		try {
			TilePosition source(s);
			TilePosition target(t);
			auto maxDist = source.getDistance(target);

			if (source == target)
				return;

			vector<const BWEM::Area *> bwemAreas;

			const auto validArea = [&](const TilePosition tile) {
				if (!mapBWEM.GetArea(tile) || find(bwemAreas.begin(), bwemAreas.end(), mapBWEM.GetArea(tile)) != bwemAreas.end())
					return true;
				return false;
			};

			const auto collision = [&](const TilePosition tile) {
				return !tile.isValid()
					|| (!validArea(tile))
					|| !mapBWEB.isWalkable(tile)
					|| (mapBWEB.usedGrid[tile.x][tile.y] > 0)
					|| tile.getDistance(target) > maxDist * 1.2;
			};

			const auto diagonalCollision = [&](const TilePosition tile) {
				return !tile.isValid()
					|| (mapBWEB.usedGrid[tile.x][tile.y] > 0);
			};

			TilePosition parentGrid[256][256];

			// This function requires that parentGrid has been filled in for a path from source to target
			const auto createPath = [&]() {
				tiles.push_back(target);
				TilePosition check = parentGrid[target.x][target.y];
				dist += Position(target).getDistance(Position(check));

				do {
					tiles.push_back(check);
					TilePosition prev = check;
					check = parentGrid[check.x][check.y];
					dist += Position(prev).getDistance(Position(check));
				} while (check != source);

				// HACK: Try to make it more accurate to positions instead of tiles
				auto correctionSource = Position(*(tiles.end() - 1));
				auto correctionTarget = Position(*(tiles.begin() + 1));
				dist += s.getDistance(correctionSource);
				dist += t.getDistance(correctionTarget);
				dist -= 64.0;
			};

			auto const direction = []() {
				vector<TilePosition> vec{ { 0, 1 },{ 1, 0 },{ -1, 0 },{ 0, -1 },{ -1,-1 },{ -1, 1 },{ 1, -1 },{ 1, 1 } };
				return vec;
			}();

			for (auto &choke : mapBWEM.GetPath((Position)source, (Position)target)) {
				bwemAreas.push_back(choke->GetAreas().first);
				bwemAreas.push_back(choke->GetAreas().second);
			}
			bwemAreas.push_back(mapBWEM.GetArea(source));
			bwemAreas.push_back(mapBWEM.GetArea(target));

			std::queue<BWAPI::TilePosition> nodeQueue;
			nodeQueue.emplace(source);
			parentGrid[source.x][source.y] = source;

			// While not empty, pop off top the closest TilePosition to target
			while (!nodeQueue.empty()) {
				auto const tile = nodeQueue.front();
				nodeQueue.pop();

				for (auto const &d : direction) {
					auto const next = tile + d;

					if (next.isValid()) {
						// If next has parent or is a collision, continue
						if (parentGrid[next.x][next.y] != TilePosition(0, 0) || collision(next))
							continue;

						// Check diagonal collisions where necessary
						if ((d.x == 1 || d.x == -1) && (d.y == 1 || d.y == -1) && (diagonalCollision(tile + TilePosition(d.x, 0)) || diagonalCollision(tile + TilePosition(0, d.y))))
							continue;

						// Set parent here, BFS optimization
						parentGrid[next.x][next.y] = tile;

						// If at target, return path
						if (next == target)
							createPath();

						nodeQueue.emplace(next);
					}
				}
			}
		}
		catch (exception ex) {
			return;
		}
	}

	Path::Path()
	{
		tiles ={};
		dist = 0.0;
	}
}