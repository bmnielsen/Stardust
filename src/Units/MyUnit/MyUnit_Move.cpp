#include "MyUnit.h"

#include "PathFinding.h"
#include "Map.h"

namespace
{
    auto &bwemMap = BWEM::Map::Instance();

    const NavigationGrid::GridNode *nextNode(const NavigationGrid::GridNode *currentNode)
    {
        if (!currentNode || !currentNode->nextNode || !currentNode->nextNode->nextNode || !currentNode->nextNode->nextNode->nextNode) return nullptr;
        return currentNode->nextNode->nextNode->nextNode;
    }
}

void MyUnit::issueMoveOrders()
{
    if (issuedOrderThisFrame) return;
    if (!targetPosition.isValid()) return;

    if (unstickMoveUnit()) return;

    updateMoveWaypoints();
}

bool MyUnit::moveTo(BWAPI::Position position)
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
    targetPosition = position;

    // Get the BWEM path
    auto &path = PathFinding::GetChokePointPath(
            unit->getPosition(),
            position,
            unit->getType(),
            PathFinding::PathFindingOptions::UseNearestBWEMArea);
    for (const BWEM::ChokePoint *chokepoint : path)
    {
        chokePath.push_back(chokepoint);
    }

    // Attempt to get an appropriate navigation grid
    resetGrid();

    // Validate if we can perform the move and issue orders for trivial moves
    if ((!gridNode || !nextNode(gridNode)) && chokePath.empty())
    {
        // If the unit is in the same area as the target position, move to it directly
        auto unitArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(unit->getPosition()));
        auto targetArea = bwemMap.GetArea(BWAPI::WalkPosition(targetPosition));
        if (!targetArea || targetArea == unitArea)
        {
            move(position);
            return true;
        }

        // Otherwise we can't make the move - we don't have a choke path or a grid
        targetPosition = BWAPI::Positions::Invalid;
        return false;
    }

    // Start the move
#if DEBUG_UNIT_ORDERS
    std::ostringstream log;
    log << "Order: Starting move to " << BWAPI::WalkPosition(position);
    if (grid) log << "; grid target " << grid->goal;
    if (gridNode) log << "; initial grid node " << *gridNode;
    CherryVis::log(unit) << log.str();
#endif

    targetPosition = position;
    moveToNextWaypoint();
    return true;
}

void MyUnit::resetMoveData()
{
    targetPosition = BWAPI::Positions::Invalid;
    currentlyMovingTowards = BWAPI::Positions::Invalid;
    grid = nullptr;
    chokePath.clear();
    gridNode = nullptr;
}

void MyUnit::moveToNextWaypoint()
{
    // Current grid node is close to the target
    if (gridNode && gridNode->cost <= 30)
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Order: Reached end of grid " << grid->goal;
#endif

        grid = nullptr;
        gridNode = nullptr;

        // Short-circuit if the unit is in the target area
        // This means we are close to the destination and just need to do a simple move from here
        // State will be reset after latency frames to avoid resetting the order later
        auto unitArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(unit->getPosition()));
        auto targetArea = bwemMap.GetArea(BWAPI::WalkPosition(targetPosition));
        if (!targetArea || targetArea == unitArea)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit) << "Order: Moving to target position " << BWAPI::WalkPosition(targetPosition);
#endif

            move(targetPosition);
            lastMoveFrame = BWAPI::Broodwar->getFrameCount();
            return;
        }

        // The unit is not in the target area, so we were using the grid to navigate to a choke
        // Pop the choke path until it fits the current position and fall through to choke-based navigation
        updateChokePath(unitArea);
    }

    // Navigate using the current grid node, if we have it
    if (gridNode)
    {
        if (auto next = nextNode(gridNode))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit) << "Order: Moving towards next grid node " << *next;
#endif

            currentlyMovingTowards = BWAPI::Position(
                    (next->x << 5U) + 16,
                    (next->y << 5U) + 16);
            move(currentlyMovingTowards);
            lastMoveFrame = BWAPI::Broodwar->getFrameCount();

            return;
        }

        // We have a valid grid, but we're not in a connected grid node
        // Ensure the choke path is updated and fall through
        // We will pick up the grid path when we can
        updateChokePath(bwemMap.GetNearestArea(BWAPI::WalkPosition(unit->getPosition())));
    }

    // If there is no choke path, and we couldn't navigate using the grid, just move to the position
    if (chokePath.empty())
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(unit) << "Order: No path; moving to target position " << BWAPI::WalkPosition(targetPosition);
#endif

        move(targetPosition);
        lastMoveFrame = BWAPI::Broodwar->getFrameCount();
        return;
    }

    const BWEM::ChokePoint *nextWaypoint = *chokePath.begin();
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
        if (chokePath.size() > 1) next = BWAPI::Position(chokePath[1]->Center());

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

