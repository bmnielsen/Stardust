#include "BuildingPlacer.h"

#include <BWEB.h>

namespace { auto & bwebMap = BWEB::Map::Instance(); }

namespace BuildingPlacer
{
    void initialize()
    {
        bwebMap.onStart();

        // TODO: Create wall
        // TODO: Create proxy blocks

        bwebMap.findBlocks();
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        bwebMap.onUnitDestroy(unit);
    }

    void onUnitMorph(BWAPI::Unit unit)
    {
        bwebMap.onUnitMorph(unit);
    }

    void onUnitCreate(BWAPI::Unit unit)
    {
        bwebMap.onUnitDiscover(unit);
    }

    void onUnitDiscover(BWAPI::Unit unit)
    {
        bwebMap.onUnitDiscover(unit);
    }
}