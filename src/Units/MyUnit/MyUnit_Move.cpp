#include <ranges>

#include "MyUnit.h"

#include "PathFinding.h"
#include "Map.h"

#include "DebugFlag_UnitOrders.h"

/*
 * In order to support multiple different triggers and forms of movement, it is done in two phases. First the move command is queued, then
 * the most recent move command is issued at the end of the frame. This allows high-priority movement like evading storms to take priority.
 *
 * There are two modes of movement: moving towards a goal position and moving towards an intermediate position. The former is used for simple
 * movements like sending a unit to a base or choke, and in this mode MyUnit manages following a grid or chokepoint path and handling mineral
 * walking. The latter is used when more fine-grained micro is needed, like when flocking, and MyUnit does minimal work, as e.g. computing
 * boids is done by the caller. The latter mode is set by specifying direct=true in the call to moveTo.
 *
 * In both modes, MyUnit handles detecting stuck units and unsticking them. This can also be triggered manually by callers before e.g. issuing
 * an attack command.
 */

namespace
{
    const NavGrid::GridNode *nextNode(const NavGrid::GridNode *currentNode)
    {
        if (!currentNode || !currentNode->nextNode) return nullptr;

        /* We prefer to go 5 tiles ahead, but accept an earlier tile if a later one is invalid */
        auto node = currentNode;
        for (int i = 0; i < 5; i++)
        {
            if (!node->nextNode) return node;
            node = node->nextNode;
        }

        return node;
    }
}

void MyUnitImpl::moveTo(BWAPI::Position position, bool direct)
{
    if (!position.isValid())
    {
        AppLog::Get() << "ERROR: MOVE TO INVALID POSITION: " << *this << " - " << position;
        Telemetry::log(ident) << "ERROR: MOVE TO INVALID POSITION: " << *this << " - " << position;
        return;
    }

    moveCommand = std::make_unique<MoveCommand>(position, direct);
}

void MyUnitImpl::issueMoveOrders()
{
    if (issuedOrderThisFrame) return;

    /* Clear the move command if this unit is loaded */
    if (rawUnit->isLoaded())
    {
        resetMoveData();
        return;
    }

    /* Process a new move command */
    if (moveCommand && moveCommand->targetPosition != targetPosition)
    {
        initiateMove();
        return;
    }

    /* Jump out now if the unit is not currently doing a move */
    if (!targetPosition.isValid()) return;

    /* Unstick the unit if it is stuck */
    /* There are two forms of sticking: general stuck units that can't do anything, and units that are probably stuck on terrain or a building */
    if (unstick()) return;
    if (unstickMoveUnit()) return;

    /* If the unit has just been unstuck, reissue the command to move towards our current target position */
    if (unstickUntil == frameNow)
    {
        move(currentlyMovingTowards);
        return;
    }

    /* Update the move */
    updateMoveWaypoints();
}

void MyUnitImpl::initiateMove()
{
    resetMoveData();
    targetPosition = moveCommand->targetPosition;

    /* No special pathing is required if the movement mode is direct or the unit is flying */
    if (moveCommand->direct || rawUnit->isFlying())
    {
        moveToNextWaypoint();
        return;
    }

    /* Get the choke path */
    auto path = Pathfinder::GetChokePointPath(
            rawUnit->getPosition(),
            targetPosition,
            rawUnit->getType(),
            Pathfinder::PathFindingOptions::UseNearestBWEMArea);
    for (const BWEM::ChokePoint *choke : path)
    {
        chokePath.push_back(choke);
    }

    /* Attempt to get an appropriate navigation grid */
    resetGrid();

#if DEBUG_UNIT_ORDERS
    std::ostringstream log;
    log << "Order: Initiating move to " << BWAPI::WalkPosition(targetPosition);
    if (grid) log << "; grid target " << grid->goal;
    if (gridNode) log << "; initial grid node " << *gridNode;
    Telemetry::log(ident) << log.str();
#endif

    moveToNextWaypoint();
}

void MyUnitImpl::resetMoveData()
{
    targetPosition = BWAPI::Positions::Invalid;
    currentlyMovingTowards = BWAPI::Positions::Invalid;
    grid = nullptr;
    chokePath.clear();
    gridNode = nullptr;
}

