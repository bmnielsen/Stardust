#include "BWEB.h"
#include "Block.h"
#include "Station.h"
#include "Wall.h"
#include "PathFind.h"

using namespace std;
using namespace BWAPI;

namespace BWEB::Map
{
	namespace {
		Position mainPosition, naturalPosition;
		TilePosition mainTile, naturalTile;
		const BWEM::Area * naturalArea{};
		const BWEM::Area * mainArea{};
		const BWEM::ChokePoint * naturalChoke{};
		const BWEM::ChokePoint * mainChoke{};
		std::set<BWAPI::TilePosition> usedTiles;

		int testGrid[256][256];
		int reserveGrid[256][256] ={};
		int overlapGrid[256][256] ={};
		int usedGrid[256][256] ={};

		void findMain()
		{
			mainTile = Broodwar->self()->getStartLocation();
			mainPosition = static_cast<Position>(mainTile) + Position(64, 48);
			mainArea = mapBWEM.GetArea(mainTile);
		}

		void findNatural()
		{
			auto distBest = DBL_MAX;
			for (auto &area : mapBWEM.Areas()) {
				for (auto &base : area.Bases()) {

					// Must have gas, be accesible and at least 5 mineral patches
					if (base.Starting()
						|| base.Geysers().empty()
						|| area.AccessibleNeighbours().empty()
						|| base.Minerals().size() < 5)
						continue;

					const auto dist = getGroundDistance(base.Center(), mainPosition);
					if (dist < distBest) {
						distBest = dist;
						naturalArea = base.GetArea();
						naturalTile = base.Location();
						naturalPosition = static_cast<Position>(naturalTile) + Position(64, 48);
					}
				}
			}
		}

		void findMainChoke()
		{
			// Add all main chokes to a set
			set<BWEM::ChokePoint const *> mainChokes;
			for (auto &choke : mainArea->ChokePoints()) {
				mainChokes.insert(choke);
			}

			// Find a chokepoint that belongs to main and natural
			auto distBest = DBL_MAX;
			if (naturalArea) {
				for (auto &choke : naturalArea->ChokePoints()) {
					const auto dist = getGroundDistance(Position(choke->Center()), mainPosition);
					if (mainChokes.find(choke) != mainChokes.end() && dist < distBest) {
						mainChoke = choke;
						distBest = dist;
					}
				}
			}

			// If we didn't find a main choke that belongs to main and natural, find another one
			if (!mainChoke) {
				for (auto &choke : mapBWEM.GetPath(mainPosition, naturalPosition)) {
					const auto width = choke->Pos(choke->end1).getDistance(choke->Pos(choke->end2));
					if (width < distBest) {
						mainChoke = choke;
						distBest = width;
					}
				}
			}
		}

		void findNaturalChoke()
		{
			// Exception for maps with a natural behind the main such as Crossing Fields
			if (getGroundDistance(mainPosition, mapBWEM.Center()) < getGroundDistance(naturalPosition, mapBWEM.Center())) {
				naturalChoke = mainChoke;
				return;
			}

			set<BWEM::ChokePoint const *> nonChokes;
			for (auto &choke : mapBWEM.GetPath(mainPosition, naturalPosition))
				nonChokes.insert(choke);

			// Find area that shares the choke we need to defend
			auto distBest = DBL_MAX;
			const BWEM::Area* second = nullptr;
			if (naturalArea) {
				for (auto &area : naturalArea->AccessibleNeighbours()) {
					auto center = area->Top();
					const auto dist = Position(center).getDistance(mapBWEM.Center());

					bool wrongArea = false;
					for (auto &choke : area->ChokePoints()) {
						if ((!choke->Blocked() && choke->Pos(choke->end1).getDistance(choke->Pos(choke->end2)) <= 2) || nonChokes.find(choke) != nonChokes.end()) {
							wrongArea = true;
						}
					}
					if (wrongArea)
						continue;

					if (center.isValid() && dist < distBest)
						second = area, distBest = dist;
				}

				// Find second choke based on the connected area
				distBest = DBL_MAX;
				for (auto &choke : naturalArea->ChokePoints()) {
					if (choke->Center() == mainChoke->Center()
						|| choke->Blocked()
						|| choke->Geometry().size() <= 3
						|| (choke->GetAreas().first != second && choke->GetAreas().second != second))
						continue;

					const auto dist = Position(choke->Center()).getDistance(Position(Broodwar->self()->getStartLocation()));
					if (dist < distBest)
						naturalChoke = choke, distBest = dist;
				}
			}
		}

