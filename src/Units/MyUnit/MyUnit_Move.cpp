#include "MyUnit.h"

#include "PathFinding.h"
#include "Map.h"

namespace
{
    auto &bwemMap = BWEM::Map::Instance();
}

void MyUnit::issueMoveOrders()
{
    if (issuedOrderThisFrame) return;

    updateMoveWaypoints();
    unstickMoveUnit();
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

    // Get an optimized path, if one is available
    // Otherwise we fall back to navigating with BWEM chokepoints
    optimizedPath = PathFinding::optimizedPath(unit->getPosition(), position);
    if (optimizedPath)
    {
        // If we are very close, just move directly
        if (optimizedPath->cost < 30)
        {
            move(position);
            return true;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Order: Starting grid-based move to " << BWAPI::WalkPosition(position);
        CherryVis::log(unit) << "Order: Path node set to (" << optimizedPath->x << "," << optimizedPath->y << ")";
#endif

        targetPosition = position;
        moveToNextWaypoint();
        return true;
    }

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
#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Starting waypoint-based move to " << BWAPI::WalkPosition(position);
#endif
    targetPosition = position;
    moveToNextWaypoint();
    return true;
}

void MyUnit::resetMoveData()
{
    waypoints.clear();
    targetPosition = BWAPI::Positions::Invalid;
    currentlyMovingTowards = BWAPI::Positions::Invalid;
    optimizedPath = nullptr;
}

void MyUnit::updateMoveWaypoints()
{
    // Reset after latency frames when we're done navigating
    if (!optimizedPath && waypoints.empty())
    {
        if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame > BWAPI::Broodwar->getLatencyFrames())
        {
            resetMoveData();
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
#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Order: Aborting move as command has changed";
#endif
        resetMoveData();
        return;
    }

    // Logic for when we are navigating along an optimized path
    if (optimizedPath)
    {
        // Wait until we are out of the current node
        if (unit->getTilePosition().x == optimizedPath->x && unit->getTilePosition().y == optimizedPath->y) return;

        // Advance the node
        optimizedPath = PathFinding::optimizedPath(unit->getPosition(), targetPosition);
#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Order: Path node set to (" << optimizedPath->x << "," << optimizedPath->y << ")";
#endif
        moveToNextWaypoint();
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
    // If we are close to the target or there are no more waypoints, move to the target position
    // State will be reset after latency frames to avoid resetting the order later
    if ((optimizedPath && optimizedPath->cost < 30) || (!optimizedPath && waypoints.empty()))
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Order: Finished waypoint navigation";
#endif
        move(targetPosition);
        lastMoveFrame = BWAPI::Broodwar->getFrameCount();
        return;
    }

    // If we are moving on an optimized path, we move towards the third node (approximately three tiles ahead)
    if (optimizedPath)
    {
        if (optimizedPath->nextNode && optimizedPath->nextNode->nextNode && optimizedPath->nextNode->nextNode->nextNode)
        {
            currentlyMovingTowards = BWAPI::Position(
                    (optimizedPath->nextNode->nextNode->nextNode->x << 5) + 16,
                    (optimizedPath->nextNode->nextNode->nextNode->y << 5) + 16);
            move(currentlyMovingTowards);
            lastMoveFrame = BWAPI::Broodwar->getFrameCount();

#if DEBUG_UNIT_ORDERS
            auto one = BWAPI::Position((optimizedPath->x << 5) + 16, (optimizedPath->y << 5) + 16);
            auto two = BWAPI::Position((optimizedPath->nextNode->x << 5) + 16, (optimizedPath->nextNode->y << 5) + 16);
            auto three = BWAPI::Position((optimizedPath->nextNode->nextNode->x << 5) + 16, (optimizedPath->nextNode->nextNode->y << 5) + 16);
            CherryVis::log(unit) << "Order: Moving through "
                                 << BWAPI::WalkPosition(one) << "[" << optimizedPath->cost << "],"
                                 << BWAPI::WalkPosition(two) << "[" << optimizedPath->nextNode->cost << "],"
                                 << BWAPI::WalkPosition(three) << "[" << optimizedPath->nextNode->nextNode->cost << "] to "
                                 << BWAPI::WalkPosition(currentlyMovingTowards) << "[" << optimizedPath->nextNode->nextNode->nextNode->cost << "]";
#endif
        }

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

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Moving towards next waypoint @ " << BWAPI::WalkPosition(currentlyMovingTowards);
#endif
}

void MyUnit::unstickMoveUnit()
{
    // Bail out if:
    // - The last command is not to move
    // - The last command was sent recently (the unit might just not have started moving yet)
    // - The unit is close to its move target (it may be decelerating or have already arrived)
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move
        || !currentCommand.getTargetPosition().isValid()
        || (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame()) < (BWAPI::Broodwar->getLatencyFrames() + 3)
        || unit->getDistance(currentCommand.getTargetPosition()) < 32)
    {
        return;
    }

    // We resend the move command if any of the following are true:
    // - The unit is not moving (either via BWAPI flag or measured by velocity)
    // - The order is changed to Guard or PlayerGuard
    if (!unit->isMoving()
        || unit->getOrder() == BWAPI::Orders::Guard
        || unit->getOrder() == BWAPI::Orders::PlayerGuard
        || (abs(unit->getVelocityX()) < 0.001 && abs(unit->getVelocityY()) < 0.001)
            )
    {
        move(currentCommand.getTargetPosition(), true);
    }
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