bool MyUnitImpl::hasReachedNextChoke() const
{
    if (chokePath.empty()) return false;

    /* Wait until the unit is close enough to the current target */
    if (rawUnit->getDistance(currentlyMovingTowards) > 100) return false;

    /* If the current target is a narrow ramp, wait until we can see the high elevation tile */
    /* We want to make sure we go up the ramp far enough to see anything potentially blocking the ramp */
    auto choke = Map::choke(*chokePath.begin());
    if (choke->isNarrowChoke && choke->isRamp && !BWAPI::Broodwar->isVisible(choke->highElevationTile))
        return false;

    return true;
}

void MyUnitImpl::moveToNextWaypoint()
{
    /* Current grid node is close to the target (approx. 3 tiles away) */
    if (gridNode && gridNode->cost <= 90)
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Order: Reached end of grid " << grid->goal;
#endif

        grid = nullptr;
        gridNode = nullptr;

        /* Short-circuit if the unit is in the target area */
        /* This means we are close to the destination and just need to do a simple move from here */
        /* State will be reset after latency frames to avoid resetting the order later */
        auto unitArea = BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(rawUnit->getPosition()));
        auto targetArea = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(targetPosition));
        if (!targetArea || targetArea == unitArea)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Order: Moving to target position " << BWAPI::WalkPosition(targetPosition);
#endif

            chokePath.clear();
            currentlyMovingTowards = targetPosition;
            move(currentlyMovingTowards);
            return;
        }

        /* The unit is not in the target area, so we were using the grid to navigate to a choke */
        /* Pop the choke path until it fits the current position and fall through to choke-based navigation */
        updateChokePath(unitArea);
    }

    /* Navigate using the current grid node, if we have it */
    if (gridNode)
    {
        if (auto next = nextNode(gridNode))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Order: Moving towards next grid node " << *next;
#endif

            currentlyMovingTowards = next->midpoint();
            move(currentlyMovingTowards);

            return;
        }

        /* We have a valid grid, but we're not in a connected grid node */
        /* Ensure the choke path is updated and fall through */
        /* We will pick up the grid path when we can */
        updateChokePath(BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(rawUnit->getPosition())));
    }

    /* If there is no choke path, and we couldn't navigate using the grid, just move to the position */
    if (chokePath.empty())
    {
        currentlyMovingTowards = targetPosition;
        move(currentlyMovingTowards);
        return;
    }

    const BWEM::ChokePoint *nextWaypoint = *chokePath.begin();
    auto choke = Map::choke(nextWaypoint);

    /* Check if the next waypoint needs to be mineral walked */
    if (mineralWalk(choke))
    {
        return;
    }

    /* Determine the position on the choke to move towards */

    /* Default to center */
    currentlyMovingTowards = choke->midpoint;

    /* If it is a narrow ramp, move towards the point with highest elevation */
    /* We do this to make sure we explore the higher elevation part of the ramp before bugging out if it is blocked */
    if (choke->isNarrowChoke && choke->isRamp)
    {
        currentlyMovingTowards = BWAPI::Position(choke->highElevationTile) + BWAPI::Position(16, 16);
    }
    else
    {
        /* Get the next position after this waypoint */
        BWAPI::Position next = targetPosition;
        if (chokePath.size() > 1) next = BWAPI::Position(chokePath[1]->Center()) + BWAPI::Position(4, 4);

        /* Move to the part of the choke closest to the next position */
        int bestDist = currentlyMovingTowards.getApproxDistance(next);
        for (auto walkPosition : nextWaypoint->Geometry())
        {
            auto pt = BWAPI::Position(walkPosition) + BWAPI::Position(4, 4);
            int span = pt.getApproxDistance(next);
            if (span < bestDist)
            {
                bestDist = span;
                currentlyMovingTowards = pt;
            }
        }
    }

    /* Check if we have arrived at the waypoint */
    /* This might happen if the move is being initiated while we are very close to the choke */
    if (hasReachedNextChoke())
    {
        chokePath.pop_front();
        moveToNextWaypoint();
        return;
    }

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Order: Moving towards next choke @ " << BWAPI::WalkPosition(currentlyMovingTowards);
#endif

    move(currentlyMovingTowards);
}

