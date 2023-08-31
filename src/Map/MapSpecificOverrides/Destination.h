#pragma once

#include "MapSpecificOverride.h"

class Destination : public MapSpecificOverride
{
public:
    std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
