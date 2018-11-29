#include "Wall.h"
#include "BWEB.h"
#include "PathFind.h"

using namespace std;
using namespace BWAPI;

namespace BWEB::Walls
{
	namespace {
		vector<Wall> walls;

		bool isWallTight(Wall&, UnitType, TilePosition);
		bool isPoweringWall(Wall&, TilePosition);
		bool iteratePieces(Wall&);
		void findCurrentHole(Wall&, bool);

		TilePosition initialStart, initialEnd;
		void initializePathPoints(Wall&);
		void checkPathPoints(Wall&);		

		double bestWallScore = 0.0;
		TilePosition currentHole, startTile, endTile;
		vector<TilePosition> currentPath;
		vector<UnitType>::iterator typeIterator;
		map<TilePosition, UnitType> bestWall;
		map<TilePosition, UnitType> currentWall;
		double currentPathSize{};

		// Information that is passed in
		UnitType tight;
		bool reservePath{};
		bool requireTight;
		int chokeWidth;
		Position wallBase;
		vector<TilePosition> chokeTiles;
		
		bool iteratePieces(Wall& wall)
		{
			TilePosition start(wall.getChokePoint()->Center());
			bool movedStart = false;

			const auto closestChokeTile = [&](Position here) {
				double best = DBL_MAX;
				Position posBest;
				for (auto &tile : chokeTiles) {
					Position p = Position(tile) + Position(16, 16);
					if (p.getDistance(here) < best) {
						posBest = p;
						best = p.getDistance(here);
					}
				}
				return posBest;
			};

			const auto checkStart = [&] {
				while (!Broodwar->isBuildable(start)) {
					double distBest = DBL_MAX;
					TilePosition test = start;
					for (int x = test.x - 1; x <= test.x + 1; x++) {
						for (int y = test.y - 1; y <= test.y + 1; y++) {
							TilePosition t(x, y);
							if (!t.isValid())
								continue;

							double dist = Position(t).getDistance((Position)wall.getArea()->Top());

							if (dist < distBest) {
								distBest = dist;
								start = t;
								movedStart = true;
							}
						}
					}
				}
			};

			const auto sortPieces = [&] {

				// Sort functionality for Pylons by Hannes
				if (find(wall.getRawBuildings().begin(), wall.getRawBuildings().end(), UnitTypes::Protoss_Pylon) != wall.getRawBuildings().end()) {
					sort(wall.getRawBuildings().begin(), wall.getRawBuildings().end(), [](UnitType l, UnitType r) { return (l == UnitTypes::Protoss_Pylon) < (r == UnitTypes::Protoss_Pylon); }); // Moves pylons to end
					sort(wall.getRawBuildings().begin(), find(wall.getRawBuildings().begin(), wall.getRawBuildings().end(), UnitTypes::Protoss_Pylon)); // Sorts everything before pylons
				}
				else
					sort(wall.getRawBuildings().begin(), wall.getRawBuildings().end());
			};

			const auto scoreWall = [&] {

				// Find current hole, not including overlap
				findCurrentHole(wall, !reservePath);
				Position chokeCenter(wall.getChokePoint()->Center());
				double dist = 1.0;

				// For walls that require a reserved path, we must have a hole
				if (reservePath && currentHole == TilePositions::None)
					return;
				if (!reservePath && currentHole != TilePositions::None)
					return;

				for (auto &piece : currentWall) {
					auto tile = piece.first;
					auto type = piece.second;
					auto center = Position(tile) + Position(type.tileWidth() * 16, type.tileHeight() * 16);
					auto chokeDist = Position(closestChokeTile(center)).getDistance(center);

					if (type == UnitTypes::Protoss_Pylon)
						dist += 1.0 / exp(chokeDist);
					else
						dist += chokeDist;
				}

				// Score wall based on path sizes and distances
				const auto score = !reservePath ? dist : currentHole.getDistance(startTile) * currentPathSize / (dist);

				if (score > bestWallScore) {
					bestWall = currentWall;
					bestWallScore = score;
				}
			};

			const auto testPiece = [&](Wall& wall, TilePosition t) {
				UnitType currentType = *typeIterator;
				Position c = Position(t) + Position(currentType.tileSize());
				auto tile = closestChokeTile(c);

				if ((currentType == UnitTypes::Protoss_Pylon && !isPoweringWall(wall, t))
					|| Map::overlapsCurrentWall(currentWall, t, currentType.tileWidth(), currentType.tileHeight()) != UnitTypes::None
					|| Map::isOverlapping(t, currentType.tileWidth(), currentType.tileHeight(), true)
					|| !Map::isPlaceable(currentType, t)
					|| Map::tilesWithinArea(wall.getArea(), t, currentType.tileWidth(), currentType.tileHeight()) <= 2)
					return false;
				return true;
			};

			// Choke angle
			auto line = Map::lineOfBestFit(wall.getChokePoint());
			auto p1 = line.first;
			auto p2 = line.second;
			double dy = abs(double(p1.y - p2.y));
			double dx = abs(double(p1.x - p2.x));
			auto angle1 = dx > 0.0 ? atan(dy / dx) * 180.0 / 3.14 : 90.0;

			function<void(TilePosition)> recursiveCheck;
			recursiveCheck = [&](TilePosition start) -> void {
				UnitType type = *typeIterator;
				for (auto x = start.x - 10; x < start.x + 10; x++) {
					for (auto y = start.y - 10; y < start.y + 10; y++) {
						const TilePosition t(x, y);
						auto center = Position(t) + Position(type.tileWidth() * 16, type.tileHeight() * 16);

						if (!t.isValid())
							continue;

						// We want to ensure the buildings are being placed at the correct angle compared to the chokepoint, within some tolerance
						double angle2 = 0.0;
						if (currentWall.size() == 1 && type.getRace() == Races::Protoss && !movedStart) {
							for (auto piece : currentWall) {
								auto tileB = piece.first;
								auto typeB = piece.second;
								auto centerB = Position(tileB) + Position(typeB.tileWidth() * 16, typeB.tileHeight() * 16);
								double dy = abs(double(centerB.y - center.y));
								double dx = abs(double(centerB.x - center.x));

								angle2 = dx > 0.0 ? atan(dy / dx) * 180.0 / 3.14 : 90.0;
							}

							if (abs(abs(angle1) - abs(angle2)) > 15.0)
								continue;
						}

						// If the piece is fine to place
						if (testPiece(wall, t) && (isWallTight(wall, type, t) || type == UnitTypes::Protoss_Pylon)) {

							// 1) Store the current type, increase the iterator
							currentWall[t] = type;
							typeIterator++;

							// 2) If at the end, score the wall, else, go another layer deeper
							if (typeIterator == wall.getRawBuildings().end())
								scoreWall();
							else
								recursiveCheck(start);

							// 3) Erase this current placement and repeat
							if (typeIterator != wall.getRawBuildings().begin())
								typeIterator--;
							currentWall.erase(t);
						}
					}
				}
			};

			// Sort all the pieces and iterate over them to find the best wall
			sortPieces();
			checkStart();
			do {
				currentWall.clear();
				typeIterator = wall.getRawBuildings().begin();
				recursiveCheck(start);
			} while (next_permutation(wall.getRawBuildings().begin(), find(wall.getRawBuildings().begin(), wall.getRawBuildings().end(), UnitTypes::Protoss_Pylon)));
			return !bestWall.empty();
		}		

