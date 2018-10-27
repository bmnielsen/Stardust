#include "Map.h"

#include <bwem.h>

namespace { auto & bwemMap = BWEM::Map::Instance(); }

namespace Map
{
    void initialize()
    {
        bwemMap.Initialize(BWAPI::BroodwarPtr);
        bwemMap.EnableAutomaticPathAnalysis();
        bool bwem = bwemMap.FindBasesForStartingLocations();
        Log::Debug() << "Initialized BWEM: " << bwem;

        // TODO: Analyze chokes
        // TODO: Find bases
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        if (unit->getType().isMineralField())
            bwemMap.OnMineralDestroyed(unit);
        else if (unit->getType().isSpecialBuilding())
            bwemMap.OnStaticBuildingDestroyed(unit);
    }
}
