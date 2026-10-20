#include "Block.h"

#include "Map.h"
#include "Geo.h"

BWAPI::Position BuildBlock::midpoint() const
{
    if (!cornerTL.isValid()) return BWAPI::Positions::Invalid;

    return BWAPI::Position(cornerTL) + BWAPI::Position(width() * 16, height() * 16);
}

bool BuildBlock::tilesReserved(BWAPI::TilePosition tilePos, BWAPI::TilePosition size, bool permanent)
{
    if (!Geom::Overlaps(tilePos, size.x, size.y, cornerTL, width(), height())) return false;

    auto removeOverlapping = [&tilePos, &size](std::vector<Location> &locations, int width, int height)
    {
        for (auto iterRef = locations.begin(); iterRef != locations.end();)
        {
            if (Geom::Overlaps(tilePos, size.x, size.y, iterRef->tilePos, width, height))
            {
                iterRef = locations.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }
    };

    removeOverlapping(small, 2, 2);
    removeOverlapping(medium, 3, 2);
    removeOverlapping(large, 4, 3);

    if (permanent)
    {
        permanentTileReservations.emplace_back(tilePos, size);
    }

    return true;
}

bool BuildBlock::tilesUsed(BWAPI::TilePosition tilePos, BWAPI::TilePosition size)
{
    return tilesReserved(tilePos, size);
}

bool BuildBlock::tilesFreed(BWAPI::TilePosition tilePos, BWAPI::TilePosition size)
{
    if (!Geom::Overlaps(tilePos, size.x, size.y, cornerTL, width(), height())) return false;

    /* The freed tiles may free up some of the initial build locations, so place them again */
    placeLocations();
    removeUsed();

    /* However, we may have some permanent reservations, so re-apply them */
    for (const auto &permanentTileReservation : permanentTileReservations)
    {
        tilesReserved(permanentTileReservation.first, permanentTileReservation.second);
    }

    return true;
}

bool BuildBlock::place(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) const
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

    if (!checkTile(tilePos.x, tilePos.y)) return false;
    if (!checkTile(tilePos.x + width() - 1, tilePos.y)) return false;
    if (!checkTile(tilePos.x + width() - 1, tilePos.y + height() - 1)) return false;
    if (!checkTile(tilePos.x, tilePos.y + height() - 1)) return false;

    for (int tileX = tilePos.x; tileX < tilePos.x + width(); tileX++)
    {
        for (int tileY = tilePos.y; tileY < tilePos.y + height(); tileY++)
        {
            if (!checkTile(tileX, tileY)) return false;
        }
    }

    /* Can be placed here, mark the tiles */
    for (int tileX = tilePos.x - 1; tileX <= tilePos.x + width(); tileX++)
    {
        for (int tileY = tilePos.y - 1; tileY <= tilePos.y + height(); tileY++)
        {
            if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;

            if (tileX == tilePos.x - 1 || tileY == tilePos.y - 1 || tileX == tilePos.x + width() || tileY == tilePos.y + height())
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

bool BuildBlock::placeStartBlock(BWAPI::TilePosition startPosition,
                            std::vector<BWAPI::TilePosition> &usedTiles,
                            std::vector<BWAPI::TilePosition> &borderTiles,
                            std::vector<unsigned int> &tileAvailability) const
{
    /* Tiles around the start position are not checked for availability */
    std::set<BWAPI::TilePosition> ignoreAvailability;
    for (int tileX = startPosition.x - 2; tileX <= startPosition.x + 5; tileX++)
    {
        for (int tileY = startPosition.y - 2; tileY <= startPosition.y + 4; tileY++)
        {
            if (tileX < 0 || tileY < 0 || tileX >= BWAPI::Broodwar->mapWidth() || tileY >= BWAPI::Broodwar->mapHeight()) continue;
            ignoreAvailability.insert(BWAPI::TilePosition(tileX, tileY));
        }
    }

    auto checkTile = [&](BWAPI::TilePosition tilePos)
    {
        if (tilePos.x >= BWAPI::Broodwar->mapWidth()) return false;
        if (tilePos.y >= BWAPI::Broodwar->mapHeight()) return false;
        if (!ignoreAvailability.contains(tilePos))
        {
            if (tileAvailability[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] > 0) return false;
        }
        else
        {
            if (tileAvailability[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] == 1) return false;
        }
        if (tilePos.x == 0 && !allowLeftEdge()) return false;
        if (tilePos.x == (BWAPI::Broodwar->mapWidth() - 1) && !allowRightEdge()) return false;
        if (tilePos.y == 0 && !allowTopEdge()) return false;
        if (tilePos.x == 0 && tilePos.y == 0 && !allowCorner()) return false;
        if (tilePos.x == (BWAPI::Broodwar->mapWidth() - 1) && tilePos.y == 0 && !allowCorner()) return false;

        return true;
    };

    for (auto tilePos : usedTiles)
    {
        if (!checkTile(tilePos)) return false;
    }

    for (auto tilePos : usedTiles)
    {
        tileAvailability[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] |= 4U;
    }
    for (auto tilePos : borderTiles)
    {
        tileAvailability[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] |= 8U;
    }

    return true;
}

void BuildBlock::removeUsed()
{
    auto removeUsed = [](std::vector<Location> &locations, int width, int height)
    {
        for (auto iterRef = locations.begin(); iterRef != locations.end();)
        {
            bool used = false;
            for (int tileX = iterRef->tilePos.x; tileX < iterRef->tilePos.x + width; tileX++)
            {
                for (int tileY = iterRef->tilePos.y; tileY < iterRef->tilePos.y + height; tileY++)
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
                iterRef = locations.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }
    };

    removeUsed(small, 2, 2);
    removeUsed(medium, 3, 2);
    removeUsed(large, 4, 3);
}