		bool isPoweringWall(Wall& wall, const TilePosition here)
		{
			for (auto &piece : currentWall) {
				const auto tile(piece.first);
				auto type(piece.second);
				if (type.tileWidth() == 4) {
					auto powersThis = false;
					if (tile.y - here.y == -5 || tile.y - here.y == 4) {
						if (tile.x - here.x >= -4 && tile.x - here.x <= 1) powersThis = true;
					}
					if (tile.y - here.y == -4 || tile.y - here.y == 3) {
						if (tile.x - here.x >= -7 && tile.x - here.x <= 4) powersThis = true;
					}
					if (tile.y - here.y == -3 || tile.y - here.y == 2) {
						if (tile.x - here.x >= -8 && tile.x - here.x <= 5) powersThis = true;
					}
					if (tile.y - here.y >= -2 && tile.y - here.y <= 1) {
						if (tile.x - here.x >= -8 && tile.x - here.x <= 6) powersThis = true;
					}
					if (!powersThis) return false;
				}
				else {
					auto powersThis = false;
					if (tile.y - here.y == 4) {
						if (tile.x - here.x >= -3 && tile.x - here.x <= 2) powersThis = true;
					}
					if (tile.y - here.y == -4 || tile.y - here.y == 3) {
						if (tile.x - here.x >= -6 && tile.x - here.x <= 5) powersThis = true;
					}
					if (tile.y - here.y >= -3 && tile.y - here.y <= 2) {
						if (tile.x - here.x >= -7 && tile.x - here.x <= 6) powersThis = true;
					}
					if (!powersThis) return false;
				}
			}
			return true;
		}

