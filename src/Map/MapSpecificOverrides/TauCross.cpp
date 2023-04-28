#include "TauCross.h"

ForgeGatewayWall TauCross::getWall(BWAPI::TilePosition startTile)
{
    // Currently disabling walls that don't give proper defense locations
    return {};

    if (startTile.x != 93 || startTile.y != 118) return {};

    ForgeGatewayWall result;

    result.pylon = BWAPI::TilePosition(52, 110);
    result.forge = BWAPI::TilePosition(50, 108);
    result.gateway = BWAPI::TilePosition(52, 105);
    result.cannons.emplace_back(53, 108);
    result.cannons.emplace_back(55, 108);
    result.cannons.emplace_back(56, 106);
    result.cannons.emplace_back(54, 110);
    result.cannons.emplace_back(56, 110);
    result.cannons.emplace_back(57, 108);

    result.naturalCannons.emplace_back(55, 113);
    result.naturalCannons.emplace_back(49, 114);

    result.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(50, 109)) + BWAPI::Position(16, 0);
    result.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(48, 109)) + BWAPI::Position(16, 0);

    return result;
}
