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
#define DRAW_COLLISION_VECTORS false
#endif

namespace Map
{
    namespace
    {
        int mapWidth;
        int mapHeight;
        
        std::vector<bool> tileTerrainWalkability;
        std::vector<bool> tileWalkability;
        std::vector<unsigned short> tileDistanceToUnwalkableDirections;
        std::vector<unsigned short> tileDistanceToUnwalkable;
        std::vector<unsigned short> tileWalkableWidth;
        std::vector<BWAPI::Position> tileCollisionVector;
        bool tileWalkabilityUpdated;

        void updateTileDistanceToUnwalkable(int x, int y)
        {
            auto index = (x + y * mapWidth);
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
            auto index = (x + y * mapWidth);
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
            tileDistanceToUnwalkableDirections.resize(mapWidth * mapHeight * 8);
            tileDistanceToUnwalkable.resize(mapWidth * mapHeight);
            tileWalkableWidth.resize(mapWidth * mapHeight);

            auto line = [](int x, int deltaX, int y, int deltaY, int dir)
            {
                short current = 0;
                while (x >= 0 && x < mapWidth &&
                       y >= 0 && y < mapHeight)
                {
                    if (tileWalkability[x + y * mapWidth])
                    {
                        current++;
                    }
                    else
                    {
                        current = 0;
                    }

                    tileDistanceToUnwalkableDirections[((x + y * mapWidth) << 3) + dir] = current;

                    x += deltaX;
                    y += deltaY;
                }
            };

            for (int x = 0; x < mapWidth; x++)
            {
                line(x, 0, 0, 1, DIR_N); // South
                line(x, 0, mapHeight - 1, -1, DIR_S); // North
                line(x, 1, 0, 1, DIR_NW); // South-east
                line(x, -1, 0, 1, DIR_NE); // South-west
                line(x, 1, mapHeight - 1, -1, DIR_SW); // North-east
                line(x, -1, mapHeight - 1, -1, DIR_SE); // North-west
            }

            for (int y = 0; y < mapHeight; y++)
            {
                line(0, 1, y, 0, DIR_W); // East
                line(mapWidth - 1, -1, y, 0, DIR_E); // West
            }

            for (int y = 0; y < (mapHeight - 1); y++)
            {
                line(0, 1, y, -1, DIR_SW); // North-east
                line(mapWidth - 1, -1, y, -1, DIR_SE); // North-west
            }

            for (int y = 1; y < mapHeight; y++)
            {
                line(0, 1, y, 1, DIR_NW); // South-east
                line(mapWidth - 1, -1, y, 1, DIR_NE); // South-west
            }

            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
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
                if (x < 0 || x >= mapWidth) return;

                int y = tileY + offsetY;
                if (y < 0 || y >= mapHeight) return;

                auto &pos = tileCollisionVector[x + y * mapWidth];
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
                    if (tileWalkability[x + y * mapWidth] != walkable)
                    {
                        tileWalkability[x + y * mapWidth] = walkable;
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
                    if (prevX >= 0 && prevX < mapWidth &&
                        prevY >= 0 && prevY < mapHeight)
                    {
                        current = tileDistanceToUnwalkableDirections[((prevX + prevY * mapWidth) << 3) + dir];
                    }

                    while (x >= 0 && x < mapWidth &&
                           y >= 0 && y < mapHeight)
                    {
                        if (tileWalkability[x + y * mapWidth])
                        {
                            current++;
                        }
                        else
                        {
                            return;
                        }

                        tileDistanceToUnwalkableDirections[((x + y * mapWidth) << 3) + dir] = current;

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
                    auto firstIndex = (tile.x + y * mapWidth);
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
                    while (x >= 0 && x < mapWidth &&
                           y >= 0 && y < mapHeight)
                    {
                        if (tileWalkability[x + y * mapWidth])
                        {
                            current++;
                        }
                        else
                        {
                            return;
                        }

                        tileDistanceToUnwalkableDirections[((x + y * mapWidth) << 3) + dir] = current;

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
        mapWidth = BWAPI::Broodwar->mapWidth();
        mapHeight = BWAPI::Broodwar->mapHeight();
        
        tileTerrainWalkability.clear();
        tileWalkability.clear();
        tileDistanceToUnwalkableDirections.clear();
        tileCollisionVector.clear();
        tileCollisionVector.resize(mapWidth * mapHeight, BWAPI::Position(0, 0));
        tileWalkabilityUpdated = true;

        tileTerrainWalkability.resize(mapWidth * mapHeight);
        tileWalkability.resize(mapWidth * mapHeight);

        // Start by checking the normal BWAPI walkability
        for (int tileX = 0; tileX < mapWidth; tileX++)
        {
            for (int tileY = 0; tileY < mapHeight; tileY++)
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
                tileTerrainWalkability[tileX + tileY * mapWidth] = walkable;
                tileWalkability[tileX + tileY * mapWidth] = walkable;
            }
        }

        // For collision vectors, mark the edges of the map as unwalkable
        for (int tileX = -1; tileX <= mapWidth; tileX++)
        {
            updateCollisionVectors(tileX, -1, false);
            updateCollisionVectors(tileX, mapHeight, false);
        }
        for (int tileY = 0; tileY <= mapHeight; tileY++)
        {
            updateCollisionVectors(-1, tileY, false);
            updateCollisionVectors(mapWidth, tileY, false);
        }

        // Initialize distances to unwalkable tiles
        initializeDistanceToUnwalkable();

        // Add our start position
        updateTileWalkability(BWAPI::Broodwar->self()->getStartLocation(), BWAPI::UnitTypes::Protoss_Nexus.tileSize(), false);

        // Add static neutrals
        auto handleStaticNeutral = [&](BWAPI::TilePosition tile, BWAPI::TilePosition size)
        {
            // Update terrain walkability directly
            auto bottomRight = tile + size;
            for (int y = tile.y; y < bottomRight.y; y++)
            {
                for (int x = tile.x; x < bottomRight.x; x++)
                {
                    tileTerrainWalkability[x + y * mapWidth] = false;
                }
            }

            // Update our main walkability through the update method so other related structures are updated
            updateTileWalkability(tile, size, false);
        };

        for (auto neutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (neutral->getType().isCritter()) continue;
            if (neutral->isFlying()) continue;

            // Eggs might not be aligned to tiles, so add any tiles they overlap
            if (neutral->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg)
            {
                BWAPI::Position bottomRight(neutral->getInitialPosition() + BWAPI::Position(neutral->getType().width(), neutral->getType().height()));
                handleStaticNeutral(neutral->getInitialTilePosition(), BWAPI::TilePosition(bottomRight) - neutral->getInitialTilePosition());
            } else {
                handleStaticNeutral(neutral->getInitialTilePosition(), neutral->getType().tileSize());
            }
        }

        dumpWalkability();

        // Terrain walkability is static, so dump it only at initialization
#if CHERRYVIS_ENABLED
        std::vector<long> tileTerrainWalkabilityCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                tileTerrainWalkabilityCVis[x + y * mapWidth] = tileTerrainWalkability[x + y * mapWidth];
            }
        }

        CherryVis::addHeatmap("TileTerrainWalkable", tileTerrainWalkabilityCVis, mapWidth, mapHeight);
#endif
    }