		bool isWallTight(Wall& wall, UnitType building, const TilePosition here)
		{
			// Some constants
			const auto height = building.tileHeight() * 4;
			const auto width = building.tileWidth() * 4;
			const auto vertTight = (tight == UnitTypes::None) ? 32 : tight.height();
			const auto horizTight = (tight == UnitTypes::None) ? 32 : tight.width();

			// Checks each side of the building to see if it is valid for walling purposes
			const auto checkLeft = (building.tileWidth() * 16) - building.dimensionLeft() < horizTight;
			const auto checkRight = (building.tileWidth() * 16) - building.dimensionRight() - 1 < horizTight;
			const auto checkUp =  (building.tileHeight() * 16) - building.dimensionUp() < vertTight;
			const auto checkDown =  (building.tileHeight() * 16) - building.dimensionDown() - 1 < vertTight;

			// HACK: I don't have a great method for buildings that can check multiple tiles for Terrain tight, hardcoded a few as shown below
			const auto right = WalkPosition(here) + WalkPosition(width, 0) + WalkPosition(building == UnitTypes::Terran_Barracks, 0);
			const auto left = WalkPosition(here) - WalkPosition(1, 0);
			const auto up = WalkPosition(here) - WalkPosition(0, 1);
			const auto down = WalkPosition(here) + WalkPosition(0, height) + WalkPosition(0, building == UnitTypes::Protoss_Gateway || building == UnitTypes::Terran_Supply_Depot);

			// Some testing parameters
			auto firstBuilding = currentWall.size() == 0;
			auto lastBuilding = currentWall.size() == 2;
			auto terrainTight = false;
			auto parentTight = false;

			// Functions for each dimension check
			function <int(UnitType, UnitType)>fRight = [](UnitType building, UnitType parent) -> int {
				return (parent.tileWidth() * 16 - parent.dimensionLeft()) + (building.tileWidth() * 16 - building.dimensionRight() - 1);
			};
			function <int(UnitType, UnitType)>fLeft = [](UnitType building, UnitType parent) -> int {
				return (parent.tileWidth() * 16 - parent.dimensionRight() - 1) + (building.tileWidth() * 16 - building.dimensionLeft());
			};
			function <int(UnitType, UnitType)>fUp = [](UnitType building, UnitType parent) -> int {
				return (parent.tileHeight() * 16 - parent.dimensionDown() - 1) + (building.tileHeight() * 16 - building.dimensionUp());
			};
			function <int(UnitType, UnitType)>fDown = [](UnitType building, UnitType parent) -> int {
				return (parent.tileHeight() * 16 - parent.dimensionUp()) + (building.tileHeight() * 16 - building.dimensionDown() - 1);
			};

			// Function to check if it's terrain tight here
			const auto terrainTightCheck = [&](WalkPosition w, bool check) {
				TilePosition t(w);

				// If the walkposition is invalid or unwalkable
				if (tight != UnitTypes::None && check && (!w.isValid() || !Broodwar->isWalkable(w)))
					return true;

				// If the tile is touching some resources
				if (Map::isOverlapping(t))
					return true;

				// If we don't care about walling tight and the tile isn't walkable
				if (!requireTight && !Map::isWalkable(t))
					return true;
				return false;
			};

			// Functions to iterate each WalkPosition of a side
			const auto checkVerticalSide = [&](WalkPosition start, int length, bool check, function <int(UnitType, UnitType)> fDiff, int tightnessFactor) {
				for (auto x = start.x; x < start.x + length; x++) {
					WalkPosition w(x, start.y);
					TilePosition t(w);
					auto parent = Map::overlapsCurrentWall(currentWall, t);
					auto parentTightCheck = parent != UnitTypes::None ? fDiff(building, parent) < tightnessFactor : false;

					// Check if it's tight with the terrain
					if (!terrainTight && terrainTightCheck(w, check))
						terrainTight = true;

					// Check if it's tight with a parent
					if (!parentTight && parentTightCheck)
						parentTight = true;
				}
				return;
			};

			const auto checkHorizontalSide = [&](WalkPosition start, int length, bool check, function <int(UnitType, UnitType)> fDiff, int tightnessFactor) {
				for (auto y = start.y; y < start.y + length; y++) {
					WalkPosition w(start.x, y);
					TilePosition t(w);
					auto parent = Map::overlapsCurrentWall(currentWall, t);
					auto parentTightCheck = parent != UnitTypes::None ? fDiff(building, parent) < tightnessFactor : false;

					// Check if it's tight with the terrain
					if (!terrainTight && terrainTightCheck(w, check))
						terrainTight = true;

					// Check if it's tight with a parent
					if (!parentTight && parentTightCheck)
						parentTight = true;
				}
				return;
			};

			/* What this does:
			1) For each side, check if it's terrain tight
			2) For each side, check if it's tight with any parent buildings beside it
			3) If we want a tight wall, check that the final piece is terrain tight and parent tight*/
			checkVerticalSide(up, width, checkUp, fUp, vertTight);
			checkVerticalSide(down, width, checkDown, fDown, vertTight);
			checkHorizontalSide(left, height, checkLeft, fLeft, horizTight);
			checkHorizontalSide(right, height, checkRight, fRight, horizTight);

			// If we don't want a reserve path, we need all buildings to be tight at the tightness resolution...
			if (!reservePath) {
				if (!lastBuilding && !firstBuilding)	// ...to the parent
					return parentTight;
				if (firstBuilding)						// ...to the terrain
					return terrainTight;
				if (lastBuilding)						// ...to the parent and terrain
					return (terrainTight && parentTight);
			}

			// If we do want a reserve path, we need this building to be tight at tile resolution to a parent or terrain
			else if (reservePath)
				return (terrainTight || parentTight);
			return false;
		}

