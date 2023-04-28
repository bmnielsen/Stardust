#include "NeoMoonGlaive.h"

ForgeGatewayWall NeoMoonGlaive::getWall(BWAPI::TilePosition startTile)
{
    if (startTile.x != 67 || startTile.y != 6) return {};

    ForgeGatewayWall result;

    result.pylon = BWAPI::TilePosition(43, 16);
    result.forge = BWAPI::TilePosition(36, 18);
    result.gateway = BWAPI::TilePosition(42, 20);
    result.cannons.emplace_back(37, 16);
    result.cannons.emplace_back(46, 19);
    result.cannons.emplace_back(46, 17);
    result.cannons.emplace_back(39, 17);
    result.cannons.emplace_back(43, 13);
    result.cannons.emplace_back(46, 13);

    result.naturalCannons.emplace_back(43, 13);
    result.naturalCannons.emplace_back(39, 12);

    result.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(38, 19)) + BWAPI::Position(20, 20);
    result.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(42, 20)) + BWAPI::Position(16, 16);

    return result;
}
