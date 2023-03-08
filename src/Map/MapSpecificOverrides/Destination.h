#pragma once

#include "MapSpecificOverride.h"

class Destination : public MapSpecificOverride
{
public:
    virtual ForgeGatewayWall getWall(BWAPI::TilePosition startTile) override;
};
