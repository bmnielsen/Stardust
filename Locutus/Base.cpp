#include "Base.h"

Base::Base(BWAPI::TilePosition _tile, const BWEM::Base * _bwemBase)
    : tile(_tile)
    , bwemBase(_bwemBase)
    , owner(BWAPI::Broodwar->neutral())
    , ownedSince(-1)
    , lastScouted(-1)
    , spiderMined(false)
    , requiresMineralWalkFromEnemyStartLocations(false)
{
}
