#include "EmpireOfTheSun.h"

std::optional<ForgeGatewayWall> EmpireOfTheSun::getWall(BWAPI::TilePosition startTile)
{
    // Empire of the sun has no tight wall options, and we currently don't have the skills to plug holes in non-tight walls
    return ForgeGatewayWall{};
}
