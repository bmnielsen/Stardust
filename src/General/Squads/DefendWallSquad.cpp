#include "DefendWallSquad.h"

#include "BuildingPlacement.h"

DefendWallSquad::DefendWallSquad()
        : Squad("Defend wall")
{
    wall = BuildingPlacement::getForgeGatewayWall();
    if (!wall.isValid())
    {
        Log::Get() << "ERROR: DefendWallSquad for invalid wall";
    }

    targetPosition = wall.gapCenter;
}

void DefendWallSquad::execute(UnitCluster &cluster)
{
}
