#pragma once

#include "MapSpecificOverride.h"

class Outsider : public MapSpecificOverride
{
public:
    void addIslandAreas(std::set<const BWEM::Area *> &islandAreas) override;

    void modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas) override;
};
