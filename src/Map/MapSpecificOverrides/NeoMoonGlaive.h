#pragma once

#include "MapSpecificOverride.h"

class NeoMoonGlaive : public MapSpecificOverride
{
public:
    virtual ForgeGatewayWall getWall(BWAPI::TilePosition startTile) override;
};
