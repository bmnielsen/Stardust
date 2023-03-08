#include "Destination.h"

ForgeGatewayWall Destination::getWall(BWAPI::TilePosition startTile)
{
    ForgeGatewayWall result;

    if (startTile.x == 31 && startTile.y == 7)
    {
        result.pylon = BWAPI::TilePosition(60, 18);
        result.forge = BWAPI::TilePosition(59, 22);
        result.gateway = BWAPI::TilePosition(56, 19);
        result.cannons.emplace_back(60, 20);
        result.cannons.emplace_back(58, 17);
        result.cannons.emplace_back(56, 17);
        result.cannons.emplace_back(63, 17);
        result.cannons.emplace_back(67, 19);

        result.naturalCannons.emplace_back(63, 17);
        result.naturalCannons.emplace_back(67, 19);

        result.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(61, 22)) + BWAPI::Position(16, 16);
        result.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(63, 21)) + BWAPI::Position(16, 16);
        result.gapCenter = BWAPI::Position(BWAPI::TilePosition(62, 22)) + BWAPI::Position(16, 0);
        result.gapSize = 1;
    }
    else if (startTile.x == 64 && startTile.y == 118)
    {
        result.pylon = BWAPI::TilePosition(34, 110);
        result.forge = BWAPI::TilePosition(38, 110);
        result.gateway = BWAPI::TilePosition(35, 107);
        result.cannons.emplace_back(36, 110);
        result.cannons.emplace_back(31, 110);
        result.cannons.emplace_back(35, 113);
        result.cannons.emplace_back(27, 109);

        result.naturalCannons.emplace_back(31, 110);
        result.naturalCannons.emplace_back(27, 109);

        result.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(32, 107)) + BWAPI::Position(16, 16);
        result.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(35, 107)) + BWAPI::Position(16, 16);
        result.gapCenter = BWAPI::Position(BWAPI::TilePosition(34, 107)) + BWAPI::Position(0, 16);
        result.gapSize = 2;
    }
    else
    {
        Log::Get() << "ERROR: Destination map-specific override used on a map that does not resemble Destination";
    }

    return result;
}