		void checkPathPoints(Wall& wall)
		{
			auto distBest = DBL_MAX;
			startTile = initialStart;
			endTile = initialEnd;

			if (!Map::isWalkable(initialStart) || Map::overlapsCurrentWall(currentWall, initialStart) != UnitTypes::None || Map::isOverlapping(endTile) != 0) {
				for (auto x = initialStart.x - 2; x < initialStart.x + 2; x++) {
					for (auto y = initialStart.y - 2; y < initialStart.y + 2; y++) {
						TilePosition t(x, y);
						const auto dist = t.getDistance(endTile);
						if (Map::overlapsCurrentWall(currentWall, t) != UnitTypes::None || !Map::isWalkable(t))
							continue;

						if (Map::mapBWEM.GetArea(t) == wall.getArea() && dist < distBest) {
							startTile = t;
							distBest = dist;
						}
					}
				}
			}

			distBest = 0.0;
			if (!Map::isWalkable(initialEnd) || Map::overlapsCurrentWall(currentWall, initialEnd) != UnitTypes::None || Map::isOverlapping(endTile) != 0) {
				for (auto x = initialEnd.x - 4; x < initialEnd.x + 4; x++) {
					for (auto y = initialEnd.y - 4; y < initialEnd.y + 4; y++) {
						TilePosition t(x, y);
						const auto dist = t.getDistance(startTile);
						if (Map::overlapsCurrentWall(currentWall, t) != UnitTypes::None || !Map::isWalkable(t))
							continue;

						if (Map::mapBWEM.GetArea(t) && dist > distBest) {
							endTile = t;
							distBest = dist;
						}
					}
				}
			}
		}

