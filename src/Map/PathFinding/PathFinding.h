#pragma once

#include "Common.h"
#include "NavigationGrid.h"
#include <bwem.h>

namespace PathFinding
{
    // Clears grids, call at start before initializing Map
    void clearGrids();

    // Initializes the navigation grids
    void initializeGrids();

    // Gets the navigation grid to a specific goal position
    NavigationGrid *getNavigationGrid(BWAPI::TilePosition goal);

    // An object that affects pathfinding (e.g. a building) has been added
    void addBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile);

    // Tiles that affect pathfinding (e.g. a mineral line) have been added
    void addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles);

    // An object that affects pathfinding (e.g. a building) has been removed
    void removeBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile);

    // Tiles that affect pathfinding (e.g. a mineral line) have been removed
    void removeBlockingTiles(const std::set<BWAPI::TilePosition> &tiles);

    // Options for use in the BWEM-based pathfinding methods
    enum class PathFindingOptions
    {
        Default = 0,
        UseNearestBWEMArea = 1 << 0
    };

    // Gets the ground distance between two points pathing through BWEM chokepoints.
    // If there is no valid path, returns -1.
    // By default, if either of the ends doesn't have a valid BWEM area, the method will fall back to the air distance.
    // If you want the ground distance to the nearest BWEM area, pass the UseNearestBWEMArea flag.
    // Make sure neither of the ends is over a lake, this will make it very slow!
    int GetGroundDistance(
            BWAPI::Position start,
            BWAPI::Position end,
            BWAPI::UnitType unitType = BWAPI::UnitTypes::Protoss_Dragoon,
            PathFindingOptions options = PathFindingOptions::Default);

    // Gets a path between two points as a list of choke points between them.
    // Returns an empty path if the two points are in the same BWEM area or if there is no valid path.
    // By default, if either of the ends doesn't have a valid BWEM area, the method will return an empty path.
    // If you want to use the nearest BWEM areas, pass the UseNearestBWEMArea flag.
    // Make sure neither of the ends is over a lake, this will make it very slow!
    BWEM::CPPath GetChokePointPath(
            BWAPI::Position start,
            BWAPI::Position end,
            BWAPI::UnitType unitType = BWAPI::UnitTypes::Protoss_Dragoon,
            PathFindingOptions options = PathFindingOptions::Default,
            int *pathLength = nullptr);

    // Gets the expected time it will take the given unit type to move from the given start position
    // to the given end position.
    // If there is no valid path, the result is undefined.
    // If you want to use the nearest BWEM areas, pass the UseNearestBWEMArea flag.
    // Make sure neither of the ends is over a lake, this will make it very slow!
    int ExpectedTravelTime(
            BWAPI::Position start,
            BWAPI::Position end,
            BWAPI::UnitType unitType,
            PathFindingOptions options = PathFindingOptions::Default);

    // Initializes the path finding search
    void initializeSearch();

    // Searches for the shortest walkable path from start to end
    // By default, any walkable tile (as determined by Map::isWalkable) can be part of the returned path
    // If a tileValidator is passed, only tiles for which it returns true can be part of the returned path
    // If an endRadius is specified, the search will stop as soon as a node is found within that radius (pixels) of the end tile
    std::vector<BWAPI::TilePosition> Search(BWAPI::TilePosition start,
                                            BWAPI::TilePosition end,
                                            const std::function<bool(const BWAPI::TilePosition &)> &tileValidator = nullptr,
                                            const std::function<bool(const BWAPI::TilePosition &)> &closeEnoughToEnd = nullptr);
}