#if DEBUG_UNIT_ORDERS
    CherryVis::log(unit) << "Order: Moving towards next choke @ " << BWAPI::WalkPosition(currentlyMovingTowards);
#endif

    move(currentlyMovingTowards);
    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
}

void MyUnit::updateMoveWaypoints()
{
    // Reset after latency frames when we're done navigating
    if (!gridNode && chokePath.empty())
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

    // We have a grid we can use for navigation
    if (grid)
    {
        auto inSameNode = unit->getTilePosition().x == gridNode->x && unit->getTilePosition().y == gridNode->y;
        auto currentNodeValid = nextNode(gridNode) != nullptr;

        // If we are still in the same node, and the node is valid, then we just wait until the unit gets to a new node
        if (inSameNode && currentNodeValid)
        {
            return;
        }

        if (!inSameNode)
        {
            gridNode = &(*grid)[unit->getTilePosition()];

#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit) << "Order: Path node set to " << *gridNode;
#endif

            // If we were navigating using the previous node, or can navigate using the new node, update the waypoint
            if (currentNodeValid || nextNode(gridNode))
            {
                moveToNextWaypoint();
                return;
            }
        }

        // In all other cases fall through - we do not have a valid grid node so we are navigating using choke points
    }

    // Wait until the unit is close enough to the current target
    if (unit->getDistance(currentlyMovingTowards) > 100) return;

    // If the current target is a narrow ramp, wait until we can see the high elevation tile
    // We want to make sure we go up the ramp far enough to see anything potentially blocking the ramp
    auto choke = Map::choke(*chokePath.begin());
    if (choke->width < 96 && choke->isRamp && !BWAPI::Broodwar->isVisible(choke->highElevationTile))
        return;

    // Move to the next waypoint
    chokePath.pop_front();
    moveToNextWaypoint();
}

void MyUnit::resetGrid()
{
    // If there is a choke in the BWEM path requiring mineral walking, try to get a navigation grid to that choke
    Choke *mineralWalkingChoke = nullptr;
    if (unit->getType().isWorker() && Map::mapSpecificOverride()->hasMineralWalking())
    {
        for (const BWEM::ChokePoint *chokepoint : chokePath)
        {
            auto choke = Map::choke(chokepoint);
            if (choke->requiresMineralWalk)
            {
                mineralWalkingChoke = choke;
                break;
            }
        }
    }

    if (mineralWalkingChoke)
    {
        grid = PathFinding::getNavigationGrid(BWAPI::TilePosition(mineralWalkingChoke->Center()));
    }
    else
    {
        // We have ruled out mineral walking, so try to get a grid to the target
        grid = PathFinding::getNavigationGrid(BWAPI::TilePosition(targetPosition));

        // If that failed, try to get a grid to the furthest choke we can
        for (auto it = chokePath.rbegin(); it != chokePath.rend(); it++)
        {
            grid = PathFinding::getNavigationGrid(BWAPI::TilePosition((*it)->Center()));
            if (grid) break;
        }
    }

    // If we have a grid, get the first grid node
    if (grid) gridNode = &(*grid)[unit->getPosition()];
}

void MyUnit::updateChokePath(const BWEM::Area *unitArea)
{
    while (!chokePath.empty())
    {
        auto nextChoke = *chokePath.begin();

        // If the choke does not link the area we are currently in, pop it and continue
        if (nextChoke->GetAreas().first != unitArea && nextChoke->GetAreas().second != unitArea)
        {
            chokePath.pop_front();
            continue;
        }

        // If the choke is the only one left, break now
        if (chokePath.size() == 1) return;

        // If the next choke also includes the area we are currently in, then it should be next
        auto secondChoke = chokePath[1];
        if (secondChoke->GetAreas().first == unitArea || secondChoke->GetAreas().second == unitArea)
        {
            chokePath.pop_front();
        }

        return;
    }
}

bool MyUnit::unstickMoveUnit()
{
    // Consider the unit to not be stuck if the last command was not a valid move command
    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move
        || !currentCommand.getTargetPosition().isValid())
    {
        return false;
    }

    // Consider the unit to not be stuck if it is close to its move target (it may be decelerating or have already arrived)
    if (unit->getDistance(currentCommand.getTargetPosition()) < 32)
    {
        return false;
    }

    // Consider the unit to not be stuck if it is moving and the order is not Guard or PlayerGuard
    if (unit->isMoving() && (abs(unit->getVelocityX()) > 0.001 || abs(unit->getVelocityY()) > 0.001)
        && unit->getOrder() != BWAPI::Orders::Guard && unit->getOrder() != BWAPI::Orders::PlayerGuard)
    {
        return false;
    }

    // Otherwise force resend the last move command unless we recently did so
    if ((BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame()) >= (BWAPI::Broodwar->getLatencyFrames() + 3))
    {
        move(currentCommand.getTargetPosition(), true);
    }

    return true;
}
