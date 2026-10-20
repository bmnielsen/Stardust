#include "NoGoAreas.h"

#include "Geo.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_HEATMAPS true
#endif

namespace
{
    struct NoGoAreaExpiry
    {
        NoGoAreas::Type type;
        int tick;
        BWAPI::Bullet bullet;
        Unit gameUnit;

        explicit NoGoAreaExpiry(NoGoAreas::Type type, int framesToExpiry)
                : type(type)
                , tick(frameNow + framesToExpiry)
                , bullet(nullptr)
                , gameUnit(nullptr)
        {}

        explicit NoGoAreaExpiry(NoGoAreas::Type type, BWAPI::Bullet bullet) : type(type), tick(-1), bullet(bullet), gameUnit(nullptr) {}

        explicit NoGoAreaExpiry(NoGoAreas::Type type, Unit gameUnit) : type(type), tick(-1), bullet(nullptr), gameUnit(std::move(gameUnit)) {}

        [[nodiscard]] bool isExpired() const
        {
            if (tick != -1) return frameNow >= tick;
            if (bullet) return !bullet->exists();
            return !gameUnit->exists();
        }
    };

    std::array<std::vector<short>, NO_GO_AREAS_TYPE_COUNT> noGoAreaTiles;
    bool noGoAreaTilesUpdated;
    std::vector<std::pair<std::set<BWAPI::TilePosition>, NoGoAreaExpiry>> noGoAreasWithExpiration;
    std::map<unsigned short, std::set<BWAPI::TilePosition>> tilesInRadiusCache;

    /* Writes the tile no go areas to CherryVis */
    void dumpNoGoAreaTiles()
    {
#if CVIS_HEATMAPS
        /* Dump to CherryVis */
        std::vector<long> noGoAreaTilesCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                noGoAreaTilesCVis[x + y * BWAPI::Broodwar->mapWidth()] =
                        noGoAreaTiles[0][x + y * BWAPI::Broodwar->mapWidth()] +
                        noGoAreaTiles[1][x + y * BWAPI::Broodwar->mapWidth()];
            }
        }

        Telemetry::addHeatmap("NoGoAreas", noGoAreaTilesCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
    }

    void add(NoGoAreas::Type type, std::set<BWAPI::TilePosition> &tiles)
    {
        for (const auto &tilePos : tiles)
        {
            noGoAreaTiles[(int)type][tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()]++;
        }

        noGoAreaTilesUpdated = true;
    }

    void remove(NoGoAreas::Type type, std::set<BWAPI::TilePosition> &tiles)
    {
        for (const auto &tilePos : tiles)
        {
            noGoAreaTiles[(int)type][tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()]--;
        }

        noGoAreaTilesUpdated = true;
    }

    std::set<BWAPI::TilePosition> generateCircle(BWAPI::Position origin, unsigned short radius)
    {
        std::set<BWAPI::TilePosition> &positions = tilesInRadiusCache[radius];

        if (positions.empty())
        {
            for (int x = -radius; x <= radius; x++)
            {
                for (int y = -radius; y <= radius; y++)
                {
                    if (Geom::ApproximateDistance(0, x, 0, y) <= radius)
                    {
                        positions.insert(BWAPI::TilePosition(BWAPI::Position(x, y)));
                    }
                }
            }
        }

        auto tileOrigin = BWAPI::TilePosition(origin);

        std::set<BWAPI::TilePosition> outcome;
        for (const auto &tilePos : positions)
        {
            auto here = tilePos + tileOrigin;
            if (here.isValid()) outcome.insert(here);
        }

        return outcome;
    }

    std::set<BWAPI::TilePosition> generateDirectedBox(BWAPI::Position origin, BWAPI::Position target, unsigned int width)
    {
        int length = origin.getApproxDistance(target);
        auto scaledVector = Geom::ScaleVector(target - origin, 16);
        auto scaledInverse = BWAPI::Position(scaledVector.y, scaledVector.x);

        std::set<BWAPI::TilePosition> outcome;
        auto insertIfValid = [&outcome](BWAPI::TilePosition tilePos)
        {
            if (tilePos.isValid()) outcome.insert(tilePos);
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

        return outcome;
    }
}

namespace NoGoAreas
{
    void initialize()
    {
        for (int i = 0; i < NO_GO_AREAS_TYPE_COUNT; i++)
        {
            noGoAreaTiles[i].clear();
            noGoAreaTiles[i].resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        }
        noGoAreaTilesUpdated = true; // So we get an initial null state
        noGoAreasWithExpiration.clear();

        update();
    }

