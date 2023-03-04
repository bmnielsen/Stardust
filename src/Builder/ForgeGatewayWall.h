#pragma once

#include "Common.h"

// Stores information about a Forge/Gateway wall.
class ForgeGatewayWall
{
public:
    BWAPI::TilePosition forge;
    BWAPI::TilePosition gateway;
    BWAPI::TilePosition pylon;
    std::vector<BWAPI::TilePosition> cannons;
    std::vector<BWAPI::TilePosition> naturalCannons;

    int gapSize;
    BWAPI::Position gapCenter;
    BWAPI::Position gapEnd1;
    BWAPI::Position gapEnd2;

    std::set<BWAPI::TilePosition> tilesInsideWall;
    std::set<BWAPI::TilePosition> tilesOutsideWall;
    std::set<BWAPI::TilePosition> tilesOutsideButCloseToWall;

    ForgeGatewayWall()
            : forge(BWAPI::TilePositions::Invalid)
            , gateway(BWAPI::TilePositions::Invalid)
            , pylon(BWAPI::TilePositions::Invalid)
            , gapSize(INT_MAX)
            , gapCenter(BWAPI::Positions::Invalid)
            , gapEnd1(BWAPI::Positions::Invalid)
            , gapEnd2(BWAPI::Positions::Invalid)
    {};

    ForgeGatewayWall(BWAPI::TilePosition forge,
                     BWAPI::TilePosition gateway,
                     BWAPI::TilePosition pylon,
                     int gapSize,
                     BWAPI::Position gapCenter,
                     BWAPI::Position gapEnd1,
                     BWAPI::Position gapEnd2)
            : forge(forge)
            , gateway(gateway)
            , pylon(pylon)
            , gapSize(gapSize)
            , gapCenter(gapCenter)
            , gapEnd1(gapEnd1)
            , gapEnd2(gapEnd2)
    {};

    [[nodiscard]] bool isValid() const
    {
        return pylon.isValid() && forge.isValid() && gateway.isValid();
    }

    friend std::ostream &operator<<(std::ostream &out, const ForgeGatewayWall &wall)
    {
        if (!wall.isValid())
        {
            out << "invalid";
        }
        else
        {
            out << "pylon@" << wall.pylon << ";forge@" << wall.forge << ";gate@" << wall.gateway << "cannons@";
            for (auto const &tile : wall.cannons)
            {
                out << tile;
            }
            out << ";natCannons@";
            for (auto const &tile : wall.naturalCannons)
            {
                out << tile;
            }
        }
        return out;
    };

    void addToHeatmap(std::vector<long> &wallHeatmap)
    {
        // Values:
        // - Gateway: 2
        // - Forge: 4
        // - Pylon: 6
        // - Cannon: 8
        // - Natural Cannon: 10

        auto addLocation = [&wallHeatmap](BWAPI::TilePosition tile, int width, int height, int value)
        {
            auto bottomRight = tile + BWAPI::TilePosition(width, height);

            for (int y = tile.y; y < bottomRight.y; y++)
            {
                if (y > BWAPI::Broodwar->mapHeight() - 1)
                {
                    Log::Get() << "ERROR: WALL LOCATION OUT OF BOUNDS @ " << tile;
                    continue;
                }

                for (int x = tile.x; x < bottomRight.x; x++)
                {
                    if (x > BWAPI::Broodwar->mapWidth() - 1)
                    {
                        Log::Get() << "ERROR: WALL LOCATION OUT OF BOUNDS @ " << tile;
                        continue;
                    }

                    wallHeatmap[x + y * BWAPI::Broodwar->mapWidth()] = value;
                }
            }
        };

        addLocation(gateway, 4, 3, 2);
        addLocation(forge, 3, 2, 4);
        addLocation(pylon, 2, 2, 6);
        for (const auto &cannon : cannons)
        {
            addLocation(cannon, 2, 2, 8);
        }
        for (const auto &cannon : naturalCannons)
        {
            addLocation(cannon, 2, 2, 10);
        }
    }
};