		void initializePathPoints(Wall& wall)
		{
			auto choke = wall.getChokePoint();
			auto line = Map::lineOfBestFit(wall.getChokePoint());
			Position n1 = line.first;
			Position n2 = line.second;
			auto dx1 = n2.x - n1.x;
			auto dy1 = n2.y - n1.y;
			auto dx2 = n1.x - n2.x;
			auto dy2 = n1.y - n2.y;
			Position direction1 = Position(-dy1 / 2, dx1 / 2) + Position(choke->Center());
			Position direction2 = Position(-dy2 / 2, dx2 / 2) + Position(choke->Center());
			Position trueDirection = direction1.getDistance(Map::mapBWEM.Center()) < direction2.getDistance(Map::mapBWEM.Center()) ? direction1 : direction2;

			if (choke == Map::getNaturalChoke()) {
				initialStart = TilePosition(Map::getMainChoke()->Center());
				initialEnd = (TilePosition)trueDirection;
			}
			else if (choke == Map::getMainChoke()) {
				initialStart = (Map::getMainTile() + TilePosition(Map::getMainChoke()->Center())) / 2;
				initialEnd = (TilePosition)Map::getNaturalChoke()->Center();
			}
			else {
				initialStart = TilePosition(wall.getArea()->Top());
				initialEnd = (TilePosition)trueDirection;
			}

			startTile = initialStart;
			endTile = initialEnd;			
		}

		void findCurrentHole(Wall& wall, bool ignoreOverlap)
		{
			// Check that the path points are possible to reach
			checkPathPoints(wall);
			Position startCenter = Position(startTile) + Position(16, 16);
			Position endCenter = Position(endTile) + Position(16, 16);

			// Get a new path
			BWEB::PathFinding::Path newPath;
			newPath.createWallPath(Map::mapBWEM, currentWall, startCenter, endCenter, ignoreOverlap);
			currentHole = TilePositions::None;
			currentPath = newPath.getTiles();

			// Quick check to see if the path contains our end point, if not then the path never reached the end
			if (find(currentPath.begin(), currentPath.end(), endTile) == currentPath.end()) {
				currentHole = TilePositions::None;
				currentPathSize = DBL_MAX;
			}

			// Otherwise iterate all tiles and locate the hole
			else {
				for (auto &tile : currentPath) {
					double closestGeo = DBL_MAX;
					for (auto &geo : wall.getChokePoint()->Geometry()) {
						if (TilePosition(geo) == tile && tile.getDistance(startTile) < closestGeo && Map::overlapsCurrentWall(currentWall, tile) == UnitTypes::None) {
							currentHole = tile;
							closestGeo = tile.getDistance(startTile);
						}
					}
				}
				currentPathSize = newPath.getDistance();
			}
		}
	}

