#include "PathFinding.h"

#include "Map.h"

namespace PathFinding
{
    namespace
    {
        std::map<BWAPI::TilePosition, NavigationGrid> goalToNavigationGrid;

        void createNavigationGrid(BWAPI::TilePosition goal, BWAPI::TilePosition goalSize = BWAPI::TilePositions::Invalid)
        {
            goalToNavigationGrid.emplace(goal, NavigationGrid(goal, goalSize));
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
            if (!choke->isNarrowChoke) continue;

            createNavigationGrid(BWAPI::TilePosition(choke->center));
        }
    }

    NavigationGrid *getNavigationGrid(BWAPI::TilePosition goal)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            if (goalAndNavigationGrid.first.getApproxDistance(goal) < 5)
            {
                goalAndNavigationGrid.second.update();
                return &goalAndNavigationGrid.second;
            }
        }

        return nullptr;
    }

    void addBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.addBlockingObject(tile, type.tileSize());
        }
    }

    void addBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.addBlockingTiles(tiles);
        }
    }

    void removeBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.removeBlockingObject(tile, type.tileSize());
        }
    }

    void removeBlockingTiles(const std::set<BWAPI::TilePosition> &tiles)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.removeBlockingTiles(tiles);
        }
    }
}
