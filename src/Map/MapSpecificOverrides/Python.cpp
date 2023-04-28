#include "Python.h"

ForgeGatewayWall Python::getWall(BWAPI::TilePosition startTile)
{
    // Currently disabling walls that aren't tight and narrow
    return {};

    if (startTile.x != 83 || startTile.y != 6) return {};

    ForgeGatewayWall result;

    result.pylon = BWAPI::TilePosition(54, 9);
    result.forge = BWAPI::TilePosition(56, 13);
    result.gateway = BWAPI::TilePosition(58, 10);
    result.cannons.emplace_back(56, 11);
    result.cannons.emplace_back(56, 9);
    result.cannons.emplace_back(52, 8);
    result.cannons.emplace_back(54, 11);
    result.cannons.emplace_back(52, 10);
    result.cannons.emplace_back(51, 5);

    result.naturalCannons.emplace_back(51, 5);
    result.naturalCannons.emplace_back(53, 5);

    result.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(50, 8)) + BWAPI::Position(16, 16);
    result.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(52, 8)) + BWAPI::Position(16, 16);

    return result;
}
