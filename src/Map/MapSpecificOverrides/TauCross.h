#pragma once

#include "MapSpecificOverride.h"

class TauCross : public MapSpecificOverride
{
public:
    virtual ForgeGatewayWall getWall(BWAPI::TilePosition startTile) override;
};
