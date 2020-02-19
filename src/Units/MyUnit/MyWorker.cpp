#include "MyWorker.h"

#include "PathFinding.h"

MyWorker::MyWorker(BWAPI::Unit unit)
        : MyUnitImpl(unit)
        , mineralWalkingPatch(nullptr)
        , mineralWalkingTargetArea(nullptr)
        , mineralWalkingStartPosition(BWAPI::Positions::Invalid)
{
}

void MyWorker::resetMoveData()
{
    MyUnitImpl::resetMoveData();
    mineralWalkingPatch = nullptr;
    mineralWalkingTargetArea = nullptr;
    mineralWalkingStartPosition = BWAPI::Positions::Invalid;
}


bool MyWorker::mineralWalk(const Choke *choke)
{
    if (!choke && !mineralWalkingPatch) return false;

    // If we're passed a choke, we should consider initializing a new mineral walk
    if (choke)
    {
        if (!choke->requiresMineralWalk)
        {
            return false;
        }

        const BWEM::ChokePoint *nextWaypoint = choke->choke;

        // Determine which of the two areas accessible by the choke we are moving towards.
        // We do this by looking at the waypoint after the next one and seeing which area they share,
        // or by looking at the area of the target position if there are no more waypoints.
        if (chokePath.size() == 1)
        {
            mineralWalkingTargetArea = BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(targetPosition));
        }
        else
        {
            mineralWalkingTargetArea = nextWaypoint->GetAreas().second;

            if (nextWaypoint->GetAreas().first == chokePath[1]->GetAreas().first ||
                nextWaypoint->GetAreas().first == chokePath[1]->GetAreas().second)
            {
                mineralWalkingTargetArea = nextWaypoint->GetAreas().first;
            }
        }

        // Pull the mineral patch and start location to use for mineral walking
        // This may be null - on some maps we need to use a visible mineral patch somewhere else on the map
        // This is handled in mineralWalk()
        mineralWalkingPatch =
                mineralWalkingTargetArea == nextWaypoint->GetAreas().first
                ? choke->firstAreaMineralPatch
                : choke->secondAreaMineralPatch;
        mineralWalkingStartPosition =
                mineralWalkingTargetArea == nextWaypoint->GetAreas().first
                ? choke->firstAreaStartPosition
                : choke->secondAreaStartPosition;

        lastMoveFrame = 0;
    }

    // If we're close to the patch, or if the patch is null and we've moved beyond the choke,
    // we're done mineral walking
    if ((mineralWalkingPatch && bwapiUnit->getDistance(mineralWalkingPatch) < 32) ||
        (!mineralWalkingPatch &&
         BWEM::Map::Instance().GetArea(getTilePosition()) == mineralWalkingTargetArea &&
         getDistance(BWAPI::Position(chokePath[0]->Center())) > 100))
    {
        mineralWalkingPatch = nullptr;
        mineralWalkingTargetArea = nullptr;
        mineralWalkingStartPosition = BWAPI::Positions::Invalid;

        // Remove the choke we just mineral walk and reset the grid
        if (!chokePath.empty()) chokePath.pop_front();
        resetGrid();

        // Move to the next waypoint
        moveToNextWaypoint();
        return true;
    }

    // Re-issue orders every second
    if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame < 24) return true;

    // If the patch is null, click on any visible patch on the correct side of the choke
    if (!mineralWalkingPatch)
    {
        for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (!staticNeutral->getType().isMineralField()) continue;
            if (!staticNeutral->exists() || !staticNeutral->isVisible()) continue;

            // The path to this mineral field should cross the choke we're mineral walking
            for (auto pathChoke : PathFinding::GetChokePointPath(
                    lastPosition,
                    staticNeutral->getInitialPosition(),
                    type,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea))
            {
                if (pathChoke == *chokePath.begin())
                {
                    // The path went through the choke, let's use this field
                    rightClick(staticNeutral);
                    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
                    return true;
                }
            }
        }

        // We couldn't find any suitable visible mineral patch, warn and abort
        Log::Debug() << "Error: Unable to find mineral patch to use for mineral walking";

        resetMoveData();
        return true;
    }

    // If the patch is visible, click on it
    if (mineralWalkingPatch->exists() && mineralWalkingPatch->isVisible())
    {
        rightClick(mineralWalkingPatch);
        lastMoveFrame = BWAPI::Broodwar->getFrameCount();
        return true;
    }

    // If we have a start location defined, click on it
    if (mineralWalkingStartPosition.isValid())
    {
        move(mineralWalkingStartPosition);
        return true;
    }

    Log::Debug() << "Error: Unable to find tile to mineral walk from";
    resetMoveData();

    // This code is still used for Plasma
    // TODO before CIG next year: migrate Plasma to set appropriate start positions for each choke
    /*
    // Find the closest and furthest walkable position within sight range of the patch
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;
    BWAPI::Position worstPos = BWAPI::Positions::Invalid;
    int bestDist = INT_MAX;
    int worstDist = 0;
    int desiredDist = unit->getType().sightRange();
    int desiredDistTiles = desiredDist / 32;
    for (int x = -desiredDistTiles; x <= desiredDistTiles; x++)
        for (int y = -desiredDistTiles; y <= desiredDistTiles; y++)
        {
            BWAPI::TilePosition tile = mineralWalkingPatch->getInitialTilePosition() + BWAPI::TilePosition(x, y);
            if (!tile.isValid()) continue;
            if (!MapTools::Instance().isWalkable(tile)) continue;

            BWAPI::Position tileCenter = BWAPI::Position(tile) + BWAPI::Position(16, 16);
            if (tileCenter.getApproxDistance(mineralWalkingPatch->getInitialPosition()) > desiredDist)
                continue;

            // Check that there is a path to the tile
            int pathLength = PathFinding::GetGroundDistance(unit->getPosition(), tileCenter, unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (pathLength == -1) continue;

            // The path should not cross the choke we're mineral walking
            for (auto choke : PathFinding::GetChokePointPath(unit->getPosition(), tileCenter, unit->getType(), PathFinding::PathFindingOptions::UseNearestBWEMArea))
                if (choke == *waypoints.begin())
                    goto cnt;

            if (pathLength < bestDist)
            {
                bestDist = pathLength;
                bestPos = tileCenter;
            }

            if (pathLength > worstDist)
            {
                worstDist = pathLength;
                worstPos = tileCenter;
            }

        cnt:;
        }


    // If we couldn't find a tile, abort
    if (!bestPos.isValid())
    {
        Log().Get() << "Error: Unable to find tile to mineral walk from";

        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        currentlyMovingTowards = BWAPI::Positions::Invalid;
        mineralWalkingPatch = nullptr;
        mineralWalkingTargetArea = nullptr;
        mineralWalkingStartPosition = BWAPI::Positions::Invalid;
        return;
    }

    // If we are already very close to the best position, it isn't working: we should have vision of the mineral patch
    // So use the worst one instead
    BWAPI::Position tile = bestPos;
    if (unit->getDistance(tile) < 16) tile = worstPos;

    // Move towards the tile
    Micro::Move(unit, tile);
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
    */
    return true;
}
