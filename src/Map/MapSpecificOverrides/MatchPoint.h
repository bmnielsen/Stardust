#pragma once

#include "MapSpecificOverride.h"

class MatchPoint : public MapSpecificOverride
{
public:
    std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
