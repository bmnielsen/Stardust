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

    // If we've passed a choke, we should consider initializing a new mineral walk
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
    if (currentFrame - lastMoveFrame < 24) return true;

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
                    lastMoveFrame = currentFrame;
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
        lastMoveFrame = currentFrame;
        return true;
    }

    // If we have a start location defined, click on it
    if (mineralWalkingStartPosition.isValid())
    {
        move(mineralWalkingStartPosition);
        return true;
    }

    Log::Get() << "ERROR: Unable to find tile to mineral walk from";
    resetMoveData();

    return true;
}

void MyWorker::attackUnit(const Unit &target,
                          std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                          bool clusterAttacking,
                          int enemyAoeRadius)
{
    MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
}
