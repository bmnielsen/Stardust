#pragma once
#pragma warning(disable : 4351)
#include <set>

#include <BWAPI.h>
#include <bwem.h>
#include "Station.h"
#include "Block.h"
#include "Wall.h"

namespace BWEB
{
	using namespace BWAPI;
	using namespace std;

	class Block;
	class Wall;
	class Station;
	class Map
	{
	private:
		
		vector<Station> stations;
		vector<Wall> walls;
		vector<Block> blocks;

		TilePosition testTile;
		vector<TilePosition> chokeTiles;
		
		// Blocks
		// TODO: Add this function. This would be used to create a block that makes room for a specific type (possibly better generation than floodfill)
		// void createBlock(BWAPI::Race, UnitType, BWEM::Area const *, TilePosition);
		void insertBlock(BWAPI::Race, TilePosition, int, int);
		void findStartBlock();
		void findStartBlock(BWAPI::Player);
		void findStartBlock(BWAPI::Race);
		bool canAddBlock(TilePosition, int, int);

		void insertStartBlock(TilePosition, bool, bool);
		void insertStartBlock(BWAPI::Player, TilePosition, bool, bool);
		void insertStartBlock(BWAPI::Race, TilePosition, bool, bool);
		map<const BWEM::Area *, int> typePerArea;

		// Walls
		bool isWallTight(Wall&, UnitType, TilePosition);
		bool isPoweringWall(Wall&, TilePosition);
		bool iteratePieces(Wall&);
		void findCurrentHole(Wall&, bool);

		TilePosition initialStart, initialEnd;
		void initializePathPoints(Wall&);
		void checkPathPoints(Wall&);

		int reserveGrid[256][256] = {};

		double bestWallScore = 0.0;
		TilePosition currentHole, startTile, endTile;
		vector<TilePosition> currentPath;
		vector<UnitType>::iterator typeIterator;
		map<TilePosition, UnitType> bestWall;
		map<TilePosition, UnitType> currentWall;
		double currentPathSize{};

		// Information that is passed in
		BWEM::Map& mapBWEM;
		UnitType tight;
		bool reservePath{};
		bool requireTight;
		int chokeWidth;
		Position wallBase;

		// Map
		void findMain(), findMainChoke(), findNatural(), findNaturalChoke(), findNeutrals();
		Position mainPosition, naturalPosition;
		TilePosition mainTile, naturalTile;
		const BWEM::Area * naturalArea{};
		const BWEM::Area * mainArea{};
		const BWEM::ChokePoint * naturalChoke{};
		const BWEM::ChokePoint * mainChoke{};
		set<TilePosition> usedTiles;
		void addOverlap(TilePosition, int, int);
		bool isPlaceable(UnitType, TilePosition);

		// Stations
		void findStations();
		set<TilePosition> stationDefenses(BWAPI::Race, TilePosition, bool, bool);
		set<TilePosition> stationDefenses(BWAPI::Player, TilePosition, bool, bool);
		set<TilePosition> stationDefenses(TilePosition, bool, bool);

		// General
		static Map* BWEBInstance;
		int testGrid[256][256];

	public:
		Map(BWEM::Map& map);
		void draw(), onStart(), onUnitDiscover(Unit), onUnitDestroy(Unit), onUnitMorph(Unit);
		static Map &Instance();
		int overlapGrid[256][256] = {};
		int usedGrid[256][256] ={};

		/// This is just put here so AStar can use it for now
		UnitType overlapsCurrentWall(TilePosition tile, int width = 1, int height = 1);

		//vector<TilePosition> findBuildableBorderTiles(const BWEM::Map &, WalkPosition, const BWEM::Area *);
		bool overlapsAnything(TilePosition here, int width = 1, int height = 1, bool ignoreBlocks = false);
		static bool isWalkable(TilePosition);
		int tilesWithinArea(BWEM::Area const *, TilePosition here, int width = 1, int height = 1);

		/// <summary> Returns the closest buildable TilePosition for any type of structure </summary>
		/// <param name="type"> The UnitType of the structure you want to build.</param>
		/// <param name="tile"> The TilePosition you want to build closest to.</param>
		TilePosition getBuildPosition(UnitType type, TilePosition searchCenter = Broodwar->self()->getStartLocation());

		/// <summary> Returns the closest buildable TilePosition for a defensive structure </summary>
		/// <param name="type"> The UnitType of the structure you want to build.</param>
		/// <param name="tile"> The TilePosition you want to build closest to. </param>
		TilePosition getDefBuildPosition(UnitType type, TilePosition tile = Broodwar->self()->getStartLocation());

		template <class T>
		/// <summary> Returns the estimated ground distance from one Position type to another Position type.</summary>
		/// <param name="first"> The first Position. </param>
		/// <param name="second"> The second Position. </param>
		double getGroundDistance(T start, T end);

		/// <summary> <para> Returns a pointer to a BWEB::Wall if it has been created in the given BWEM::Area and BWEM::ChokePoint. </para>
		/// <para> Note: If you only pass a BWEM::Area or a BWEM::ChokePoint (not both), it will imply and pick a BWEB::Wall that exists within that Area or blocks that BWEM::ChokePoint. </para></summary>
		/// <param name="area"> The BWEM::Area that the BWEB::Wall resides in </param>
		/// <param name="choke"> The BWEM::Chokepoint that the BWEB::Wall blocks </param>
		Wall* getWall(BWEM::Area const* area = nullptr, BWEM::ChokePoint const* choke = nullptr);

