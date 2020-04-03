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

    void initialize()
    {
        goalToNavigationGrid.clear();

        for (auto base : Map::allBases())
        {
            createNavigationGrid(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus.tileSize());
        }
        for (auto choke : Map::allChokes())
        {
            if (choke->width >= 128) continue;

            auto tile = choke->highElevationTile.isValid() ? choke->highElevationTile : BWAPI::TilePosition(choke->center);
            createNavigationGrid(tile);
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

    void removeBlockingObject(BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            goalAndNavigationGrid.second.removeBlockingObject(tile, type.tileSize());
        }
    }
}
