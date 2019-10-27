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
        for (auto base : Map::allBases())
        {
            createNavigationGrid(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus.tileSize());
        }
        for (auto choke : Map::allChokes())
        {
            if (choke->width >= 128) continue;

            auto tile = choke->highElevationTile.isValid() ? choke->highElevationTile : BWAPI::TilePosition(choke->Center());
            createNavigationGrid(tile);
        }
    }

    const NavigationGrid::GridNode *optimizedPath(BWAPI::Position start, BWAPI::Position end)
    {
        for (auto &goalAndNavigationGrid : goalToNavigationGrid)
        {
            if (goalAndNavigationGrid.first.getApproxDistance(BWAPI::TilePosition(end)) < 5)
            {
                goalAndNavigationGrid.second.update();
                return &goalAndNavigationGrid.second[start];
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
