#include "Building.h"

Building::Building(BWAPI::UnitType type, BWAPI::TilePosition tile, BWAPI::Unit builder)
    : type(type)
    , tile(tile)
    , unit(nullptr)
    , builder(builder)
{
}

BWAPI::Position Building::getPosition() const
{
    return BWAPI::Position(tile) + BWAPI::Position(type.tileWidth() * 16, type.tileHeight() * 16);
}
