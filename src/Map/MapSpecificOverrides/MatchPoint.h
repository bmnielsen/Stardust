#pragma once

#include "MapSpecificOverride.h"

class MatchPoint : public MapSpecificOverride
{
public:
    virtual ForgeGatewayWall getWall(BWAPI::TilePosition startTile) override;
};
