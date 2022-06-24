#pragma once

#include "MapSpecificOverride.h"

class Outsider : public MapSpecificOverride
{
public:
    void addIslandAreas(std::set<const BWEM::Area *> &islandAreas) override;
};
