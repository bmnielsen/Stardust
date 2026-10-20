#include "MatchPoint.h"

std::optional<ForgeGatewayWall> MatchPoint::getWall(BWAPI::TilePosition startTile)
{
    /* BWEM doesn't generate a choke at the natural for Match Point, so we define the wall manually */
    if (startTile.x != 100 || startTile.y != 14) return {};

    ForgeGatewayWall outcome;

    outcome.pylon = BWAPI::TilePosition(104, 48);
    outcome.forge = BWAPI::TilePosition(100, 51);
    outcome.gateway = BWAPI::TilePosition(106, 51);
    outcome.cannons.emplace_back(100, 49);
    outcome.cannons.emplace_back(102, 49);
    outcome.cannons.emplace_back(109, 49);
    outcome.cannons.emplace_back(107, 49);
    outcome.cannons.emplace_back(98, 50);
    outcome.cannons.emplace_back(108, 47);

    outcome.naturalCannons.emplace_back(100, 44);
    outcome.naturalCannons.emplace_back(98, 46);

    outcome.gapEnd1 = BWAPI::Position(BWAPI::TilePosition(102, 52)) + BWAPI::Position(16, 16);
    outcome.gapEnd2 = BWAPI::Position(BWAPI::TilePosition(106, 52)) + BWAPI::Position(16, 16);

    return outcome;
}