	void addToWall(UnitType building, Wall& wall, UnitType tight)
	{
		auto distance = DBL_MAX;
		auto tileBest = TilePositions::Invalid;
		auto start = TilePosition(wall.getCentroid());
		auto centroidDist = wall.getCentroid().getDistance(Position(endTile));
		auto end = Position(endTile);
		auto doorCenter = Position(wall.getDoor()) + Position(16, 16);

		auto furthest = 0.0;
		// Find the furthest non pylon building to the chokepoint
		for (auto &tile : wall.largeTiles()) {
			Position p = Position(tile) + Position(64, 48);
			auto dist = p.getDistance((Position)wall.getChokePoint()->Center());
			if (dist > furthest)
				furthest = dist;
		}
		for (auto &tile : wall.mediumTiles()) {
			Position p = Position(tile) + Position(48, 32);
			auto dist = p.getDistance((Position)wall.getChokePoint()->Center());
			if (dist > furthest)
				furthest = dist;
		}

		// Iterate around wall centroid to find a suitable position
		for (auto x = start.x - 8; x <= start.x + 8; x++) {
			for (auto y = start.y - 8; y <= start.y + 8; y++) {
				const TilePosition t(x, y);
				const auto center = (Position(t) + Position(32, 32));

				if (!t.isValid()
					|| Map::isOverlapping(t, building.tileWidth(), building.tileHeight())
					|| !Map::isPlaceable(building, t)
					|| Map::tilesWithinArea(wall.getArea(), t, 2, 2) == 0
					|| (building == UnitTypes::Protoss_Photon_Cannon && (center.getDistance((Position)wall.getChokePoint()->Center()) < furthest || center.getDistance(doorCenter) < 96.0)))
					continue;

				const auto dist = center.getDistance(doorCenter);

				if (dist < distance)
					tileBest = TilePosition(x, y), distance = dist;
			}
		}

		// If tile is valid, add to wall
		if (tileBest.isValid()) {
			currentWall[tileBest] = building;
			wall.insertDefense(tileBest);
			Map::addOverlap(tileBest, 2, 2);
		}
	}
		
