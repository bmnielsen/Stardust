#pragma once

#include "Common.h"

/* Stores information about a Forge/Gateway wall. */
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

    std::set<BWAPI::Position> probeBlockingPositions;

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
            for (auto const &tilePos : wall.cannons)
            {
                out << tilePos;
            }
            out << ";natCannons@";
            for (auto const &tilePos : wall.naturalCannons)
            {
                out << tilePos;
            }
            out << ";gapSize=" << wall.gapSize;
        }
        return out;
    };

    void addToHeatmap(std::vector<long> &wallHeatmap)
    {
        /* Values: */
        /* - Gateway: 10 */
        /* - Forge: 10 */
        /* - Pylon: 40 */
        /* - Cannon: 20 */
        /* - Natural Cannon: 20 */
        /* - tilesInsideWall: 2 */
        /* - tilesOutsideWall: -2 */
        /* - tilesOutsideButCloseToWall: -1 */

        auto addLocation = [&wallHeatmap](BWAPI::TilePosition tilePos, int width, int height, int value)
        {
            auto bottomRight = tilePos + BWAPI::TilePosition(width, height);

            for (int y = tilePos.y; y < bottomRight.y; y++)
            {
                if (y > BWAPI::Broodwar->mapHeight() - 1)
                {
                    AppLog::Get() << "ERROR: WALL LOCATION OUT OF BOUNDS @ " << tilePos;
                    continue;
                }

                for (int x = tilePos.x; x < bottomRight.x; x++)
                {
                    if (x > BWAPI::Broodwar->mapWidth() - 1)
                    {
                        AppLog::Get() << "ERROR: WALL LOCATION OUT OF BOUNDS @ " << tilePos;
                        continue;
                    }

                    wallHeatmap[x + y * BWAPI::Broodwar->mapWidth()] = value;
                }
            }
        };

        addLocation(gateway, 4, 3, 10);
        addLocation(forge, 3, 2, 10);
        addLocation(pylon, 2, 2, 40);
        for (const auto &cannon : cannons)
        {
            addLocation(cannon, 2, 2, 20);
        }
        for (const auto &cannon : naturalCannons)
        {
            addLocation(cannon, 2, 2, 25);
        }

        for (const auto &tilePos : tilesInsideWall)
        {
            wallHeatmap[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] += 2;
        }
        for (const auto &tilePos : tilesOutsideWall)
        {
            wallHeatmap[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] -= 2;
        }
        for (const auto &tilePos : tilesOutsideButCloseToWall)
        {
            wallHeatmap[tilePos.x + tilePos.y * BWAPI::Broodwar->mapWidth()] -= 1;
        }
    }
};
