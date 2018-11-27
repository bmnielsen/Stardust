#include "Building.h"

#include "PathFinding.h"

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

bool Building::constructionStarted() const
{
    return unit && unit->exists();
}

int Building::expectedFramesUntilCompletion() const
{
    if (unit && unit->exists()) return unit->getRemainingBuildTime();
    
    // This can be inaccurate if this isn't the next building in the builder's queue
    // TODO: Verify this doesn't cause problems
    return PathFinding::ExpectedTravelTime(builder->getPosition(), getPosition(), builder->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea)
        + type.buildTime();
}