    // Writes the tile walkability grid to CherryVis
    void dumpWalkability()
    {
#if CHERRYVIS_ENABLED
#if DRAW_COLLISION_VECTORS
        // Draw tile collision vectors on first frame only, as they are too intensive to draw every frame
        if (currentFrame == 0)
        {
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    if (tileCollisionVector[x + y * mapWidth].x == 0 && tileCollisionVector[x + y * mapWidth].y == 0) continue;

                    auto color = CherryVis::DrawColor::Green;
                    if (tileCollisionVector[x + y * mapWidth].x <= 0 && tileCollisionVector[x + y * mapWidth].y <= 0)
                    {
                        color = CherryVis::DrawColor::Red;
                    }
                    else if (tileCollisionVector[x + y * mapWidth].x < 0 && tileCollisionVector[x + y * mapWidth].y > 0)
                    {
                        color = CherryVis::DrawColor::Yellow;
                    }
                    if (tileCollisionVector[x + y * mapWidth].x > 0 && tileCollisionVector[x + y * mapWidth].y < 0)
                    {
                        color = CherryVis::DrawColor::Cyan;
                    }
                    CherryVis::drawLine(
                            x * 32 + 16,
                            y * 32 + 16,
                            x * 32 + 16 + tileCollisionVector[x + y * mapWidth].x,
                            y * 32 + 16 + tileCollisionVector[x + y * mapWidth].y,
                            color);
                }
            }
        }
#endif

