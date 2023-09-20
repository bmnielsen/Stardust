#pragma once

#include "MapSpecificOverride.h"

class Colosseum : public MapSpecificOverride
{
public:
    void modifyBases(std::vector<Base *> &bases) override;

    bool hasBackdoorNatural() override { return true; }

    void modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation) override;

    std::optional<ForgeGatewayWall> getWall(BWAPI::TilePosition startTile) override;
};
