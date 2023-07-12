#pragma once
#include <set>
#include <BWAPI.h>
#include <bwem.h>

namespace BWEB {

    class Wall
    {
        BWAPI::UnitType tightType;
        BWAPI::Position centroid;
        BWAPI::TilePosition opening, initialPathStart, initialPathEnd, pathStart, pathEnd, creationStart;
        std::set<BWAPI::TilePosition> allDefenses, smallTiles, mediumTiles, largeTiles;
        std::map<int, std::set<BWAPI::TilePosition>> defenses;
        std::set<BWAPI::Position> notableLocations;
        std::vector<BWAPI::UnitType>::iterator typeIterator;
        std::vector<BWAPI::UnitType> rawBuildings, rawDefenses;
        std::vector<const BWEM::Area *> accessibleNeighbors;
        std::map<BWAPI::TilePosition, BWAPI::UnitType> currentLayout, bestLayout;
        const BWEM::Area * area = nullptr;
        const BWEM::ChokePoint * choke = nullptr;
        const BWEM::Base * base = nullptr;
        double chokeAngle, bestWallScore, jpsDist;
        bool valid, pylonWall, openWall, requireTight, movedStart, pylonWallPiece, allowLifted, flatRamp, angledChoke;
        int bestDoorCount = 25;
        int defenseAngle = 0;
        BWEB::Station * station = nullptr;
        Path finalPath;

        BWAPI::Position findCentroid();
        BWAPI::TilePosition findOpening();
        Path findPathOut();
        bool powerCheck(const BWAPI::UnitType, const BWAPI::TilePosition);
        bool angleCheck(const BWAPI::UnitType, const BWAPI::TilePosition);
        bool placeCheck(const BWAPI::UnitType, const BWAPI::TilePosition);
        bool tightCheck(const BWAPI::UnitType, const BWAPI::TilePosition, bool visual = false);
        bool spawnCheck(const BWAPI::UnitType, const BWAPI::TilePosition);
        bool wallWalkable(const BWAPI::TilePosition&);
        void initialize();
        void initializePathPoints();
        void checkPathPoints();
        void addPieces();
        void addNextPiece(BWAPI::TilePosition);
        void addDefenses();
        void scoreWall();
        void cleanup();

    public:
        Wall(const BWEM::Area * _area, const BWEM::ChokePoint * _choke, std::vector<BWAPI::UnitType> _buildings, std::vector<BWAPI::UnitType> _defenses, BWAPI::UnitType _tightType, bool _requireTight, bool _openWall) {
            area                = _area;
            choke               = _choke;
            rawBuildings        = _buildings;
            rawDefenses         = _defenses;
            tightType           = _tightType;
            requireTight        = _requireTight;
            openWall            = _openWall;

            // Create Wall layout and find basic features
            initialize();
            addPieces();
            currentLayout       = bestLayout;
            centroid            = findCentroid();
            opening             = findOpening();
            valid               = (getSmallTiles().size() + getMediumTiles().size() + getLargeTiles().size()) == getRawBuildings().size();

            // Add defenses
            addDefenses();

            // Verify opening and cleanup Wall
            opening             = findOpening();
            cleanup();
        }

        /// <summary> Adds a piece at the TilePosition based on the UnitType. </summary>
        void addToWallPieces(BWAPI::TilePosition here, BWAPI::UnitType building) {
            if (building.tileWidth() >= 4)
                largeTiles.insert(here);
            else if (building.tileWidth() >= 3)
                mediumTiles.insert(here);
            else if (building.tileWidth() >= 2)
                smallTiles.insert(here);
        }

        /// <summary> Returns the Station this wall is close to if one exists. </summary>
        Station* getStation() { return station; }

        /// <summary> Returns the Path out if one exists. </summary>
        Path& getPath() { return finalPath; }

        /// <summary> Returns the Chokepoint associated with this Wall. </summary>
        const BWEM::ChokePoint * getChokePoint() { return choke; }

        /// <summary> Returns the Area associated with this Wall. </summary>
        const BWEM::Area * getArea() { return area; }

        /// <summary> Returns the defense locations associated with this Wall. </summary>
        std::set<BWAPI::TilePosition>& getDefenses(int row = 0) {
            return defenses[row];
        }

        /// <summary> Returns the TilePosition belonging to the opening of the wall. </summary>
        BWAPI::TilePosition getOpening() { return opening; }

        /// <summary> Returns the TilePosition belonging to the centroid of the wall pieces. </summary>
        BWAPI::Position getCentroid() { return centroid; }

