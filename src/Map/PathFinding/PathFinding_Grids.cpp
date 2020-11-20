#include "PathFinding.h"

#include "Map.h"

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
        for (auto base : Map::allBases())
        {
            createNavigationGrid(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus.tileSize());
        }
        for (auto choke : Map::allChokes())
        {
            if (choke != Map::getMyMainChoke() && !choke->isNarrowChoke) continue;

            createNavigationGrid(BWAPI::TilePosition(choke->center));
        }
    }

    NavigationGrid *getNavigationGrid(BWAPI::TilePosition goal, bool ignoreEnemyBuildings)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            if (goalAndNavigationGrid.first.getApproxDistance(goal) < 5)
            {
                auto &grid = ignoreEnemyBuildings ? goalAndNavigationGrid.second.second : goalAndNavigationGrid.second.first;
                grid.update();
                return &grid;
            }
        }

        return nullptr;
    }

    void addBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile, bool isEnemyBuilding)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.addBlockingObject(tile, type.tileSize());
            if (!isEnemyBuilding)
            {
                goalAndNavigationGrid.second.second.addBlockingObject(tile, type.tileSize());
            }
        }
    }

    void addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.addBlockingTiles(tiles);
            goalAndNavigationGrid.second.second.addBlockingTiles(tiles);
        }
    }

    void removeBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile, bool isEnemyBuilding)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.removeBlockingObject(tile, type.tileSize());
            if (!isEnemyBuilding)
            {
                goalAndNavigationGrid.second.second.removeBlockingObject(tile, type.tileSize());
            }
        }
    }

    void removeBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.first.removeBlockingTiles(tiles);
            goalAndNavigationGrid.second.second.removeBlockingTiles(tiles);
        }
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
            if (node->cost < 30) return true;
            if (!node->nextNode) return false;
            if (!predicate(*node)) return false;
            node = node->nextNode;
        }

        return true;
    }
}
