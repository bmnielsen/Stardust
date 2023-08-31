#include "NoGoAreas.h"

#include "Geo.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_HEATMAPS true
#endif

namespace
{
    struct NoGoAreaExpiry
    {
        int frame;
        BWAPI::Bullet bullet;
        Unit unit;

        explicit NoGoAreaExpiry(int framesToExpiry) : frame(currentFrame + framesToExpiry), bullet(nullptr), unit(nullptr) {}

        explicit NoGoAreaExpiry(BWAPI::Bullet bullet) : frame(-1), bullet(bullet), unit(nullptr) {}

        explicit NoGoAreaExpiry(Unit unit) : frame(-1), bullet(nullptr), unit(std::move(unit)) {}

        [[nodiscard]] bool isExpired() const
        {
            if (frame != -1) return currentFrame >= frame;
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
#if CVIS_HEATMAPS
        // Dump to CherryVis
        std::vector<long> noGoAreaTilesCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
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
            for (int x = -radius; x <= radius; x++)
            {
                for (int y = -radius; y <= radius; y++)
                {
                    if (Geo::ApproximateDistance(0, x, 0, y) <= radius)
                    {
                        positions.insert(BWAPI::TilePosition(BWAPI::Position(x, y)));
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

    std::set<BWAPI::TilePosition> generateDirectedBox(BWAPI::Position origin, BWAPI::Position target, int width)
    {
        int length = origin.getApproxDistance(target);
        auto scaledVector = Geo::ScaleVector(target - origin, 16);
        auto scaledInverse = BWAPI::Position(scaledVector.y, scaledVector.x);

        std::set<BWAPI::TilePosition> result;
        auto insertIfValid = [&result](BWAPI::TilePosition tile)
        {
            if (tile.isValid()) result.insert(tile);
        };

        auto currentLengthwise = origin;
        for (int i = 0; i <= length / 16; i++)
        {
            insertIfValid(BWAPI::TilePosition(currentLengthwise));

            auto currentWidthwise = currentLengthwise;
            for (int j = 0; j < width / 16; j++)
            {
                currentWidthwise += scaledInverse;
                insertIfValid(BWAPI::TilePosition(currentWidthwise));
            }

            currentWidthwise = currentLengthwise;
            for (int j = 0; j < width / 16; j++)
            {
                currentWidthwise -= scaledInverse;
                insertIfValid(BWAPI::TilePosition(currentWidthwise));
            }

            currentLengthwise += scaledVector;
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
        for (auto it = noGoAreasWithExpiration.begin(); it != noGoAreasWithExpiration.end();)
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

        for (auto nukeDot : BWAPI::Broodwar->getNukeDots())
        {
            if (nukeDot.isValid())
            {
                addCircle(nukeDot, 256, 1);
            }
        }
    }

    void writeInstrumentation()
    {
        if (noGoAreaTilesUpdated)
        {
            dumpNoGoAreaTiles();
            noGoAreaTilesUpdated = false;
        }
    }

    void addBox(BWAPI::TilePosition topLeft, BWAPI::TilePosition size)
    {
        auto bottomRight = topLeft + size;

        for (int y = topLeft.y; y < bottomRight.y; y++)
        {
            if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

            for (int x = topLeft.x; x < bottomRight.x; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()]++;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    void removeBox(BWAPI::TilePosition topLeft, BWAPI::TilePosition size)
    {
        auto bottomRight = topLeft + size;

        for (int y = topLeft.y; y < bottomRight.y; y++)
        {
            if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

            for (int x = topLeft.x; x < bottomRight.x; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()]--;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    void addCircle(BWAPI::Position origin, int radius, int expireFrames)
    {
        auto tiles = generateCircle(origin, radius);
        add(tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), expireFrames);
    }

    void addCircle(BWAPI::Position origin, int radius, const Unit &unit)
    {
        auto tiles = generateCircle(origin, radius);
        add(tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), unit);
    }

    void addCircle(BWAPI::Position origin, int radius, BWAPI::Bullet bullet)
    {
        auto tiles = generateCircle(origin, radius);
        add(tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), bullet);
    }

    void addDirectedBox(BWAPI::Position origin, BWAPI::Position target, int width, int expireFrames)
    {
        auto tiles = generateDirectedBox(origin, target, width);
        add(tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), expireFrames);
    }

    void addDirectedBox(BWAPI::Position origin, BWAPI::Position target, int width, BWAPI::Bullet bullet)
    {
        auto tiles = generateDirectedBox(origin, target, width);
        add(tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), bullet);
    }

    bool isNoGo(BWAPI::TilePosition pos)
    {
        return noGoAreaTiles[pos.x + pos.y * BWAPI::Broodwar->mapWidth()] > 0;
    }

    bool isNoGo(int x, int y)
    {
        return noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()] > 0;
    }

    void onUnitCreate(const Unit &unit)
    {
        if (unit->type == BWAPI::UnitTypes::Terran_Nuclear_Missile)
        {
            CherryVis::log() << "Detected nuke targeting " << BWAPI::WalkPosition(unit->lastPosition);
            addCircle(unit->lastPosition, 288 + 32, unit);
        }
    }

    void onBulletCreate(BWAPI::Bullet bullet)
    {
        if (bullet->getType() == BWAPI::BulletTypes::Psionic_Storm)
        {
            CherryVis::log() << "Detected storm targeting " << BWAPI::WalkPosition(bullet->getPosition());
            addCircle(bullet->getPosition(), 80 + 32, bullet);
        }

        if (bullet->getType() == BWAPI::BulletTypes::Subterranean_Spines)
        {
            auto direction = Geo::ScaleVector(BWAPI::Position((int) bullet->getVelocityX(), (int) bullet->getVelocityY()), 192);
            if (direction != BWAPI::Positions::Invalid)
            {
                addDirectedBox(bullet->getPosition(), bullet->getPosition() + direction, 50, bullet);
            }
        }

        if (bullet->getType() == BWAPI::BulletTypes::EMP_Missile)
        {
            CherryVis::log() << "Detected EMP targeting " << BWAPI::WalkPosition(bullet->getTargetPosition());
            addCircle(bullet->getTargetPosition(), 80 + 32, bullet);
        }
    }
}
