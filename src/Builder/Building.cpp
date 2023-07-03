#include "Building.h"

#include "PathFinding.h"
#include "UnitUtil.h"
#include "NoGoAreas.h"

Building::Building(BWAPI::UnitType type, BWAPI::TilePosition tile, MyUnit builder, int desiredStartFrame)
        : type(type)
        , tile(tile)
        , unit(nullptr)
        , builder(std::move(builder))
        , desiredStartFrame(desiredStartFrame)
        , buildCommandSuccessFrames(0)
        , buildCommandFailureFrames(0)
        , noGoAreaAdded(false)
        , startFrame(-1)
{
}

void Building::constructionStarted(MyUnit startedUnit)
{
    unit = std::move(startedUnit);
    startFrame = currentFrame;
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
            PathFinding::ExpectedTravelTime(builder->lastPosition,
                                            getPosition(),
                                            builder->type,
                                            PathFinding::PathFindingOptions::UseNearestBWEMArea);

    return std::max(workerFrames, desiredStartFrame - currentFrame);
}

int Building::expectedFramesUntilCompletion() const
{
    if (startFrame != -1)
    {
        return UnitUtil::BuildTime(type) - (currentFrame - startFrame);
    }

    return UnitUtil::BuildTime(type) + expectedFramesUntilStarted();
}

bool Building::builderReady() const
{
    if (isConstructionStarted() || !builder || !builder->exists()) return false;

    return builder->getDistance(getPosition()) < 32;
}

void Building::addNoGoAreaWhenNeeded()
{
    if (noGoAreaAdded) return;

    // Add the no-go area when the desired start frame is within 5 seconds of now
    if (desiredStartFrame > (currentFrame + 120)) return;

    NoGoAreas::addBox(tile - BWAPI::TilePosition(1, 1), type.tileSize() + BWAPI::TilePosition(2, 2));
    noGoAreaAdded = true;
}

void Building::removeNoGoArea()
{
    if (!noGoAreaAdded) return;

    NoGoAreas::removeBox(tile - BWAPI::TilePosition(1, 1), type.tileSize() + BWAPI::TilePosition(2, 2));
    noGoAreaAdded = false;
}

