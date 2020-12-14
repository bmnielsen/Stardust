#include "NoGoAreas.h"

#include "Geo.h"

namespace
{
    struct NoGoAreaExpiry
    {
        int frame;
        BWAPI::Bullet bullet;
        Unit unit;

        NoGoAreaExpiry(int frame) : frame(frame), bullet(nullptr), unit(nullptr) {}
        NoGoAreaExpiry(BWAPI::Bullet bullet) : frame(-1), bullet(bullet), unit(nullptr) {}
        NoGoAreaExpiry(Unit unit) : frame(-1), bullet(nullptr), unit(unit) {}

        bool isExpired()
        {
            if (frame != -1) return BWAPI::Broodwar->getFrameCount() >= frame;
            if (bullet) return !bullet->exists();
            return !unit->exists();
        }
    };

    std::vector<int> noGoAreaTiles;
    bool noGoAreaTilesUpdated;
    std::vector<std::pair<std::set<BWAPI::TilePosition>, NoGoAreaExpiry>> noGoAreasWithExpiration;
    std::map<int, std::set<BWAPI::TilePosition>> tilesInRadiusCache;

    // Writes the tile no go areas to CherryVis
    void dumpNoGoAreaTiles()
    {
#if CHERRYVIS_ENABLED
        // Dump to CherryVis
        std::vector<long> noGoAreaTilesCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
        {
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                noGoAreaTilesCVis[x + y * BWAPI::Broodwar->mapWidth()] = noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()];
            }
        }

        CherryVis::addHeatmap("NoGoAreas", noGoAreaTilesCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
    }

    void add(std::set<BWAPI::TilePosition> &tiles)
    {
        for (const auto &tile : tiles)
        {
            noGoAreaTiles[tile.x + tile.y * BWAPI::Broodwar->mapWidth()]++;
        }

        noGoAreaTilesUpdated = true;
    }

    void remove(std::set<BWAPI::TilePosition> &tiles)
    {
        for (const auto &tile : tiles)
        {
            noGoAreaTiles[tile.x + tile.y * BWAPI::Broodwar->mapWidth()]--;
        }

        noGoAreaTilesUpdated = true;
    }

    std::set<BWAPI::TilePosition> generateCircle(BWAPI::Position origin, int radius)
    {
        std::set<BWAPI::TilePosition> &positions = tilesInRadiusCache[radius];

        if (positions.empty())
        {
            for (int x = radius; x <= radius; x++)
            {
                for (int y = -radius; y <= radius; y++)
                {
                    if (Geo::ApproximateDistance(0, x, 0, y) <= radius)
                    {
                        positions.insert(BWAPI::TilePosition(x >> 5U, y >> 5U));
                    }
                }
            }
        }

        auto tileOrigin = BWAPI::TilePosition(origin);

        std::set<BWAPI::TilePosition> result;
        for (const auto &tile : positions)
        {
            auto here = tile + tileOrigin;
            if (here.isValid()) result.insert(here);
        }

        return result;
    }
}

namespace NoGoAreas
{
    void initialize()
    {
        noGoAreaTiles.clear();
        noGoAreaTiles.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        noGoAreaTilesUpdated = true; // So we get an initial null state
        noGoAreasWithExpiration.clear();

        update();
    }

    void update()
    {
        for (auto it = noGoAreasWithExpiration.begin(); it != noGoAreasWithExpiration.end(); )
        {
            if (it->second.isExpired())
            {
                remove(it->first);
                it = noGoAreasWithExpiration.erase(it);
            }
            else
            {
                it++;
            }
        }

        if (noGoAreaTilesUpdated)
        {
            dumpNoGoAreaTiles();
            noGoAreaTilesUpdated = false;
        }
    }

    void addBox(BWAPI::TilePosition topLeft, BWAPI::TilePosition size)
    {
        for (int x = topLeft.x; x < topLeft.x + size.x; x++)
        {
            if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

            for (int y = topLeft.y; y < topLeft.y + size.y; y++)
            {
                if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()]++;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    void removeBox(BWAPI::TilePosition topLeft, BWAPI::TilePosition size)
    {
        for (int x = topLeft.x; x < topLeft.x + size.x; x++)
        {
            if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

            for (int y = topLeft.y; y < topLeft.y + size.y; y++)
            {
                if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()]--;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    void addCircle(BWAPI::Position origin, int radius, int expireFrames)
    {
        auto tiles = generateCircle(origin, radius);
        add(tiles);
        noGoAreasWithExpiration.emplace_back(std::make_pair(std::move(tiles), expireFrames));
    }

    bool isNoGo(BWAPI::TilePosition pos)
    {
        return noGoAreaTiles[pos.x + pos.y * BWAPI::Broodwar->mapWidth()] > 0;
    }

    bool isNoGo(int x, int y)
    {
        return noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()] > 0;
    }
}
