#include "MatchPoint.h"

std::optional<ForgeGatewayWall> MatchPoint::getWall(BWAPI::TilePosition startTile)
{
    // BWEM doesn't generate a choke at the natural for Match Point, so we define the wall manually
    if (startTile.x != 100 || startTile.y != 14) return {};

    ForgeGatewayWall result;

    result.pylon = BWAPI::TilePosition(104, 48);
    result.forge = BWAPI::TilePosition(100, 51);
    result.gateway = BWAPI::TilePosition(106, 51);
    result.cannons.emplace_back(100, 49);
    result.cannons.emplace_back(102, 49);
    result.cannons.emplace_back(109, 49);
    result.cannons.emplace_back(107, 49);
    result.cannons.emplace_back(98, 50);
    result.cannons.emplace_back(108, 47);

    result.naturalCannons.emplace_back(100, 44);
    result.naturalCannons.emplace_back(98, 46);

    result.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(102, 52)) + BWAPI::Position(16, 16);
    result.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(106, 52)) + BWAPI::Position(16, 16);

    return result;
}
