#include "Map.h"

#include "Fortress.h"
#include "Plasma.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }

namespace Map
{
    namespace
    {
        MapSpecificOverride * _mapSpecificOverride;
        std::vector<Base> bases;
        std::map<const BWEM::ChokePoint *, Choke> chokes;
        int _minChokeWidth;
    }

    void initialize()
    {
        // Initialize BWEM
        bwemMap.Initialize(BWAPI::BroodwarPtr);
        bwemMap.EnableAutomaticPathAnalysis();
        bool bwem = bwemMap.FindBasesForStartingLocations();
        Log::Debug() << "Initialized BWEM: " << bwem;

        // Select map-specific overrides
        if (BWAPI::Broodwar->mapHash() == "83320e505f35c65324e93510ce2eafbaa71c9aa1")
            _mapSpecificOverride = new Fortress();
        else if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67")
            _mapSpecificOverride = new Plasma();
        else
            _mapSpecificOverride = new MapSpecificOverride();

        // Initialize bases
        for (const auto & area : bwemMap.Areas())
            for (const auto & base : area.Bases())
                bases.emplace_back(base.Location(), &base);
        Log::Debug() << "Found " << bases.size() << " bases";

        // Analyze chokepoints
        for (const auto & area : bwemMap.Areas())
            for (const BWEM::ChokePoint * choke : area.ChokePoints())
                if (chokes.find(choke) == chokes.end())
                    chokes.emplace(choke, Choke{ choke });
        _mapSpecificOverride->initializeChokes(chokes);

        // Compute the minimum choke width
        _minChokeWidth = INT_MAX;
        for (const auto & pair : chokes)
            if (pair.second.width < _minChokeWidth)
                _minChokeWidth = pair.second.width;
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        if (unit->getType().isMineralField())
            bwemMap.OnMineralDestroyed(unit);
        else if (unit->getType().isSpecialBuilding())
            bwemMap.OnStaticBuildingDestroyed(unit);
    }

    MapSpecificOverride * mapSpecificOverride()
    {
        return _mapSpecificOverride;
    }

    const std::vector<Base>& allBases()
    {
        return bases;
    }

    Base * baseNear(BWAPI::TilePosition tile)
    {
        for (auto & base : bases)
        {
            int dist = base.getTilePosition().getApproxDistance(tile);
            if (dist < 10) return &base;
        }

        return nullptr;
    }

    Choke * choke(const BWEM::ChokePoint * bwemChoke)
    {
        if (!bwemChoke) return nullptr;
        auto it = chokes.find(bwemChoke);
        return it == chokes.end()
            ? nullptr
            : &it->second;
    }

    int minChokeWidth()
    {
        return _minChokeWidth;
    }
}