		void findNeutrals()
		{
			// Add overlap for neutrals
			for (auto &unit : Broodwar->neutral()->getUnits()) {
				if (unit && unit->exists() && unit->getType().topSpeed() == 0.0)
					addOverlap(unit->getTilePosition(), unit->getType().tileWidth(), unit->getType().tileHeight());
			}
		}
	}

	void onStart()
	{
		findNeutrals();
		findMain();
		findNatural();
		findMainChoke();
		findNaturalChoke();
		Stations::findStations();
	}

	void onUnitDiscover(const Unit unit)
	{
		if (!unit
			|| !unit->getType().isBuilding()
			|| unit->isFlying()
			|| unit->getType() == UnitTypes::Resource_Vespene_Geyser)
			return;

		const auto tile = unit->getTilePosition();
		const auto type = unit->getType();

		// Add used tiles
		for (auto x = tile.x; x < tile.x + type.tileWidth(); x++) {
			for (auto y = tile.y; y < tile.y + type.tileHeight(); y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					continue;
				usedTiles.insert(t);
				usedGrid[x][y] = 1;
			}
		}

		// Add defense count to stations
		if (type == UnitTypes::Protoss_Photon_Cannon
			|| type == UnitTypes::Zerg_Sunken_Colony
			|| type == UnitTypes::Zerg_Spore_Colony
			|| type == UnitTypes::Terran_Missile_Turret) {

			for (auto &station : Stations::getStations()) {
				int defCnt = station.getDefenseCount();
				for (auto &defense : station.DefenseLocations()) {
					if (unit->getTilePosition() == defense) {
						station.setDefenseCount(defCnt + 1);
						return;
					}
				}
			}
		}
	}

	void onUnitMorph(const Unit unit)
	{
		onUnitDiscover(unit);
	}

	void onUnitDestroy(const Unit unit)
	{
		if (!unit
			|| !unit->getType().isBuilding()
			|| unit->isFlying())
			return;

		const auto tile = unit->getTilePosition();
		const auto type = unit->getType();

		// Remove any used tiles
		for (auto x = tile.x; x < tile.x + type.tileWidth(); x++) {
			for (auto y = tile.y; y < tile.y + type.tileHeight(); y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					continue;
				usedTiles.erase(t);
				usedGrid[x][y] = 0;
			}
		}

		// Remove defense count from stations
		if (type == UnitTypes::Protoss_Photon_Cannon
			|| type == UnitTypes::Zerg_Sunken_Colony
			|| type == UnitTypes::Zerg_Spore_Colony
			|| type == UnitTypes::Terran_Missile_Turret) {

			for (auto &station : Stations::getStations()) {
				int defCnt = station.getDefenseCount();
				for (auto &defense : station.DefenseLocations()) {
					if (unit->getTilePosition() == defense) {
						station.setDefenseCount(defCnt - 1);
						return;
					}
				}
			}
		}
	}

	void draw()
	{
		for (auto &choke : mainArea->ChokePoints()) {
			const auto dist = getGroundDistance(Position(choke->Center()), mainPosition);
			Broodwar->drawTextMap((Position)choke->Center() + Position(0, 16), "%.2f", dist);
		}

		// Draw Blocks 
		for (auto &block : Blocks::getBlocks()) {
			for (auto &tile : block.SmallTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), Broodwar->self()->getColor());
			for (auto &tile : block.MediumTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(97, 65), Broodwar->self()->getColor());
			for (auto &tile : block.LargeTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(129, 97), Broodwar->self()->getColor());
		}

