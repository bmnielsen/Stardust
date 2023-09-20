#pragma once

#include "MapSpecificOverride.h"

class CrossingField : public MapSpecificOverride
{
public:
    bool hasBackdoorNatural() override { return true; }

    void modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation) override;

    std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