	void createWall(vector<UnitType>& buildings, const BWEM::Area * area, const BWEM::ChokePoint * choke, const UnitType t, const vector<UnitType>& defenses, const bool rp, const bool rt)
	{
		if (!area) {
			Broodwar << "BWEB: Can't initialize a wall without a valid BWEM::Area" << endl;
			return;
		}

		if (!choke) {
			Broodwar << "BWEB: Can't initialize a wall without a valid BWEM::Chokepoint" << endl;
			return;
		}

		if (buildings.empty()) {
			Broodwar << "BWEB: Can't initialize a wall with an empty vector of UnitTypes." << endl;
			return;
		}

		// Create a new wall object
		Wall wall(area, choke, buildings, defenses);

		// I got sick of passing the parameters everywhere, sue me
		tight = t;
		reservePath = rp;
		requireTight = rt;

		// Create a line of tiles that make up the geometry of the choke		
		for (auto &walk : wall.getChokePoint()->Geometry()) {
			TilePosition t(walk);
			if (Broodwar->isBuildable(t))
				chokeTiles.push_back(t);
		}

		const auto addDoor = [&] {
			// Check which tile is closest to each part on the path, set as door
			double distBest = DBL_MAX;
			for (auto chokeTile : chokeTiles) {
				for (auto pathTile : currentPath) {
					double dist = chokeTile.getDistance(pathTile);
					if (dist < distBest) {
						distBest = dist;
						wall.setWallDoor(chokeTile);
					}
				}
			}

			// If the line of tiles we made is empty, assign closest path tile to closest to wall centroid
			if (chokeTiles.empty()) {
				for (auto pathTile : currentPath) {
					Position p(pathTile);
					double dist = wall.getCentroid().getDistance(p);
					if (dist < distBest) {
						distBest = dist;
						wall.setWallDoor(pathTile);
					}
				}
			}
		};

		// Set the walls reserve path if needed
		const auto addReservePath = [&] {
			if (reservePath) {
				for (auto &tile : currentPath)
					Map::addReserve(tile, 1, 1);
			}
		};

		// Set the walls centroid
		const auto addCentroid = [&] {
			Position centroid;
			int sizeWall = currentWall.size();
			for (auto &piece : currentWall) {
				if (piece.second != UnitTypes::Protoss_Pylon)
					centroid += static_cast<Position>(piece.first) + static_cast<Position>(piece.second.tileSize()) / 2;
				else
					sizeWall--;
			}
			wall.setCentroid(centroid / sizeWall);
		};

		// Add wall defenses if requested
		const auto addDefenses = [&] {
			for (auto &building : wall.getRawDefenses())
				addToWall(building, wall, tight);
		};

		// HACK: reset these values so I can make multiple walls easily
		bestWallScore = 0.0;
		currentHole = TilePositions::None;
		startTile = TilePositions::None;
		endTile = TilePositions::None;
		currentPath.clear();
		bestWall.clear();
		currentWall.clear();

		double distBest = DBL_MAX;
		for (auto &base : wall.getArea()->Bases()) {
			double dist = base.Center().getDistance((Position)choke->Center());
			if (dist < distBest)
				distBest = dist, wallBase = Position(base.Location()) + Position(64, 48);
		}

		chokeWidth = int(choke->Pos(choke->end1).getDistance(choke->Pos(choke->end2)) / 4);
		chokeWidth = max(6, chokeWidth);

		// Setup pathing parameters
		initializePathPoints(wall);

		// Iterate pieces, try to find best location
		if (iteratePieces(wall)) {
			for (auto &placement : bestWall) {
				wall.insertSegment(placement.first, placement.second);
				Map::addOverlap(placement.first, placement.second.tileWidth(), placement.second.tileHeight());
			}

			currentWall = bestWall;
			findCurrentHole(wall, false);

			if (requireTight && currentHole.isValid())
				Broodwar << "BWEB: Unable to find a tight wall at the given location. Placed best one possible." << endl;

			addReservePath();
			addCentroid();
			addDoor();
			addDefenses();

			// Push wall into the vector
			walls.push_back(wall);
		}
	}
	
	const Wall * getClosestWall(TilePosition here)
	{
		double distBest = DBL_MAX;
		const Wall * bestWall = nullptr;
		for (auto &wall : walls) {
			const auto dist = here.getDistance(static_cast<TilePosition>(wall.getChokePoint()->Center()));

			if (dist < distBest)
				distBest = dist, bestWall = &wall;

		}
		return bestWall;
	}

	vector<Wall> & getWalls() {
		return walls;
	}

	Wall* getWall(const BWEM::Area * area, const BWEM::ChokePoint * choke)
	{
		if (!area && !choke)
			return nullptr;

		for (auto &wall : walls) {
			if ((!area || wall.getArea() == area) && (!choke || wall.getChokePoint() == choke))
				return &wall;
		}
		return nullptr;
	}

	void draw()
	{
		Broodwar->drawTextMap(Position(endTile), "%d", Position(endTile));
		Broodwar->drawTextMap(Position(startTile), "%d", Position(startTile));

		/*map<TilePosition, UnitType> test;
		BWEB::PathFinding::Path newPath;
		newPath.createWallPath(Map::mapBWEM, test, Position(startTile), Position(endTile), false);

		for (auto &tile : newPath.getTiles()) {
			Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(33, 33), Colors::Red);
		}*/

		//for (int x = 0; x < Broodwar->mapWidth(); x++) {
		//	for (int y = 0; y < Broodwar->mapHeight(); y++) {
		//		TilePosition tile(x, y);
		//		if (Map::isOverlapping(tile))
		//			Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(33, 33), Colors::Red);
		//	}
		//}
	}
}
