#include "Block.h"

#include "Map.h"

namespace
{
    bool
    overlaps(BWAPI::TilePosition firstTopLeft, int firstWidth, int firstHeight, BWAPI::TilePosition secondtopLeft, int secondWidth, int secondHeight)
    {
        return firstTopLeft.x < secondtopLeft.x + secondWidth // first not right of second
               && firstTopLeft.y < secondtopLeft.y + secondHeight // first not below second
               && firstTopLeft.x + firstWidth > secondtopLeft.x // first not left of second
               && firstTopLeft.y + firstHeight > secondtopLeft.y; // first not above second
    }
}

BWAPI::Position Block::center() const
{
    if (!topLeft.isValid()) return BWAPI::Positions::Invalid;

    return BWAPI::Position(topLeft) + BWAPI::Position(width() * 16, height() * 16);
}

bool Block::tilesUsed(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
    if (!overlaps(tile, size.x, size.y, topLeft, width(), height())) return false;

    auto removeOverlapping = [&tile, &size](std::vector<Location> &locations, int width, int height)
    {
        for (auto it = locations.begin(); it != locations.end();)
        {
            if (overlaps(tile, size.x, size.y, it->tile, width, height))
            {
                it = locations.erase(it);
            }
            else
            {
                it++;
            }
        }
    };

    removeOverlapping(small, 2, 2);
    removeOverlapping(medium, 3, 2);
    removeOverlapping(large, 4, 3);

    return true;
}

bool Block::tilesFreed(BWAPI::TilePosition tile, BWAPI::TilePosition size)
{
    if (!overlaps(tile, size.x, size.y, topLeft, width(), height())) return false;

    // The freed tiles may free up some of the initial build locations, so place them again
    placeLocations();
    removeUsed();
    return true;
}

bool Block::place(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability)
{
    auto checkTile = [&](int tileX, int tileY)
    {
        if (tileX >= BWAPI::Broodwar->mapWidth()) return false;
        if (tileY >= BWAPI::Broodwar->mapHeight()) return false;
        if (tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] > 0) return false;
        if (tileX == 0 && !allowLeftEdge()) return false;
        if (tileX == (BWAPI::Broodwar->mapWidth() - 1) && !allowRightEdge()) return false;
        if (tileY == 0 && !allowTopEdge()) return false;
        if (tileX == 0 && tileY == 0 && !allowCorner()) return false;
        if (tileX == (BWAPI::Broodwar->mapWidth() - 1) && tileY == 0 && !allowCorner()) return false;

        return true;
    };

    if (!checkTile(tile.x, tile.y)) return false;
    if (!checkTile(tile.x + width() - 1, tile.y)) return false;
    if (!checkTile(tile.x + width() - 1, tile.y + height() - 1)) return false;
    if (!checkTile(tile.x, tile.y + height() - 1)) return false;

    for (int tileX = tile.x; tileX < tile.x + width(); tileX++)
    {
        for (int tileY = tile.y; tileY < tile.y + height(); tileY++)
        {
            if (!checkTile(tileX, tileY)) return false;
        }
    }

    // Can be placed here, mark the tiles
    for (int tileX = tile.x - 1; tileX <= tile.x + width(); tileX++)
    {
        for (int tileY = tile.y - 1; tileY <= tile.y + height(); tileY++)
        {
            if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;

            if (tileX == tile.x - 1 || tileY == tile.y - 1 || tileX == tile.x + width() || tileY == tile.y + height())
            {
                tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] |= 8U;
            }
            else
            {
                tileAvailability[tileX + tileY * BWAPI::Broodwar->mapWidth()] |= 4U;
            }
        }
    }

    return true;
}

bool Block::placeStartBlock(std::vector<BWAPI::TilePosition> &usedTiles,
                            std::vector<BWAPI::TilePosition> &borderTiles,
                            std::vector<unsigned int> &tileAvailability)
{
    // Tiles around the start position are not checked for availability
    std::set<BWAPI::TilePosition> ignoreAvailability;
    for (int tileX = BWAPI::Broodwar->self()->getStartLocation().x - 3; tileX <= BWAPI::Broodwar->self()->getStartLocation().x + 6; tileX++)
    {
        for (int tileY = BWAPI::Broodwar->self()->getStartLocation().y - 3; tileY <= BWAPI::Broodwar->self()->getStartLocation().y + 6; tileY++)
        {
            if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;
            ignoreAvailability.insert(BWAPI::TilePosition(tileX, tileY));
        }
    }

    auto checkTile = [&](BWAPI::TilePosition tile)
    {
        if (tile.x >= BWAPI::Broodwar->mapWidth()) return false;
        if (tile.y >= BWAPI::Broodwar->mapHeight()) return false;
        if (ignoreAvailability.find(tile) == ignoreAvailability.end()
            && tileAvailability[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] > 0)
        {
            return false;
        }
        if (tile.x == 0 && !allowLeftEdge()) return false;
        if (tile.x == (BWAPI::Broodwar->mapWidth() - 1) && !allowRightEdge()) return false;
        if (tile.y == 0 && !allowTopEdge()) return false;
        if (tile.x == 0 && tile.y == 0 && !allowCorner()) return false;
        if (tile.x == (BWAPI::Broodwar->mapWidth() - 1) && tile.y == 0 && !allowCorner()) return false;

        return true;
    };

    for (auto tile : usedTiles)
    {
        if (!checkTile(tile)) return false;
    }

    for (auto tile : usedTiles)
    {
        tileAvailability[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] |= 4U;
    }
    for (auto tile : borderTiles)
    {
        tileAvailability[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] |= 8U;
    }

    return true;
}

void Block::removeUsed()
{
    auto removeUsed = [](std::vector<Location> &locations, int width, int height)
    {
        for (auto it = locations.begin(); it != locations.end();)
        {
            bool used = false;
            for (int tileX = it->tile.x; tileX < it->tile.x + width; tileX++)
            {
                for (int tileY = it->tile.y; tileY < it->tile.y + height; tileY++)
                {
                    if (!Map::isWalkable(tileX, tileY))
                    {
                        used = true;
                        goto breakOuter;
                    }
                }
            }
            breakOuter:;

            if (used)
            {
                it = locations.erase(it);
            }
            else
            {
                it++;
            }
        }
    };

    removeUsed(small, 2, 2);
    removeUsed(medium, 3, 2);
    removeUsed(large, 4, 3);
}