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