		// Draw Stations
		for (auto &station : Stations::getStations()) {
			for (auto &tile : station.DefenseLocations())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), Broodwar->self()->getColor());
			Broodwar->drawBoxMap(Position(station.BWEMBase()->Location()), Position(station.BWEMBase()->Location()) + Position(129, 97), Broodwar->self()->getColor());
		}

		// Draw Walls
		for (auto &wall : Walls::getWalls()) {
			for (auto &tile : wall.smallTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), Broodwar->self()->getColor());
			for (auto &tile : wall.mediumTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(97, 65), Broodwar->self()->getColor());
			for (auto &tile : wall.largeTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(129, 97), Broodwar->self()->getColor());
			for (auto &tile : wall.getDefenses())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), Broodwar->self()->getColor());
			Broodwar->drawBoxMap(Position(wall.getDoor()), Position(wall.getDoor()) + Position(33, 33), Broodwar->self()->getColor(), true);
			Broodwar->drawCircleMap(Position(wall.getCentroid()) + Position(16, 16), 8, Broodwar->self()->getColor(), true);

			auto p1 = wall.getChokePoint()->Pos(wall.getChokePoint()->end1);
			auto p2 = wall.getChokePoint()->Pos(wall.getChokePoint()->end2);

			Broodwar->drawLineMap(Position(p1), Position(p2), Colors::Green);
		}

		// Draw Reserve Path and some grids
		for (int x = 0; x < Broodwar->mapWidth(); x++) {
			for (int y = 0; y < Broodwar->mapHeight(); y++) {
				TilePosition t(x, y);
				if (reserveGrid[x][y] >= 1)
					Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Black, false);
				if (testGrid[x][y] >= 1)
					Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Red, false);
			}
		}

		BWEB::Walls::draw();
	}

	UnitType overlapsCurrentWall(map<TilePosition, UnitType>& currentWall, const TilePosition here, const int width, const int height)
	{
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				for (auto &placement : currentWall) {
					const auto tile = placement.first;
					if (x >= tile.x && x < tile.x + placement.second.tileWidth() && y >= tile.y && y < tile.y + placement.second.tileHeight())
						return placement.second;
				}
			}
		}
		return UnitTypes::None;
	}

	template <class T>
	double getGroundDistance(T s, T e)
	{
		Position start(s), end(e);
		auto dist = 0.0;
		if (!start.isValid()
			|| !end.isValid()
			|| !mapBWEM.GetArea(WalkPosition(start))
			|| !mapBWEM.GetArea(WalkPosition(end))
			|| !mapBWEM.GetArea(WalkPosition(start))->AccessibleFrom(mapBWEM.GetArea(WalkPosition(end))))
			return DBL_MAX;

		for (auto &cpp : mapBWEM.GetPath(start, end)) {
			auto center = Position(cpp->Center());
			dist += start.getDistance(center);
			start = center;
		}

		return dist += start.getDistance(end);
	}

	double distanceNextChoke(Position start, Position end)
	{
		if (!start.isValid() || !end.isValid())
			return DBL_MAX;

		auto bwemPath = mapBWEM.GetPath(start, end);
		auto source = bwemPath.front();


		BWEB::PathFinding::Path newPath;
		newPath.createUnitPath(mapBWEM, start, (Position)source->Center());
		return newPath.getDistance();
	}

	TilePosition Map::getBuildPosition(UnitType type, const TilePosition searchCenter)
	{
		auto distBest = DBL_MAX;
		auto tileBest = TilePositions::Invalid;

		// Search through each block to find the closest block and valid position
		for (auto &block : Blocks::getBlocks()) {
			set<TilePosition> placements;
			if (type.tileWidth() == 4) placements = block.LargeTiles();
			else if (type.tileWidth() == 3) placements = block.MediumTiles();
			else placements = block.SmallTiles();

			for (auto &tile : placements) {
				const auto dist = tile.getDistance(searchCenter);
				if (dist < distBest && isPlaceable(type, tile))
					distBest = dist, tileBest = tile;
			}
		}
		return tileBest;
	}

	TilePosition Map::getDefBuildPosition(UnitType type, const TilePosition searchCenter)
	{
		auto distBest = DBL_MAX;
		auto tileBest = TilePositions::Invalid;

		// Search through each wall to find the closest valid TilePosition
		for (auto &wall : Walls::getWalls()) {
			for (auto &tile : wall.getDefenses()) {
				const auto dist = tile.getDistance(searchCenter);
				if (dist < distBest && isPlaceable(type, tile))
					distBest = dist, tileBest = tile;
			}
		}

		// Search through each station to find the closest valid TilePosition
		for (auto &station : Stations::getStations()) {
			for (auto &tile : station.DefenseLocations()) {
				const auto dist = tile.getDistance(searchCenter);
				if (dist < distBest && isPlaceable(type, tile))
					distBest = dist, tileBest = tile;
			}
		}
		return tileBest;
	}

	bool isOverlapping(const TilePosition here, const int width, const int height, bool ignoreBlocks)
	{
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					continue;
				if (Map::overlapGrid[x][y] > 0)
					return true;
			}
		}
		return false;
	}

	bool isReserved(const TilePosition here, const int width, const int height)
	{
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					continue;
				if (Map::reserveGrid[x][y] > 0)
					return true;
			}
		}
		return false;
	}

	bool isUsed(const TilePosition here, const int width, const int height)
	{
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					continue;
				if (Map::usedGrid[x][y] > 0)
					return true;
			}
		}
		return false;
	}

	bool isWalkable(const TilePosition here)
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

	int tilesWithinArea(BWEM::Area const * area, const TilePosition here, const int width, const int height)
	{
		auto cnt = 0;
		for (auto x = here.x; x < here.x + width; x++) {
			for (auto y = here.y; y < here.y + height; y++) {
				TilePosition t(x, y);
				if (!t.isValid())
					return false;

				// Make an assumption that if it's on a chokepoint geometry, it belongs to the area provided
				if (Map::mapBWEM.GetArea(t) == area /*|| !mapBWEM.GetArea(t)*/)
					cnt++;
			}
		}
		return cnt;
	}

	pair<Position, Position> lineOfBestFit(const BWEM::ChokePoint * choke)
	{
		auto sumX = 0.0, sumY = 0.0;
		auto sumXY = 0.0, sumX2 = 0.0;
		for (auto geo : choke->Geometry()) {
			Position p = Position(geo) + Position(4, 4);
			sumX += p.x;
			sumY += p.y;
			sumXY += p.x * p.y;
			sumX2 += p.x * p.x;
		}
		double xMean = sumX / choke->Geometry().size();
		double yMean = sumY / choke->Geometry().size();
		double denominator = sumX2 - sumX * xMean;

		double slope = (sumXY - sumX * yMean) / denominator;
		double yInt = yMean - slope * xMean;

		// y = mx + b
		// solve for x and y

		// end 1
		int x1 = Position(choke->Pos(choke->end1)).x;
		int y1 = int(ceil(x1 * slope)) + int(yInt);
		Position p1 = Position(x1, y1);

		// end 2
		int x2 = Position(choke->Pos(choke->end2)).x;
		int y2 = int(ceil(x2 * slope)) + int(yInt);
		Position p2 = Position(x2, y2);

		return make_pair(p1, p2);
	}

	bool isPlaceable(UnitType type, const TilePosition location)
	{
		// Placeable is valid if buildable and not overlapping neutrals
		// Note: Must check neutrals due to the terrain below them technically being buildable
		const auto creepCheck = type.requiresCreep() ? true : false;
		for (auto x = location.x; x < location.x + type.tileWidth(); x++) {

			if (creepCheck) {
				TilePosition tile(x, location.y + 2);
				if (!Broodwar->isBuildable(tile))
					return false;
			}

			for (auto y = location.y; y < location.y + type.tileHeight(); y++) {

				TilePosition tile(x, y);
				if (!tile.isValid()
					|| !Broodwar->isBuildable(tile)
					|| Map::usedTiles.find(tile) != Map::usedTiles.end()
					|| Map::reserveGrid[x][y] > 0
					|| (type.isResourceDepot() && !Broodwar->canBuildHere(tile, type)))
					return false;
			}
		}

		return true;
	}

	void addOverlap(const TilePosition t, const int w, const int h)
	{
		for (auto x = t.x; x < t.x + w; x++) {
			for (auto y = t.y; y < t.y + h; y++)
				overlapGrid[x][y] = 1;
		}
	}

	void removeOverlap(const TilePosition t, const int w, const int h)
	{
		for (auto x = t.x; x < t.x + w; x++) {
			for (auto y = t.y; y < t.y + h; y++)
				overlapGrid[x][y] = 0;
		}
	}

	void addReserve(const TilePosition t, const int w, const int h)
	{
		for (auto x = t.x; x < t.x + w; x++) {
			for (auto y = t.y; y < t.y + h; y++)
				reserveGrid[x][y] = 1;
		}
	}

	const BWEM::Area * getNaturalArea() { return naturalArea; }
	const BWEM::Area * getMainArea() { return mainArea; }
	const BWEM::ChokePoint * getNaturalChoke() { return naturalChoke; }
	const BWEM::ChokePoint * getMainChoke() { return mainChoke; }
	TilePosition getNaturalTile() { return naturalTile; }
	Position getNaturalPosition() { return naturalPosition; }
	TilePosition getMainTile() { return mainTile; }
	Position getMainPosition() { return mainPosition; }
	set<TilePosition>& getUsedTiles() { return usedTiles; }
}