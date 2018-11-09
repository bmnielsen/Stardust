#include "Base.h"

Base::Base(BWAPI::TilePosition _tile, const BWEM::Base * _bwemBase)
    : owner(Owner::None)
    , resourceDepot(nullptr)
    , tile(_tile)
    , bwemBase(_bwemBase)
    , ownedSince(-1)
    , lastScouted(-1)
    , spiderMined(false)
    , requiresMineralWalkFromEnemyStartLocations(false)
{
}

std::vector<BWAPI::Unit> Base::mineralPatches() const
{
    std::vector<BWAPI::Unit> result;
    for (auto mineral : bwemBase->Minerals())
    {
        result.push_back(mineral->Unit());
    }

    return result;
}
