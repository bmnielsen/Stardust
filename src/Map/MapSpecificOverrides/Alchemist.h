#pragma once

#include "MapSpecificOverride.h"

class Alchemist : public MapSpecificOverride
{
public:
    void enemyStartingMainDetermined() override;

    bool hasBackdoorNatural() override { return true; }

    void modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas) override;
};
