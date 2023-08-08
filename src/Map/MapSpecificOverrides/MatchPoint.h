#pragma once

#include "MapSpecificOverride.h"

class MatchPoint : public MapSpecificOverride
{
public:
    virtual std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
