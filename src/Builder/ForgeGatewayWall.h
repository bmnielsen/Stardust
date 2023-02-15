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

    [[nodiscard]] bool containsBuildingAt(BWAPI::TilePosition tile) const
    {
        if (pylon == tile || forge == tile || gateway == tile) return true;
        for (auto const &cannon : cannons)
        {
            if (cannon == tile) return true;
        }

        return false;
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
        }
        return out;
    };
};
