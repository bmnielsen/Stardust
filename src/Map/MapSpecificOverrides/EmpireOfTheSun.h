#pragma once

#include "MapSpecificOverride.h"

class EmpireOfTheSun : public MapSpecificOverride
{
public:
    virtual std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