    void update()
    {
        for (auto iterRef = noGoAreasWithExpiration.begin(); iterRef != noGoAreasWithExpiration.end();)
        {
            if (iterRef->second.isExpired())
            {
                remove(iterRef->second.type, iterRef->first);
                iterRef = noGoAreasWithExpiration.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }

        for (auto nukeDot : BWAPI::Broodwar->getNukeDots())
        {
            if (nukeDot.isValid())
            {
                addCircle(Type::Danger, nukeDot, 256, 1);
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

    void addBox(Type type, BWAPI::TilePosition cornerTL, BWAPI::TilePosition size)
    {
        auto bottomRight = cornerTL + size;

        for (int y = cornerTL.y; y < bottomRight.y; y++)
        {
            if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

            for (int x = cornerTL.x; x < bottomRight.x; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[(int)type][x + y * BWAPI::Broodwar->mapWidth()]++;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    void removeBox(Type type, BWAPI::TilePosition cornerTL, BWAPI::TilePosition size)
    {
        auto bottomRight = cornerTL + size;

        for (int y = cornerTL.y; y < bottomRight.y; y++)
        {
            if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

            for (int x = cornerTL.x; x < bottomRight.x; x++)
            {
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[(int)type][x + y * BWAPI::Broodwar->mapWidth()]--;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    void addCircle(Type type, BWAPI::Position origin, unsigned short radius, int expireFrames)
    {
        auto tiles = generateCircle(origin, radius);
        add(type, tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), NoGoAreaExpiry{type, expireFrames});
    }

    void addCircle(Type type, BWAPI::Position origin, int radius, const Unit &gameUnit)
    {
        auto tiles = generateCircle(origin, radius);
        add(type, tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), NoGoAreaExpiry{type, gameUnit});
    }

    void addCircle(Type type, BWAPI::Position origin, int radius, BWAPI::Bullet bullet)
    {
        auto tiles = generateCircle(origin, radius);
        add(type, tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), NoGoAreaExpiry{type, bullet});
    }

    void addDirectedBox(Type type, BWAPI::Position origin, BWAPI::Position target, int width, int expireFrames)
    {
        auto tiles = generateDirectedBox(origin, target, width);
        add(type, tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), NoGoAreaExpiry{type, expireFrames});
    }

    void addDirectedBox(Type type, BWAPI::Position origin, BWAPI::Position target, int width, BWAPI::Bullet bullet)
    {
        auto tiles = generateDirectedBox(origin, target, width);
        add(type, tiles);
        noGoAreasWithExpiration.emplace_back(std::move(tiles), NoGoAreaExpiry{type, bullet});
    }

    bool isNoGo(BWAPI::TilePosition pt, TypeFilter typeFilter)
    {
        return isNoGo(pt.x, pt.y, typeFilter);
    }

    bool isNoGo(int x, int y, TypeFilter typeFilter)
    {
        if (typeFilter == TypeFilter::Any || typeFilter == TypeFilter::OnlyDanger)
        {
            if (noGoAreaTiles[(int)Type::Danger][x + y * BWAPI::Broodwar->mapWidth()] > 0)
            {
                return true;
            }
        }
        if (typeFilter == TypeFilter::Any || typeFilter == TypeFilter::OnlyGroundNavigational)
        {
            if (noGoAreaTiles[(int)Type::GroundNavigational][x + y * BWAPI::Broodwar->mapWidth()] > 0)
            {
                return true;
            }
        }
        return false;
    }

    void onUnitCreate(const Unit &gameUnit)
    {
        if (gameUnit->type == BWAPI::UnitTypes::Terran_Nuclear_Missile)
        {
            Telemetry::log() << "Detected nuke targeting " << BWAPI::WalkPosition(gameUnit->prevPos);
            addCircle(Type::Danger, gameUnit->prevPos, 288 + 32, gameUnit);
        }
    }

    void onBulletCreate(BWAPI::Bullet bullet)
    {
        if (bullet->getType() == BWAPI::BulletTypes::Psionic_Storm)
        {
            Telemetry::log() << "Detected storm targeting " << BWAPI::WalkPosition(bullet->getPosition());
            addCircle(Type::Danger, bullet->getPosition(), 80 + 32, bullet);
        }

        if (bullet->getType() == BWAPI::BulletTypes::Subterranean_Spines)
        {
            auto direction = Geom::ScaleVector(BWAPI::Position((int) bullet->getVelocityX(), (int) bullet->getVelocityY()), 192);
            if (direction != BWAPI::Positions::Invalid)
            {
                addDirectedBox(Type::Danger, bullet->getPosition(), bullet->getPosition() + direction, 50, bullet);
            }
        }

        if (bullet->getType() == BWAPI::BulletTypes::EMP_Missile)
        {
            Telemetry::log() << "Detected EMP targeting " << BWAPI::WalkPosition(bullet->getTargetPosition());
            addCircle(Type::Danger, bullet->getTargetPosition(), 80 + 32, bullet);
        }
    }
}
