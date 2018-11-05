#pragma once

#include "Common.h"

#include <bwem.h>

#include "MapSpecificOverride.h"
#include "Base.h"
#include "Choke.h"

namespace Map
{
    void initialize();
    void onUnitDestroy(BWAPI::Unit unit);

    MapSpecificOverride * mapSpecificOverride();
    
    const std::vector<Base> & allBases();
    Base * baseNear(BWAPI::TilePosition tile);
    
    Choke * choke(const BWEM::ChokePoint * bwemChoke);

    int minChokeWidth();
}