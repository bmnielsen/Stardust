#pragma once

#include "MapSpecificOverride.h"

class EmpireOfTheSun : public MapSpecificOverride
{
public:
    std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
