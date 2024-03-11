#include "PathFinding.h"

#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define OUTPUT_GRID_TIMING true
#endif

namespace PathFinding
{
    namespace
    {
        std::map<BWAPI::TilePosition, std::pair<NavigationGrid, NavigationGrid>> goalToNavigationGrid;

        void createNavigationGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize = BWAPI::TilePositions::Invalid)
        {
            goalToNavigationGrid.emplace(goal, std::make_pair(NavigationGrid(goal, goalSize), NavigationGrid(goal, goalSize)));
        }
    }

    void clearGrids()
    {
        goalToNavigationGrid.clear();
    }

    void initializeGrids()
    {
        NavigationGridGlobals::initialize();

        for (auto base : Map::allBases())
        {
            createNavigationGrid(BWAPI::TilePosition(base->getPosition()), BWAPI::UnitTypes::Protoss_Nexus.tileSize());
        }
        for (auto choke : Map::allChokes())
        {
            if (choke != Map::getMyMainChoke() && !choke->isNarrowChoke) continue;

            createNavigationGrid(BWAPI::TilePosition(choke->center));
        }
    }

    NavigationGrid *getNavigationGrid(BWAPI::TilePosition goal, bool ignoreEnemyBuildings)
    {
        auto gridIt = goalToNavigationGrid.find(goal);
        if (gridIt == goalToNavigationGrid.end()) return nullptr;

        auto &grid = ignoreEnemyBuildings ? gridIt->second.second : gridIt->second.first;
        grid.update();
        return &grid;
    }

    NavigationGrid *getNavigationGrid(BWAPI::Position goal, bool ignoreEnemyBuildings)
    {
        return getNavigationGrid(BWAPI::TilePosition(goal), ignoreEnemyBuildings);
    }

    void addBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile, bool isEnemyBuilding)
    {
#if OUTPUT_GRID_TIMING
        auto start = std::chrono::high_resolution_clock::now();
#endif

        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.addBlockingObject(tile, type.tileSize());
            if (!isEnemyBuilding)
            {
                goalAndNavigationGrid.second.second.addBlockingObject(tile, type.tileSize());
            }
        }

#if OUTPUT_GRID_TIMING
        auto now = std::chrono::high_resolution_clock::now();
        Log::Get() << "addBlockingObject(" << type << ", " << tile << ", enemyBuilding=" << isEnemyBuilding << "): "
            << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
#endif
    }

    void addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
    {
#if OUTPUT_GRID_TIMING
        auto start = std::chrono::high_resolution_clock::now();
#endif

        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.addBlockingTiles(tiles);
            goalAndNavigationGrid.second.second.addBlockingTiles(tiles);
        }

#if OUTPUT_GRID_TIMING
        auto now = std::chrono::high_resolution_clock::now();
        Log::Get() << "addBlockingTiles: " << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
#endif
    }

    void removeBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile, bool isEnemyBuilding)
    {
#if OUTPUT_GRID_TIMING
        auto start = std::chrono::high_resolution_clock::now();
#endif

        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.removeBlockingObject(tile, type.tileSize());
            if (!isEnemyBuilding)
            {
                goalAndNavigationGrid.second.second.removeBlockingObject(tile, type.tileSize());
            }
        }

#if OUTPUT_GRID_TIMING
        auto now = std::chrono::high_resolution_clock::now();
        Log::Get() << "removeBlockingObject(" << type << ", " << tile << ", enemyBuilding=" << isEnemyBuilding << "): "
                   << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
#endif
    }

    void removeBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
    {
#if OUTPUT_GRID_TIMING
        auto start = std::chrono::high_resolution_clock::now();
#endif

        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.removeBlockingTiles(tiles);
            goalAndNavigationGrid.second.second.removeBlockingTiles(tiles);
        }

#if OUTPUT_GRID_TIMING
        auto now = std::chrono::high_resolution_clock::now();
        Log::Get() << "removeBlockingTiles: " << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
#endif
    }

    bool checkGridPath(BWAPI::TilePosition start,
                       BWAPI::TilePosition end,
                       const std::function<bool(const NavigationGrid::GridNode &gridNode)> &predicate)
    {
        auto grid = getNavigationGrid(end);
        if (!grid) return true;

        auto node = &(*grid)[start];
        for (int i = 0; i < 1000; i++)
        {
            if (node->cost < 90) return true;
            if (!node->nextNode) return false;
            if (!predicate(*node)) return false;
            node = node->nextNode;
        }

        return true;
    }
}
