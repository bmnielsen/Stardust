#include "Building.h"

#include "PathFinding.h"
#include "UnitUtil.h"

Building::Building(BWAPI::UnitType type, BWAPI::TilePosition tile, BWAPI::Unit builder, int desiredStartFrame)
        : type(type)
        , tile(tile)
        , unit(nullptr)
        , builder(builder)
        , desiredStartFrame(desiredStartFrame)
        , startFrame(-1)
{
}

void Building::constructionStarted(BWAPI::Unit _unit)
{
    unit = _unit;
    startFrame = BWAPI::Broodwar->getFrameCount();
}

BWAPI::Position Building::getPosition() const
{
    return BWAPI::Position(tile) + BWAPI::Position(type.tileWidth() * 16, type.tileHeight() * 16);
}

bool Building::isConstructionStarted() const
{
    return unit && unit->exists();
}

int Building::expectedFramesUntilStarted() const
{
    if (unit && unit->exists()) return 0;

    // This can be inaccurate if this isn't the next building in the builder's queue
    // TODO: Verify this doesn't cause problems
    int workerFrames =
            PathFinding::ExpectedTravelTime(builder->getPosition(),
                                            getPosition(),
                                            builder->getType(),
                                            PathFinding::PathFindingOptions::UseNearestBWEMArea);

    return std::max(workerFrames, desiredStartFrame - BWAPI::Broodwar->getFrameCount());
}

int Building::expectedFramesUntilCompletion() const
{
    if (startFrame != -1)
    {
        return UnitUtil::BuildTime(type) - (BWAPI::Broodwar->getFrameCount() - startFrame);
    }

    return UnitUtil::BuildTime(type) + expectedFramesUntilStarted();
}

bool Building::builderReady() const
{
    if (isConstructionStarted() || !builder || !builder->exists()) return false;

    return builder->getDistance(getPosition()) < 32;
}
