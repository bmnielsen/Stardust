#include "MyUnit.h"

#include "PathFinding.h"
#include "Map.h"

namespace
{
    auto &bwemMap = BWEM::Map::Instance();
}

bool MyUnit::moveTo(BWAPI::Position position, bool avoidNarrowChokes)
{
    // Fliers just move to the target
    if (unit->isFlying())
    {
        move(position);
        return true;
    }

    // If the unit is already moving to this position, don't do anything
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (position == targetPosition ||
        (currentCommand.getType() == BWAPI::UnitCommandTypes::Move && currentCommand.getTargetPosition() == position))
    {
        return true;
    }

    // Clear any existing waypoints
    resetMoveData();

    // If the unit is already in the same area, or the target doesn't have an area, just move it directly
    auto targetArea = bwemMap.GetArea(BWAPI::WalkPosition(position));
    if (!targetArea || targetArea == bwemMap.GetArea(BWAPI::WalkPosition(unit->getPosition())))
    {
        move(position);
        return true;
    }

    // Get the BWEM path
    // TODO: Consider narrow chokes
    auto &path = PathFinding::GetChokePointPath(
            unit->getPosition(),
            position,
            unit->getType(),
            PathFinding::PathFindingOptions::UseNearestBWEMArea);
    if (path.empty()) return false;

    for (const BWEM::ChokePoint *chokepoint : path)
    {
        waypoints.push_back(chokepoint);
    }

    // Start moving
    targetPosition = position;
    moveToNextWaypoint();
    return true;
}

void MyUnit::resetMoveData()
{
    waypoints.clear();
    targetPosition = BWAPI::Positions::Invalid;
    currentlyMovingTowards = BWAPI::Positions::Invalid;
}

void MyUnit::updateMoveWaypoints()
{
    if (waypoints.empty())
    {
        if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame > BWAPI::Broodwar->getLatencyFrames())
        {
            targetPosition = BWAPI::Positions::Invalid;
            currentlyMovingTowards = BWAPI::Positions::Invalid;
        }
        return;
    }

    if (mineralWalk(nullptr))
    {
        return;
    }

    // If the unit command is no longer to move towards our current target, clear the waypoints
    // This means we have ordered the unit to do something else in the meantime
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move || currentCommand.getTargetPosition() != currentlyMovingTowards)
    {
        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        currentlyMovingTowards = BWAPI::Positions::Invalid;
        return;
    }

    // Wait until the unit is close enough to the current target
    if (unit->getDistance(currentlyMovingTowards) > 100) return;

    // If the current target is a narrow ramp, wait until we can see the high elevation tile
    // We want to make sure we go up the ramp far enough to see anything potentially blocking the ramp
    auto choke = Map::choke(*waypoints.begin());
    if (choke->width < 96 && choke->isRamp && !BWAPI::Broodwar->isVisible(choke->highElevationTile))
        return;

    // Move to the next waypoint
    waypoints.pop_front();
    moveToNextWaypoint();
}

void MyUnit::moveToNextWaypoint()
{
    // If there are no more waypoints, move to the target position
    // State will be reset after latency frames to avoid resetting the order later
    if (waypoints.empty())
    {
        move(targetPosition);
        lastMoveFrame = BWAPI::Broodwar->getFrameCount();
        return;
    }

    const BWEM::ChokePoint *nextWaypoint = *waypoints.begin();
    auto choke = Map::choke(nextWaypoint);

    // Check if the next waypoint needs to be mineral walked
    if (mineralWalk(choke))
    {
        return;
    }

    // Determine the position on the choke to move towards

    // If it is a narrow ramp, move towards the point with highest elevation
    // We do this to make sure we explore the higher elevation part of the ramp before bugging out if it is blocked
    if (choke->width < 96 && choke->isRamp)
    {
        currentlyMovingTowards = BWAPI::Position(choke->highElevationTile) + BWAPI::Position(16, 16);
    }
    else
    {
        // Get the next position after this waypoint
        BWAPI::Position next = targetPosition;
        if (waypoints.size() > 1) next = BWAPI::Position(waypoints[1]->Center());

        // Move to the part of the choke closest to the next position
        int bestDist = INT_MAX;
        for (auto walkPosition : nextWaypoint->Geometry())
        {
            BWAPI::Position pos(walkPosition);
            int dist = pos.getApproxDistance(next);
            if (dist < bestDist)
            {
                bestDist = dist;
                currentlyMovingTowards = pos;
            }
        }
    }

    move(currentlyMovingTowards);
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
}

int MyUnit::distanceToMoveTarget() const
{
    // If we're currently doing a move with waypoints, sum up the total ground distance
    if (targetPosition.isValid())
    {
        BWAPI::Position current = unit->getPosition();
        int dist = 0;
        for (auto waypoint : waypoints)
        {
            dist += current.getApproxDistance(BWAPI::Position(waypoint->Center()));
            current = BWAPI::Position(waypoint->Center());
        }
        return dist + current.getApproxDistance(targetPosition);
    }

    // Otherwise, if the unit has a move order, compute the simple air distance
    // Usually this means the unit is already in the same area as the target (or is a flier)
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Move)
        return unit->getDistance(currentCommand.getTargetPosition());

    // The unit is not moving
    return INT_MAX;
}