        /// <summary> Returns the TilePosition belonging to large UnitType buildings. </summary>
        std::set<BWAPI::TilePosition>& getLargeTiles() { return largeTiles; }

        /// <summary> Returns the TilePosition belonging to medium UnitType buildings. </summary>
        std::set<BWAPI::TilePosition>& getMediumTiles() { return mediumTiles; }

        /// <summary> Returns the TilePosition belonging to small UnitType buildings. </summary>
        std::set<BWAPI::TilePosition>& getSmallTiles() { return smallTiles; }

        /// <summary> Returns the raw vector of the buildings the wall was initialzied with. </summary>
        std::vector<BWAPI::UnitType>& getRawBuildings() { return rawBuildings; }

        /// <summary> Returns the raw vector of the defenses the wall was initialzied with. </summary>
        std::vector<BWAPI::UnitType>& getRawDefenses() { return rawDefenses; }

        /// <summary> Returns true if the Wall only contains Pylons. </summary>
        bool isPylonWall() { return pylonWall; }

        /// <summary> Returns the number of ground defenses associated with this Wall. </summary>
        int getGroundDefenseCount();

        /// <summary> Returns the number of air defenses associated with this Wall. </summary>
        int getAirDefenseCount();

        /// <summary> Draws all the features of the Wall. </summary>
        void draw();
    };

    namespace Walls {

        /// <summary> Returns a map containing every Wall keyed by Chokepoint. </summary>
        std::map<const BWEM::ChokePoint *, Wall>& getWalls();

        /// <summary> <para> Returns a pointer to a Wall if it has been created in the given Area and ChokePoint. </para>
        /// <para> Note: If you only pass a Area or a ChokePoint (not both), it will imply and pick a Wall that exists within that Area or blocks that ChokePoint. </para></summary>
        /// <param name="area"> The Area that the Wall resides in. </param>
        /// <param name="choke"> The Chokepoint that the Wall blocks. </param>
        Wall * getWall(const BWEM::ChokePoint *);

        /// <summary> Returns the closest Wall to the given TilePosition. </summary>
        Wall * getClosestWall(BWAPI::TilePosition);

        /// <summary> <para> Given a vector of UnitTypes, an Area and a Chokepoint, finds an optimal wall placement, returns a valid pointer if a Wall was created. </para>
        /// <para> Note: Highly recommend that only Terran walls attempt to be walled tight, as most Protoss and Zerg wallins have gaps to allow your units through.</para>
        /// <para> BWEB makes tight walls very differently from non tight walls and will only create a tight wall if it is completely tight. </para></summary>
        /// <param name="buildings"> A Vector of UnitTypes that you want the Wall to consist of. </param>
        /// <param name="area"> The Area that you want the Wall to be contained within. </param>
        /// <param name="choke"> The Chokepoint that you want the Wall to block. </param>
        /// <param name="tight"> (Optional) Decides whether this Wall intends to be walled around a specific UnitType. </param>
        /// <param name="defenses"> (Optional) A Vector of UnitTypes that you want the Wall to have defenses consisting of. </param>
        /// <param name="openWall"> (Optional) Set as true if you want an opening in the wall for unit movement. </param>
        /// <param name="requireTight"> (Optional) Set as true if you want pixel perfect placement. </param>
        Wall * createWall(std::vector<BWAPI::UnitType>& buildings, const BWEM::Area * area, const BWEM::ChokePoint * choke, BWAPI::UnitType tight = BWAPI::UnitTypes::None, const std::vector<BWAPI::UnitType>& defenses ={}, bool openWall = false, bool requireTight = false);

        /// I only added this to support Alchemist because it's a trash fucking map.
        Wall* createHardWall(std::multimap<BWAPI::UnitType, BWAPI::TilePosition>&, const BWEM::Area *, const BWEM::ChokePoint *);

        /// <summary><para> Creates a Forge Fast Expand at the natural. </para>
        /// <para> Places 1 Forge, 1 Gateway, 1 Pylon and 10 Cannons. </para></summary>
        Wall * createFFE();

        /// <summary><para> Creates a "Sim City" of Zerg buildings at the natural. </para>
        /// <para> Places 10 Sunkens, 1 Evolution Chamber and 1 Hatchery. </para>
        Wall * createZSimCity();

        /// <summary><para> Creates a full wall of Terran buildings at the main choke. </para>
        /// <para> Places 2 Depots and 1 Barracks. </para>
        Wall * createTWall();

        /// <summary> Calls the draw function for each Wall that exists. </summary>
        void draw();
    }
}
