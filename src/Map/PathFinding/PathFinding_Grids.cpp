#include "PathFinding.h"

#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define OUTPUT_GRID_TIMING false
#endif

namespace Pathfinder
{
    namespace
    {
        std::map<BWAPI::TilePosition, std::pair<NavGrid, NavGrid>> goalToNavigationGrid;

        void createNavigationGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize = BWAPI::TilePositions::Invalid)
        {
            goalToNavigationGrid.emplace(goal, std::make_pair(NavGrid(goal, goalSize), NavGrid(goal, goalSize)));
        }
    }

    void clearGrids()
    {
        goalToNavigationGrid.clear();
    }

    void initializeGrids()
    {
        NavigationGridGlobals::initialize();

        for (auto baseLoc : Map::allBases())
        {
            createNavigationGrid(BWAPI::TilePosition(baseLoc->getPosition()), BWAPI::UnitTypes::Protoss_Nexus.tileSize());
        }
        for (auto choke : Map::allChokes())
        {
            if (choke != Map::getMyMainChoke() && !choke->isNarrowChoke) continue;

            createNavigationGrid(BWAPI::TilePosition(choke->midpoint));
        }
    }

    NavGrid *getNavigationGrid(BWAPI::TilePosition goal, bool ignoreEnemyBuildings)
    {
        auto gridIt = goalToNavigationGrid.find(goal);
        if (gridIt == goalToNavigationGrid.end()) return nullptr;

        auto &grid = ignoreEnemyBuildings ? gridIt->second.second : gridIt->second.first;
        grid.update();
        return &grid;
    }

    NavGrid *getNavigationGrid(BWAPI::Position goal, bool ignoreEnemyBuildings)
    {
        return getNavigationGrid(BWAPI::TilePosition(goal), ignoreEnemyBuildings);
    }

    void addBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tilePos, bool isEnemyBuilding)
    {
#if OUTPUT_GRID_TIMING
        auto start = std::chrono::high_resolution_clock::now();
#endif

        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.addBlockingObject(tilePos, type.tileSize());
            if (!isEnemyBuilding)
            {
                goalAndNavigationGrid.second.second.addBlockingObject(tilePos, type.tileSize());
            }
        }

#if OUTPUT_GRID_TIMING
        auto now = std::chrono::high_resolution_clock::now();
        AppLog::Get() << "addBlockingObject(" << type << ", " << tilePos << ", enemyBuilding=" << isEnemyBuilding << "): "
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
        AppLog::Get() << "addBlockingTiles: " << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
#endif
    }

    void removeBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tilePos, bool isEnemyBuilding)
    {
#if OUTPUT_GRID_TIMING
        auto start = std::chrono::high_resolution_clock::now();
#endif

        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.removeBlockingObject(tilePos, type.tileSize());
            if (!isEnemyBuilding)
            {
                goalAndNavigationGrid.second.second.removeBlockingObject(tilePos, type.tileSize());
            }
        }

#if OUTPUT_GRID_TIMING
        auto now = std::chrono::high_resolution_clock::now();
        AppLog::Get() << "removeBlockingObject(" << type << ", " << tilePos << ", enemyBuilding=" << isEnemyBuilding << "): "
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
        AppLog::Get() << "removeBlockingTiles: " << std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() << "us";
#endif
    }

    bool checkGridPath(BWAPI::TilePosition start,
                       BWAPI::TilePosition end,
                       const std::function<bool(const NavGrid::GridNode &gridNode)> &predicate)
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
