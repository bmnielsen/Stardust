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

    void onUnitCreate(BWAPI::Unit unit)
    {
        // Resource depots near bases imply base ownership
        if (unit->getType().isResourceDepot())
        {
            auto * base = baseNear(unit->getTilePosition());
            if (base)
            {
                // Determine the owner of the created building
                Base::Owner unitOwner = Base::Owner::None;
                if (unit->getPlayer() == BWAPI::Broodwar->self())
                    unitOwner = Base::Owner::Me;
                else if (unit->getPlayer()->isEnemy(BWAPI::Broodwar->self()))
                    unitOwner = Base::Owner::Enemy;
                else if (unit->getPlayer()->isAlly(BWAPI::Broodwar->self()))
                    unitOwner = Base::Owner::Ally;
                else
                    return; // Neutral unit?

                // If the base was previously unowned, set the owner
                if (base->owner == Base::Owner::None)
                {
                    base->owner = unitOwner;
                    base->ownedSince = BWAPI::Broodwar->getFrameCount();
                }

                // Set the resource depot when appropriate
                if (base->owner == unitOwner)
                {
                    // If we already have a registered resource depot for this base, replace it if the new one is
                    // closer to where we expect it should be
                    if (base->resourceDepot && base->resourceDepot->exists())
                    {
                        int existingDist = base->resourceDepot->getPosition().getDistance(base->getPosition());
                        int newDist = unit->getPosition().getDistance(base->getPosition());
                        if (newDist < existingDist) base->resourceDepot = unit;
                    }
                    else
                        base->resourceDepot = unit;
                }
            }
        }
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        if (unit->getType().isMineralField())
            bwemMap.OnMineralDestroyed(unit);
        else if (unit->getType().isSpecialBuilding())
            bwemMap.OnStaticBuildingDestroyed(unit);

        // If the lost unit was the resource depot of a base, update the base ownership
        if (unit->getType().isResourceDepot())
        {
            for (auto & base : bases)
            {
                if (base.resourceDepot == unit)
                {
                    base.owner = Base::Owner::None;
                    base.resourceDepot = nullptr;
                    base.ownedSince = -1;
                }
            }
        }
    }

    MapSpecificOverride * mapSpecificOverride()
    {
        return _mapSpecificOverride;
    }

    std::vector<Base>& allBases()
    {
        return bases;
    }

    Base * baseNear(BWAPI::TilePosition tile)
    {
        int closestDist = INT_MAX;
        Base * result = nullptr;

        for (auto & base : bases)
        {
            int dist = base.getTilePosition().getApproxDistance(tile);
            if (dist < 10 && dist < closestDist)
            {
                closestDist = dist;
                result = &base;
            }
        }

        return result;
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