void MyUnitImpl::updateMoveWaypoints()
{
    if (mineralWalk(nullptr))
    {
        return;
    }

    BWAPI::UnitCommand currentCommand(rawUnit->getLastCommand());

    /* If this unit has just finished a mineral walk, resend the move command until it works */
    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit &&
        currentCommand.getTarget() && currentCommand.getTarget()->getType().isMineralField())
    {
        move(currentlyMovingTowards);
        return;
    }

    /* Check if the unit has been ordered to do something else and clear our move data */
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move ||
        currentCommand.getTargetPosition().getApproxDistance(currentlyMovingTowards) > 3)
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Order: Aborting move as command has changed";
#endif
        resetMoveData();
        return;
    }

    /* We have a grid we can use for navigation */
    if (grid)
    {
        grid->update();

        /* If we are no longer in the same node, update it and move to the next waypoint */
        if (tilePositionX != gridNode->x || tilePositionY != gridNode->y)
        {
            gridNode = &(*grid)[getTilePosition()];

#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Order: Path node set to " << *gridNode;
#endif
            moveToNextWaypoint();
            return;
        }

        /* Check if the waypoint we are currently using is still valid */
        auto next = nextNode(gridNode);
        if (next != nullptr)
        {
            /* React if a grid update has changed the desired waypoint */
            if (currentlyMovingTowards != next->midpoint())
            {
                moveToNextWaypoint();
            }
            return;
        }

        /* In all other cases fall through - we do not have a valid grid node so we are navigating using choke points */
    }

    if (hasReachedNextChoke())
    {
        chokePath.pop_front();
        moveToNextWaypoint();
        return;
    }

    /* For a direct move, resend the move command frequently to avoid units doing weird stuff because of collisions */
    if (lastMoveFrame < (frameNow - BWAPI::Broodwar->getLatencyFrames() - 12))
    {
        move(currentlyMovingTowards, true);
    }
}

void MyUnitImpl::resetGrid()
{
    /* If there is a choke in the BWEM path requiring mineral walking, try to get a navigation grid to that choke */
    Choke *mineralWalkingChoke = nullptr;
    if (rawUnit->getType().isWorker() && Map::mapSpecificOverride()->hasMineralWalking())
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
        grid = Pathfinder::getNavigationGrid(mineralWalkingChoke->midpoint);
    }
    else
    {
        /* We have ruled out mineral walking, so try to get a grid to the target */
        grid = Pathfinder::getNavigationGrid(targetPosition);

        /* If that failed, try to get a grid to the furthest choke we can */
        if (!grid)
        {
            for (auto &bwemChoke : std::ranges::reverse_view(chokePath))
            {
                grid = Pathfinder::getNavigationGrid(BWAPI::TilePosition(Map::choke(bwemChoke)->midpoint));

                /* Don't use a grid if the current node is invalid or if the goal is very close */
                if (grid)
                {
                    auto &node = (*grid)[getTilePosition()];
                    if (!node.nextNode || node.cost < 90)
                    {
                        grid = nullptr;
                    }
                }

                if (grid) break;
            }
        }
    }

    /* If we have a grid, get the first grid node */
    if (grid) gridNode = &(*grid)[rawUnit->getPosition()];
}

void MyUnitImpl::updateChokePath(const BWEM::Area *unitArea)
{
    while (!chokePath.empty())
    {
        auto nextChoke = *chokePath.begin();

        /* If the choke does not link the area we are currently in, pop it and continue */
        if (nextChoke->GetAreas().first != unitArea && nextChoke->GetAreas().second != unitArea)
        {
            chokePath.pop_front();
            continue;
        }

        /* If the choke is the only one left, break now */
        if (chokePath.size() == 1) break;

        /* If the next choke also includes the area we are currently in, then it should be next */
        auto secondChoke = chokePath[1];
        if (secondChoke->GetAreas().first == unitArea || secondChoke->GetAreas().second == unitArea)
        {
            chokePath.pop_front();
        }

        break;
    }

    if (chokePath.empty()) return;

    /* Finally pop the choke if the unit is close enough to it that we want to use the next choke */
    /* Exceptions: choke requires mineral walk or choke is a ramp and we can't see the high elevation tile yet */
    auto nextChoke = Map::choke(chokePath[0]);
    if (nextChoke->requiresMineralWalk || rawUnit->getDistance(nextChoke->midpoint) > 100 ||
        (nextChoke->isNarrowChoke && nextChoke->isRamp && !BWAPI::Broodwar->isVisible(nextChoke->highElevationTile)))
    {
        return;
    }
    chokePath.pop_front();
}

