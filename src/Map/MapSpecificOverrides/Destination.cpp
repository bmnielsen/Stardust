#include "Destination.h"

std::optional<ForgeGatewayWall> Destination::getWall(BWAPI::TilePosition startTile)
{
    // We don't have the skills to defend a non-standard wall like what would work on Destination
    return ForgeGatewayWall{};
}
