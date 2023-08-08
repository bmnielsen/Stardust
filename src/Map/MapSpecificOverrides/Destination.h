#pragma once

#include "MapSpecificOverride.h"

class Destination : public MapSpecificOverride
{
public:
    virtual std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