        if (!tileWalkabilityUpdated) return;

        tileWalkabilityUpdated = false;

        // Dump to CherryVis
        std::vector<long> tileWalkabilityCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                tileWalkabilityCVis[x + y * mapWidth] = tileWalkability[x + y * mapWidth];
            }
        }

        CherryVis::addHeatmap("TileWalkable", tileWalkabilityCVis, mapWidth, mapHeight);

#if UNWALKABLE_DIRECTION_HEATMAP_ENABLED
        for (int dir = 0; dir < 8; dir++)
        {
            std::vector<long> tileDistanceToUnwalkableDirectionsCVis(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    tileDistanceToUnwalkableDirectionsCVis[x + y * mapWidth] =
                            tileDistanceToUnwalkableDirections[((x + y * mapWidth) << 3U) + dir];
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

            CherryVis::addHeatmap(key.str(), tileDistanceToUnwalkableDirectionsCVis, mapWidth, mapHeight);
        }
#endif

        std::vector<long> tileDistanceToUnwalkableCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                tileDistanceToUnwalkableCVis[x + y * mapWidth] =
                        tileDistanceToUnwalkable[x + y * mapWidth];
            }
        }

        CherryVis::addHeatmap("TileDistToUnwalkable", tileDistanceToUnwalkableCVis, mapWidth, mapHeight);

        std::vector<long> tileWalkableWidthCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                tileWalkableWidthCVis[x + y * mapWidth] =
                        tileWalkableWidth[x + y * mapWidth];
            }
        }

        CherryVis::addHeatmap("TileWalkableWidth", tileWalkableWidthCVis, mapWidth, mapHeight);

        std::vector<long> tileCollisionVectorXCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                tileCollisionVectorXCVis[x + y * mapWidth] = tileCollisionVector[x + y * mapWidth].x;
            }
        }

        CherryVis::addHeatmap("TileCollisionVectorX", tileCollisionVectorXCVis, mapWidth, mapHeight);

        std::vector<long> tileCollisionVectorYCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                tileCollisionVectorYCVis[x + y * mapWidth] = tileCollisionVector[x + y * mapWidth].y;
            }
        }

        CherryVis::addHeatmap("TileCollisionVectorY", tileCollisionVectorYCVis, mapWidth, mapHeight);
#endif
    }

    void onUnitCreated_Walkability(const Unit &unit)
    {
        // Units that affect tile walkability
        // Skip on frame 0, since we handle static neutrals and our base explicitly
        // Skip refineries, since creation of a refinery does not affect tile walkability (there was already a geyser)
        if (currentFrame > 0 && unit->type.isBuilding() && !unit->isFlying && !unit->type.isRefinery())
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
        if (currentFrame > 0 && unit->getType().isMineralField())
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
        return tileWalkability[x + y * mapWidth];
    }

    bool isTerrainWalkable(int x, int y)
    {
        return tileTerrainWalkability[x + y * mapWidth];
    }

    unsigned short unwalkableProximity(int x, int y)
    {
        return tileDistanceToUnwalkable[x + y * mapWidth];
    }

    unsigned short walkableWidth(int x, int y)
    {
        return tileWalkableWidth[x + y * mapWidth];
    }

    BWAPI::Position collisionVector(int x, int y)
    {
        return tileCollisionVector[x + y * mapWidth];
    }
}
