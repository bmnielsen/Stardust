#include "BWEB.h"

// Authors Notes
// TODO:
// Creep estimations
// Pylon power grid for building placer
// Dynamic block addition (insert UnitTypes, get block)
//	- May not be added for a while
// Check two walkpositions for not being walkable for buildings that have a dimension on one side less than 8
//	- Not sure how critical this is
//	- Barracks and depots can be placed in this way
// Placements are fine if satisfies one condition, needs to satisfy all tight conditions
//	- The final piece for a Terran wall can be placed such that it is no longer unit-tight
// Min wall width to prevent walling self in with a depot
//	- Maps with main/nat on same terrain level can wall itself in
// Door too far away on destination style maps (where we need to move the start locations away from the choke)
//	- Consider a line of best fit across the wall pieces for the door instead of using the chokepoint geometry?

namespace BWEB
{
	Map::Map(BWEM::Map& map)
		: mapBWEM(map)
	{
	}

	void Map::onStart()
	{
		findNeutrals();
		findMain();
		findNatural();
		findMainChoke();
		findNaturalChoke();
		findStations();
	}

	void Map::onUnitDiscover(const Unit unit)
	{
		if (!unit
			|| !unit->getType().isBuilding()
			|| unit->isFlying()
			|| unit->getType() == UnitTypes::Resource_Vespene_Geyser)
			return;

		const auto tile(unit->getTilePosition());
		auto type(unit->getType());

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
	}

	void Map::onUnitMorph(const Unit unit)
	{
		onUnitDiscover(unit);
	}

	void Map::onUnitDestroy(const Unit unit)
	{
		if (!unit
			|| !unit->getType().isBuilding()
			|| unit->isFlying())
			return;

		const auto tile(unit->getTilePosition());
		auto type(unit->getType());

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
	}

	void Map::findMain()
	{
		mainTile = Broodwar->self()->getStartLocation();
		mainPosition = static_cast<Position>(mainTile) + Position(64, 48);
		mainArea = mapBWEM.GetArea(mainTile);
	}

	void Map::findNatural()
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

	void Map::findMainChoke()
	{
		// Add all main chokes to a set
		set<BWEM::ChokePoint const *> mainChokes;
		for (auto &choke : mainArea->ChokePoints()) {
			mainChokes.insert(choke);
		}

		// Find a chokepoint that belongs to main and natural
		auto distBest = DBL_MAX;
		for (auto &choke : naturalArea->ChokePoints()) {
			const auto dist = getGroundDistance(Position(choke->Center()), mainPosition);
			if (mainChokes.find(choke) != mainChokes.end() && dist < distBest) {
				mainChoke = choke;
				distBest = dist;
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

	void Map::findNaturalChoke()
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

	void Map::findNeutrals()
	{
		// Add overlap for neutrals
		for (auto &unit : Broodwar->neutral()->getUnits()) {
			if (unit && unit->exists() && unit->getType().topSpeed() == 0.0)
				addOverlap(unit->getTilePosition(), unit->getType().tileWidth(), unit->getType().tileHeight());
		}
	}

	void Map::draw()
	{
		for (auto &choke : mainArea->ChokePoints()) {
			const auto dist = getGroundDistance(Position(choke->Center()), mainPosition);
			Broodwar->drawTextMap((Position)choke->Center() + Position(0, 16), "%.2f", dist);
		}

		// Draw Blocks 
		for (auto &block : blocks) {
			for (auto &tile : block.SmallTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), Broodwar->self()->getColor());
			for (auto &tile : block.MediumTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(97, 65), Broodwar->self()->getColor());
			for (auto &tile : block.LargeTiles())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(129, 97), Broodwar->self()->getColor());
		}

		// Draw Stations
		for (auto &station : stations) {
			for (auto &tile : station.DefenseLocations())
				Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), Broodwar->self()->getColor());
			Broodwar->drawBoxMap(Position(station.BWEMBase()->Location()), Position(station.BWEMBase()->Location()) + Position(129, 97), Broodwar->self()->getColor());
		}

		// Draw Walls
		for (auto &wall : walls) {
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

		Broodwar->drawCircleMap(Position(testTile), 8, Broodwar->self()->getColor(), true);

		Broodwar->drawTextMap(Position(endTile), "EndTile");
		Broodwar->drawTextMap(Position(startTile), "StartTile");
		Broodwar->drawTextMap(Position(initialEnd), "InitialEnd");
		Broodwar->drawTextMap(Position(initialStart), "InitialStart");

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
	}

	template <class T>
	double Map::getGroundDistance(T s, T e)
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

	TilePosition Map::getBuildPosition(UnitType type, const TilePosition searchCenter)
	{
		auto distBest = DBL_MAX;
		auto tileBest = TilePositions::Invalid;

		// Search through each block to find the closest block and valid position
		for (auto &block : blocks) {
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
		for (auto &wall : walls) {
			for (auto &tile : wall.getDefenses()) {
				const auto dist = tile.getDistance(searchCenter);
				if (dist < distBest && isPlaceable(type, tile))
					distBest = dist, tileBest = tile;
			}
		}

		// Search through each station to find the closest valid TilePosition
		for (auto &station : stations) {
			for (auto &tile : station.DefenseLocations()) {
				const auto dist = tile.getDistance(searchCenter);
				if (dist < distBest && isPlaceable(type, tile))
					distBest = dist, tileBest = tile;
			}
		}
		return tileBest;
	}

	bool Map::isPlaceable(UnitType type, const TilePosition location)
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
					|| usedTiles.find(tile) != usedTiles.end()
					|| reserveGrid[x][y] > 0
					|| (type.isResourceDepot() && !Broodwar->canBuildHere(tile, type)))
					return false;
			}
		}

		return true;
	}

	void Map::addOverlap(const TilePosition t, const int w, const int h)
	{
		for (auto x = t.x; x < t.x + w; x++) {
			for (auto y = t.y; y < t.y + h; y++)
				overlapGrid[x][y] = 1;			
		}
	}

	Map* Map::BWEBInstance = nullptr;

	Map & Map::Instance()
	{
		if (!BWEBInstance)
			BWEBInstance = new Map(BWEM::Map::Instance());
		return *BWEBInstance;
	}
}