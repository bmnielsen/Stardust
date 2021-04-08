#include "Map.h"

#include "PathFinding.h"

#define DIR_N 0
#define DIR_S 1
#define DIR_W 2
#define DIR_E 3
#define DIR_NW 4
#define DIR_NE 5
#define DIR_SW 6
#define DIR_SE 7

#if INSTRUMENTATION_ENABLED
#define UNWALKABLE_DIRECTION_HEATMAP_ENABLED false
#endif

namespace Map
{
    namespace
    {
        std::vector<bool> tileWalkability;
        std::vector<unsigned short> tileDistanceToUnwalkableDirections;
        std::vector<unsigned short> tileDistanceToUnwalkable;
        std::vector<unsigned short> tileWalkableWidth;
        std::vector<BWAPI::Position> tileCollisionVector;
        bool tileWalkabilityUpdated;

        void updateTileDistanceToUnwalkable(int x, int y)
        {
            auto index = (x + y * BWAPI::Broodwar->mapWidth());
            auto dirIndex = index << 3U;
            tileDistanceToUnwalkable[index] = std::min(
                    {
                            (unsigned short) 20, // Cap it at 20, we treat anything more than this as open terrain
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_N],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_S],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_E],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_W],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_NE],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_SW],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_NW],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_SE],
                    }
            );
        }

        void updateTileWalkableWidth(int x, int y)
        {
            auto index = (x + y * BWAPI::Broodwar->mapWidth());
            auto dirIndex = index << 3U;
            tileWalkableWidth[index] = std::min(
                    {
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_N] + tileDistanceToUnwalkableDirections[dirIndex + DIR_S],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_E] + tileDistanceToUnwalkableDirections[dirIndex + DIR_W],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_NE] + tileDistanceToUnwalkableDirections[dirIndex + DIR_SW],
                            tileDistanceToUnwalkableDirections[dirIndex + DIR_NW] + tileDistanceToUnwalkableDirections[dirIndex + DIR_SE],
                    });
        }

        void initializeDistanceToUnwalkable()
        {
            tileDistanceToUnwalkableDirections.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 8);
            tileDistanceToUnwalkable.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            tileWalkableWidth.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());

            auto line = [](int x, int deltaX, int y, int deltaY, int dir)
            {
                short current = 0;
                while (x >= 0 && x < BWAPI::Broodwar->mapWidth() &&
                       y >= 0 && y < BWAPI::Broodwar->mapHeight())
                {
                    if (tileWalkability[x + y * BWAPI::Broodwar->mapWidth()])
                    {
                        current++;
                    }
                    else
                    {
                        current = 0;
                    }

                    tileDistanceToUnwalkableDirections[((x + y * BWAPI::Broodwar->mapWidth()) << 3) + dir] = current;

                    x += deltaX;
                    y += deltaY;
                }
            };

            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                line(x, 0, 0, 1, DIR_N); // South
                line(x, 0, BWAPI::Broodwar->mapHeight() - 1, -1, DIR_S); // North
                line(x, 1, 0, 1, DIR_NW); // South-east
                line(x, -1, 0, 1, DIR_NE); // South-west
                line(x, 1, BWAPI::Broodwar->mapHeight() - 1, -1, DIR_SW); // North-east
                line(x, -1, BWAPI::Broodwar->mapHeight() - 1, -1, DIR_SE); // North-west
            }

            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                line(0, 1, y, 0, DIR_W); // East
                line(BWAPI::Broodwar->mapWidth() - 1, -1, y, 0, DIR_E); // West
            }

            for (int y = 0; y < (BWAPI::Broodwar->mapHeight() - 1); y++)
            {
                line(0, 1, y, -1, DIR_SW); // North-east
                line(BWAPI::Broodwar->mapWidth() - 1, -1, y, -1, DIR_SE); // North-west
            }

            for (int y = 1; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                line(0, 1, y, 1, DIR_NW); // South-east
                line(BWAPI::Broodwar->mapWidth() - 1, -1, y, 1, DIR_NE); // South-west
            }

            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
                {
                    updateTileWalkableWidth(x, y);
                    updateTileDistanceToUnwalkable(x, y);
                }
            }
        }

        void updateCollisionVectors(int tileX, int tileY, bool walkable)
        {
            auto updateTile = [&tileX, &tileY](int offsetX, int offsetY, int deltaX, int deltaY)
            {
                int x = tileX + offsetX;
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) return;

                int y = tileY + offsetY;
                if (y < 0 || y >= BWAPI::Broodwar->mapHeight()) return;

                auto &pos = tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()];
                pos.x += deltaX;
                pos.y += deltaY;
            };

            if (walkable)
            {
                updateTile(-1, -1, 10, 10);
                updateTile(-1, 0, 14, 0);
                updateTile(-1, 1, 10, -10);
                updateTile(0, 1, 0, -14);
                updateTile(1, 1, -10, -10);
                updateTile(1, 0, -14, 0);
                updateTile(1, -1, -10, 10);
                updateTile(0, -1, 0, 14);
            }
            else
            {
                updateTile(-1, -1, -10, -10);
                updateTile(-1, 0, -14, 0);
                updateTile(-1, 1, -10, 10);
                updateTile(0, 1, 0, 14);
                updateTile(1, 1, 10, 10);
                updateTile(1, 0, 14, 0);
                updateTile(1, -1, 10, -10);
                updateTile(0, -1, 0, -14);
            }
        }

        // Updates the tile walkability grid based on appearance or disappearance of a building, mineral field, etc.
        bool updateTileWalkability(BWAPI::TilePosition tile, BWAPI::TilePosition size, bool walkable)
        {
            bool updated = false;
            auto bottomRight = tile + size;

            for (int y = tile.y; y < bottomRight.y; y++)
            {
                for (int x = tile.x; x < bottomRight.x; x++)
                {
                    if (tileWalkability[x + y * BWAPI::Broodwar->mapWidth()] != walkable)
                    {
                        tileWalkability[x + y * BWAPI::Broodwar->mapWidth()] = walkable;
                        updateCollisionVectors(x, y, walkable);
                        updated = true;
                    }
                }
            }

            if (!updated) return false;

            // Update distance to unwalkable
            std::set<std::pair<int, int>> changed;
            if (walkable)
            {
                // We need to trace "inside out" from all of the outer tiles

                auto line = [&changed](int x, int deltaX, int y, int deltaY, int dir)
                {
                    short current = 0;

                    // Initialize the current value to the previous tile
                    int prevX = x - deltaX;
                    int prevY = y - deltaY;
                    if (prevX >= 0 && prevX < BWAPI::Broodwar->mapWidth() &&
                        prevY >= 0 && prevY < BWAPI::Broodwar->mapHeight())
                    {
                        current = tileDistanceToUnwalkableDirections[((prevX + prevY * BWAPI::Broodwar->mapWidth()) << 3) + dir];
                    }

                    while (x >= 0 && x < BWAPI::Broodwar->mapWidth() &&
                           y >= 0 && y < BWAPI::Broodwar->mapHeight())
                    {
                        if (tileWalkability[x + y * BWAPI::Broodwar->mapWidth()])
                        {
                            current++;
                        }
                        else
                        {
                            return;
                        }

                        tileDistanceToUnwalkableDirections[((x + y * BWAPI::Broodwar->mapWidth()) << 3) + dir] = current;

                        changed.emplace(std::make_pair(x, y));

                        x += deltaX;
                        y += deltaY;
                    }
                };

                // Up from the bottom, down from the top
                for (int x = tile.x; x < bottomRight.x; x++)
                {
                    line(x, 0, tile.y, 1, DIR_N); // South
                    line(x, 0, bottomRight.y - 1, -1, DIR_S); // North
                    line(x, 1, tile.y, 1, DIR_NW); // South-east
                    line(x, -1, tile.y, 1, DIR_NE); // South-west
                    line(x, 1, bottomRight.y - 1, -1, DIR_SW); // North-east
                    line(x, -1, bottomRight.y - 1, -1, DIR_SE); // North-west
                }

                // Left from the right, right from the left
                for (int y = tile.y; y < bottomRight.y; y++)
                {
                    line(tile.x, 1, y, 0, DIR_W); // East
                    line(bottomRight.x - 1, -1, y, 0, DIR_E); // West
                }

                // Diagonal up from the sides
                for (int y = tile.y; y < (bottomRight.y - 1); y++)
                {
                    line(tile.x, 1, y, -1, DIR_SW); // North-east
                    line(bottomRight.x - 1, -1, y, -1, DIR_SE); // North-west
                }

                // Diagonal down from the sides
                for (int y = tile.y + 1; y < bottomRight.y; y++)
                {
                    line(tile.x, 1, y, 1, DIR_NW); // South-east
                    line(bottomRight.x - 1, -1, y, 1, DIR_NE); // South-west
                }
            }
            else
            {
                // We need to zero all of the now-unwalkable tiles, then trace outwards from the surrounding tiles

                // Zero
                for (int y = tile.y; y < bottomRight.y; y++)
                {
                    auto firstIndex = (tile.x + y * BWAPI::Broodwar->mapWidth());
                    std::fill(tileDistanceToUnwalkable.begin() + firstIndex,
                              tileDistanceToUnwalkable.begin() + firstIndex + size.x,
                              0);

                    std::fill(tileWalkableWidth.begin() + firstIndex,
                              tileWalkableWidth.begin() + firstIndex + size.x,
                              0);

                    auto firstDirIndex = firstIndex << 3U;
                    std::fill(tileDistanceToUnwalkableDirections.begin() + firstDirIndex,
                              tileDistanceToUnwalkableDirections.begin() + firstDirIndex + size.x * 8,
                              0);
                }

                auto line = [&changed](int x, int deltaX, int y, int deltaY, int dir)
                {
                    short current = 0;
                    x += deltaX;
                    y += deltaY;
                    while (x >= 0 && x < BWAPI::Broodwar->mapWidth() &&
                           y >= 0 && y < BWAPI::Broodwar->mapHeight())
                    {
                        if (tileWalkability[x + y * BWAPI::Broodwar->mapWidth()])
                        {
                            current++;
                        }
                        else
                        {
                            return;
                        }

                        tileDistanceToUnwalkableDirections[((x + y * BWAPI::Broodwar->mapWidth()) << 3) + dir] = current;

                        changed.emplace(std::make_pair(x, y));

                        x += deltaX;
                        y += deltaY;
                    }
                };

                // Up from the top, down from the bottom
                for (int x = tile.x; x < bottomRight.x; x++)
                {
                    line(x, 0, bottomRight.y - 1, 1, DIR_N); // South
                    line(x, 0, tile.y, -1, DIR_S); // North
                    line(x, 1, bottomRight.y - 1, 1, DIR_NW); // South-east
                    line(x, -1, bottomRight.y - 1, 1, DIR_NE); // South-west
                    line(x, 1, tile.y, -1, DIR_SW); // North-east
                    line(x, -1, tile.y, -1, DIR_SE); // North-west
                }

                // Left from the left, right from the right
                for (int y = tile.y; y < bottomRight.y; y++)
                {
                    line(bottomRight.x - 1, 1, y, 0, DIR_W); // East
                    line(tile.x, -1, y, 0, DIR_E); // West
                }

                // Diagonal down from the sides
                for (int y = tile.y; y < (bottomRight.y - 1); y++)
                {
                    line(bottomRight.x - 1, 1, y, 1, DIR_NW); // South-east
                    line(tile.x, -1, y, 1, DIR_NE); // South-west
                }

                // Diagonal up from the sides
                for (int y = tile.y + 1; y < bottomRight.y; y++)
                {
                    line(bottomRight.x - 1, 1, y, -1, DIR_SW); // North-east
                    line(tile.x, -1, y, -1, DIR_SE); // North-west
                }
            }

            for (const auto &xy : changed)
            {
                updateTileDistanceToUnwalkable(xy.first, xy.second);
                updateTileWalkableWidth(xy.first, xy.second);
            }

            return updated;
        }
    }

    void initializeWalkability()
    {
        tileWalkability.clear();
        tileDistanceToUnwalkableDirections.clear();
        tileCollisionVector.clear();
        tileCollisionVector.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), BWAPI::Position(0, 0));
        tileWalkabilityUpdated = true;

        tileWalkability.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());

        // Start by checking the normal BWAPI walkability
        for (int tileX = 0; tileX < BWAPI::Broodwar->mapWidth(); tileX++)
        {
            for (int tileY = 0; tileY < BWAPI::Broodwar->mapHeight(); tileY++)
            {
                bool walkable = true;
                for (int walkX = 0; walkX < 4; walkX++)
                {
                    for (int walkY = 0; walkY < 4; walkY++)
                    {
                        if (!BWAPI::Broodwar->isWalkable((tileX << 2U) + walkX, (tileY << 2U) + walkY))
                        {
                            walkable = false;
                            updateCollisionVectors(tileX, tileY, false);
                            goto breakInnerLoop;
                        }
                    }
                }
                breakInnerLoop:
                tileWalkability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = walkable;
            }
        }

        // For collision vectors, mark the edges of the map as unwalkable
        for (int tileX = -1; tileX <= BWAPI::Broodwar->mapWidth(); tileX++)
        {
            updateCollisionVectors(tileX, -1, false);
            updateCollisionVectors(tileX, BWAPI::Broodwar->mapHeight(), false);
        }
        for (int tileY = 0; tileY <= BWAPI::Broodwar->mapHeight(); tileY++)
        {
            updateCollisionVectors(-1, tileY, false);
            updateCollisionVectors(BWAPI::Broodwar->mapWidth(), tileY, false);
        }

        // Initialize distances to unwalkable tiles
        initializeDistanceToUnwalkable();

        // Add our start position
        updateTileWalkability(BWAPI::Broodwar->self()->getStartLocation(), BWAPI::UnitTypes::Protoss_Nexus.tileSize(), false);

        // Add static neutrals
        for (auto neutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (neutral->getType().isCritter()) continue;
            if (neutral->isFlying()) continue;

            updateTileWalkability(neutral->getInitialTilePosition(), neutral->getType().tileSize(), false);
        }

        dumpWalkability();
    }

    // Writes the tile walkability grid to CherryVis
    void dumpWalkability()
    {
#if CHERRYVIS_ENABLED
        if (!tileWalkabilityUpdated) return;

        tileWalkabilityUpdated = false;

        // Dump to CherryVis
        std::vector<long> tileWalkabilityCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                tileWalkabilityCVis[x + y * BWAPI::Broodwar->mapWidth()] = tileWalkability[x + y * BWAPI::Broodwar->mapWidth()];
            }
        }

        CherryVis::addHeatmap("TileWalkable", tileWalkabilityCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

#if UNWALKABLE_DIRECTION_HEATMAP_ENABLED
        for (int dir = 0; dir < 8; dir++)
        {
            std::vector<long> tileDistanceToUnwalkableDirectionsCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
                {
                    tileDistanceToUnwalkableDirectionsCVis[x + y * BWAPI::Broodwar->mapWidth()] =
                            tileDistanceToUnwalkableDirections[((x + y * BWAPI::Broodwar->mapWidth()) << 3U) + dir];
                }
            }

            std::ostringstream key;
            key << "TileDistToUnwalkable_";
            switch (dir)
            {
                case DIR_N:
                    key << "N";
                    break;
                case DIR_S:
                    key << "S";
                    break;
                case DIR_E:
                    key << "E";
                    break;
                case DIR_W:
                    key << "W";
                    break;
                case DIR_NE:
                    key << "NE";
                    break;
                case DIR_NW:
                    key << "NW";
                    break;
                case DIR_SE:
                    key << "SE";
                    break;
                case DIR_SW:
                    key << "SW";
                    break;
                default:
                    key << "?";
                    break;
            }

            CherryVis::addHeatmap(key.str(), tileDistanceToUnwalkableDirectionsCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
        }
#endif

        std::vector<long> tileDistanceToUnwalkableCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                tileDistanceToUnwalkableCVis[x + y * BWAPI::Broodwar->mapWidth()] =
                        tileDistanceToUnwalkable[x + y * BWAPI::Broodwar->mapWidth()];
            }
        }

        CherryVis::addHeatmap("TileDistToUnwalkable", tileDistanceToUnwalkableCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

        std::vector<long> tileWalkableWidthCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                tileWalkableWidthCVis[x + y * BWAPI::Broodwar->mapWidth()] =
                        tileWalkableWidth[x + y * BWAPI::Broodwar->mapWidth()];
            }
        }

        CherryVis::addHeatmap("TileWalkableWidth", tileWalkableWidthCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

        std::vector<long> tileCollisionVectorXCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                tileCollisionVectorXCVis[x + y * BWAPI::Broodwar->mapWidth()] = tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()].x;
            }
        }

        CherryVis::addHeatmap("TileCollisionVectorX", tileCollisionVectorXCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

        std::vector<long> tileCollisionVectorYCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                tileCollisionVectorYCVis[x + y * BWAPI::Broodwar->mapWidth()] = tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()].y;
            }
        }

        CherryVis::addHeatmap("TileCollisionVectorY", tileCollisionVectorYCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
    }

    void onUnitCreated_Walkability(const Unit &unit)
    {
        // Units that affect tile walkability
        // Skip on frame 0, since we handle static neutrals and our base explicitly
        // Skip refineries, since creation of a refinery does not affect tile walkability (there was already a geyser)
        if (BWAPI::Broodwar->getFrameCount() > 0 && unit->type.isBuilding() && !unit->isFlying && !unit->type.isRefinery())
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->type.tileSize(), false))
            {
                PathFinding::addBlockingObject(unit->type, unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }

        // FIXME: Check tile walkability for mineral fields
    }

    void onUnitDiscover(BWAPI::Unit unit)
    {
        // Update tile walkability for discovered mineral fields
        // TODO: Is this even needed?
        if (BWAPI::Broodwar->getFrameCount() > 0 && unit->getType().isMineralField())
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->getType().tileSize(), false))
            {
                PathFinding::addBlockingObject(unit->getType(), unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }
    }

    void onUnitDestroy_Walkability(const Unit &unit)
    {
        // Units that affect tile walkability
        // Skip refineries, since destruction of a refinery does not affect tile walkability (there will still be a geyser)
        if (unit->type.isBuilding() && !unit->isFlying && !unit->type.isRefinery())
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->type.tileSize(), true))
            {
                PathFinding::removeBlockingObject(unit->type, unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }
    }

    void onUnitDestroy_Walkability(BWAPI::Unit unit)
    {
        // Units that affect tile walkability
        if (unit->getType().isMineralField() ||
            (unit->getType().isBuilding() && !unit->isFlying() && !unit->getType().isRefinery()) ||
            (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getPlayer() == BWAPI::Broodwar->neutral()))
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->getType().tileSize(), true))
            {
                PathFinding::removeBlockingObject(unit->getType(), unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }
    }

    void onBuildingLifted(BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        if (updateTileWalkability(tile, type.tileSize(), true))
        {
            PathFinding::removeBlockingObject(type, tile);
            tileWalkabilityUpdated = true;
        }
    }

    void onBuildingLanded(BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        if (updateTileWalkability(tile, type.tileSize(), false))
        {
            PathFinding::addBlockingObject(type, tile);
            tileWalkabilityUpdated = true;
        }
    }

    bool isWalkable(BWAPI::TilePosition pos)
    {
        return isWalkable(pos.x, pos.y);
    }

    bool isWalkable(int x, int y)
    {
        return tileWalkability[x + y * BWAPI::Broodwar->mapWidth()];
    }

    unsigned short unwalkableProximity(int x, int y)
    {
        return tileDistanceToUnwalkable[x + y * BWAPI::Broodwar->mapWidth()];
    }

    unsigned short walkableWidth(int x, int y)
    {
        return tileWalkableWidth[x + y * BWAPI::Broodwar->mapWidth()];
    }

    BWAPI::Position collisionVector(int x, int y)
    {
        return tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()];
    }
}
