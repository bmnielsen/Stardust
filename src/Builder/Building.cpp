#include "Building.h"

#include "PathFinding.h"
#include "UnitUtil.h"
#include "NoGoAreas.h"

Building::Building(BWAPI::UnitType type, BWAPI::TilePosition tilePos, OwnedWorker builder, int desiredStartFrame)
        : type(type)
        , tilePos(tilePos)
        , gameUnit(nullptr)
        , builder(std::move(builder))
        , desiredStartFrame(desiredStartFrame)
        , buildCommandSuccessFrames(0)
        , buildCommandFailureFrames(0)
        , noGoAreaAdded(false)
        , startFrame(-1)
{
}

void Building::constructionStarted(OwnedUnit startedUnit)
{
    gameUnit = std::move(startedUnit);
    startFrame = frameNow;
}

BWAPI::Position Building::getPosition() const
{
    return BWAPI::Position(tilePos) + BWAPI::Position(type.tileWidth() * 16, type.tileHeight() * 16);
}

bool Building::isConstructionStarted() const
{
    return gameUnit && gameUnit->exists();
}

int Building::expectedFramesUntilStarted() const
{
    if (gameUnit && gameUnit->exists()) return 0;

    /* This can be inaccurate if this isn't the next building in the builder's queue */
    /* TODO: Verify this doesn't cause problems */
    int workerFrames =
            Pathfinder::ExpectedTravelTime(builder->prevPos,
                                            getPosition(),
                                            builder->type,
                                            Pathfinder::PathFindingOptions::UseNearestBWEMArea);

    return std::max(workerFrames, desiredStartFrame - frameNow);
}

int Building::expectedFramesUntilCompletion() const
{
    if (startFrame != -1)
    {
        return UnitHelper::BuildTime(type) - (frameNow - startFrame);
    }

    return UnitHelper::BuildTime(type) + expectedFramesUntilStarted();
}

bool Building::builderReady() const
{
    if (isConstructionStarted() || !builder || !builder->exists()) return false;

    return builder->getDistance(getPosition()) < 32;
}

void Building::addNoGoAreaWhenNeeded()
{
    if (noGoAreaAdded) return;

    /* Add the no-go area when the desired start frame is within 5 seconds of now */
    if (expectedFramesUntilStarted() > 120) return;

    NoGoAreas::addBox(NoGoAreas::Type::GroundNavigational, tilePos - BWAPI::TilePosition(1, 1), type.tileSize() + BWAPI::TilePosition(2, 2));
    noGoAreaAdded = true;
}

void Building::removeNoGoArea()
{
    if (!noGoAreaAdded) return;

    NoGoAreas::removeBox(NoGoAreas::Type::GroundNavigational, tilePos - BWAPI::TilePosition(1, 1), type.tileSize() + BWAPI::TilePosition(2, 2));
    noGoAreaAdded = false;
}