bool MyUnitImpl::unstickMoveUnit()
{
    /* First validate that the last move command was issued more than 6+LF frames ago */
    if ((frameNow - lastMoveFrame) < (BWAPI::Broodwar->getLatencyFrames() + 6))
    {
        return false;
    }

    /* Now validate that the last move command matches the current move target */
    BWAPI::UnitCommand currentCommand(rawUnit->getLastCommand());
    if (currentCommand.getType() != BWAPI::UnitCommandTypes::Move ||
        !currentCommand.getTargetPosition().isValid() ||
        currentCommand.getTargetPosition() != currentlyMovingTowards)
    {
        return false;
    }

    /* Don't consider the unit stuck if it is within 32 pixels of the target (it may be decelerating or have already arrived) */
    if (rawUnit->getDistance(currentlyMovingTowards) < 32)
    {
        return false;
    }

    /* Don't consider the unit stuck if it is moving and the order is not Guard or PlayerGuard */
    if (rawUnit->isMoving() && (abs(rawUnit->getVelocityX()) > 0.001 || abs(rawUnit->getVelocityY()) > 0.001)
        && rawUnit->getOrder() != BWAPI::Orders::Guard && rawUnit->getOrder() != BWAPI::Orders::PlayerGuard)
    {
        return false;
    }

    /* If we haven't moved for the past 48 frames, assume previous attempts to unstick the unit have failed and try to reset completely */
    if (frameLastMoved < (frameNow - 48))
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Unstick by sending stop command";
#endif
        stop();
        unstickUntil = frameNow + BWAPI::Broodwar->getRemainingLatencyFrames();
        return true;
    }

    /* We are stuck. If we are close to unwalkable terrain, move along it to get us moving again. */
    if (Map::unwalkableProximity(tilePositionX, tilePositionY) < 2)
    {
        /* Scores the distance from a neighbouring tile to the target position */
        /* Prefers tiles that are farther away from unwalkable terrain */
        BWAPI::Position best = BWAPI::Positions::Invalid;
        int bestDist = INT_MAX;
        auto scoreTile = [&currentCommand, &best, &bestDist](BWAPI::TilePosition tilePos)
        {
            if (!tilePos.isValid()) return;
            if (!Map::isWalkable(tilePos)) return;

            auto position = BWAPI::Position(tilePos) + BWAPI::Position(16, 16);
            int span = currentCommand.getTargetPosition().getApproxDistance(position);
            if (Map::unwalkableProximity(tilePos.x, tilePos.y) > 1) span /= 2;
            if (span < bestDist)
            {
                bestDist = span;
                best = position;
            }
        };

        auto currentTile = getTilePosition();
        scoreTile(currentTile + BWAPI::TilePosition(1, 0));
        scoreTile(currentTile + BWAPI::TilePosition(-1, 0));
        scoreTile(currentTile + BWAPI::TilePosition(0, 1));
        scoreTile(currentTile + BWAPI::TilePosition(0, -1));
        scoreTile(currentTile + BWAPI::TilePosition(1, 1));
        scoreTile(currentTile + BWAPI::TilePosition(-1, -1));
        scoreTile(currentTile + BWAPI::TilePosition(-1, 1));
        scoreTile(currentTile + BWAPI::TilePosition(1, -1));

        if (!best.isValid())
        {
            scoreTile(currentTile + BWAPI::TilePosition(2, 0));
            scoreTile(currentTile + BWAPI::TilePosition(-2, 0));
            scoreTile(currentTile + BWAPI::TilePosition(0, 2));
            scoreTile(currentTile + BWAPI::TilePosition(0, -2));
        }

        if (best.isValid())
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Unstick by moving to neighbouring walkable tile";
#endif
            move(best);
            unstickUntil = frameNow + BWAPI::Broodwar->getRemainingLatencyFrames() + 4;
            return true;
        }
    }

#if DEBUG_UNIT_ORDERS
    Telemetry::log(ident) << "Unstick by resending previous move command";
#endif

    /* Reissue the move command */
    move(currentlyMovingTowards, true);
    unstickUntil = frameNow + BWAPI::Broodwar->getRemainingLatencyFrames();
    return true;
}