		// TODO: Add this
		Station* getStation(BWEM::Area const* area);

		/// <summary> Returns the BWEM::Area of the natural expansion </summary>
		const BWEM::Area * getNaturalArea() const { return naturalArea; }

		/// <summary> Returns the BWEM::Area of the main </summary>
		const BWEM::Area * getMainArea() const { return mainArea; }

		/// <summary> Returns the BWEM::Chokepoint of the natural </summary>
		const BWEM::ChokePoint * getNaturalChoke() const { return naturalChoke; }

		/// <summary> Returns the BWEM::Chokepoint of the main </summary>
		const BWEM::ChokePoint * getMainChoke() const { return mainChoke; }

		/// <summary> Returns a vector containing every BWEB::Wall. </summary>
		vector<Wall> getWalls() const { return walls; }

		/// <summary> Returns a vector containing every BWEB::Block </summary>
		vector<Block> Blocks() const { return blocks; }

		/// <summary> Returns a vector containing every BWEB::Station </summary>
		vector<Station> Stations() const { return stations; }

		/// <summary> Returns the closest BWEB::Station to the given TilePosition. </summary>
		const Station* getClosestStation(TilePosition) const;

		/// <summary> Returns the closest BWEB::Wall to the given TilePosition. </summary>
		const Wall* getClosestWall(TilePosition) const;

		/// <summary> Returns the closest BWEB::Block to the given TilePosition. </summary>
		const Block* getClosestBlock(TilePosition) const;

		/// Returns the TilePosition of the natural expansion
		TilePosition getNaturalTile() const { return naturalTile; }

		/// Returns the Position of the natural expansion
		Position getNaturalPosition() const { return naturalPosition; }

		/// Returns the TilePosition of the main
		TilePosition getMainTile() const { return mainTile; }

		/// Returns the Position of the main
		Position getMainPosition() const { return mainPosition; }

		/// Returns the set of used TilePositions
		set<TilePosition>& getUsedTiles() { return usedTiles; }

		/// <summary> <para> Given a vector of UnitTypes, an Area and a Chokepoint, finds an optimal wall placement, returns true if a valid BWEB::Wall was created. </para>
		/// <para> Note: Highly recommend that only Terran walls attempt to be walled tight, as most Protoss and Zerg wallins have gaps to allow your units through.</para>
		/// <para> BWEB makes tight walls very differently from non tight walls and will only create a tight wall if it is completely tight. </para></summary>
		/// <param name="buildings"> A Vector of UnitTypes that you want the BWEB::Wall to consist of. </param>
		/// <param name="area"> The BWEM::Area that you want the BWEB::Wall to be contained within. </param>
		/// <param name="choke"> The BWEM::Chokepoint that you want the BWEB::Wall to block. </param>
		/// <param name="tight"> (Optional) Decides whether this BWEB::Wall intends to be walled around a specific UnitType. </param>
		/// <param name="defenses"> A Vector of UnitTypes that you want the BWEB::Wall to have defenses consisting of. </param>
		/// <param name="reservePath"> Optional parameter to ensure that a path of TilePositions is reserved and not built on. </param>
		/// <param name="requireTight"> Optional parameter to ensure that the Wall must be walltight. </param>
		void createWall(vector<UnitType>& buildings, const BWEM::Area * area, const BWEM::ChokePoint * choke, UnitType tight = UnitTypes::None, const vector<UnitType>& defenses = {}, bool reservePath = false, bool requireTight = false);

		/// <summary> Adds a UnitType to a currently existing BWEB::Wall. </summary>
		/// <param name="type"> The UnitType you want to place at the BWEB::Wall. </param>
		/// <param name="area"> The BWEB::Wall you want to add to. </param>
		/// <param name="tight"> (Optional) Decides whether this addition to the BWEB::Wall intends to be walled around a specific UnitType. Defaults to none. </param>
		void addToWall(UnitType type, Wall& wall, UnitType tight = UnitTypes::None);

		/// <summary> Erases any blocks at the specified TilePosition. </summary>
		/// <param name="here"> The TilePosition that you want to delete any BWEB::Block that exists here. </param>
		void eraseBlock(TilePosition here);

		/// <summary> Initializes the building of every BWEB::Block on the map, call it only once per game. </summary>
		void findBlocks(BWAPI::Player);
		void findBlocks(BWAPI::Race);
		void findBlocks();		
	};

	class Path {
		vector<TilePosition> tiles;
		double dist;		
	public:
		Path();
		vector<TilePosition>& getTiles() { return tiles; }
		double getDistance() { return dist; }
		void createUnitPath(BWEB::Map&, BWEM::Map&, const Position, const Position);
		void createWallPath(BWEB::Map&, BWEM::Map&, const TilePosition, const TilePosition, bool);
	};

	// This namespace contains functions which could be used for backward compatibility
	// with existing code.
	// just put using namespace BWEB::Utils in the header and 
	// replace all usages of Map::overlapsBlocks with just overlapsBlocks
	namespace Utils
	{
		static int tilesWithinArea(BWEM::Area const *, TilePosition here, int width = 1, int height = 1);
	}
}
