#pragma once

#include "MapSpecificOverride.h"

class Python : public MapSpecificOverride
{
public:
    virtual ForgeGatewayWall getWall(BWAPI::TilePosition startTile) override;
};
