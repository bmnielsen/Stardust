#include "MyUnit.h"

#include <BWEB.h>
#include "Map.h"
#include "PathFinding.h"
#include "UnitUtil.h"
#include "Players.h"

const double pi = 3.14159265358979323846;

const int DRAGOON_ATTACK_FRAMES = 6;

namespace { auto & bwemMap = BWEM::Map::Instance(); }

MyUnit::MyUnit(BWAPI::Unit unit)
    : unit(unit)
    , issuedOrderThisFrame(false)
    , targetPosition(BWAPI::Positions::Invalid)
    , currentlyMovingTowards(BWAPI::Positions::Invalid)
    , mineralWalkingPatch(nullptr)
    , mineralWalkingTargetArea(nullptr)
    , mineralWalkingStartPosition(BWAPI::Positions::Invalid)
    , lastMoveFrame(0)
    , lastAttackStartedAt(0)
    , lastPosition(BWAPI::Positions::Invalid)
    , potentiallyStuckSince(0)
{
}

void MyUnit::update()
{
    if (!unit || !unit->exists()) { return; }

    issuedOrderThisFrame = false;

    if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon) updateGoon();

    // FIXME: This seems fishy, we're updating this early in the frame so it is actually the current position
    lastPosition = unit->getPosition();

    // If a worker is stuck, order it to move again
    // This will often happen when a worker can't get out of the mineral line to build something
    if (unit->getType().isWorker() &&
        unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Move &&
        unit->getLastCommand().getTargetPosition().isValid() &&
        (unit->getOrder() == BWAPI::Orders::PlayerGuard || !unit->isMoving()))
    {
        move(unit->getLastCommand().getTargetPosition());
        return;
    }

    updateMoveWaypoints();
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
    waypoints.clear();
    targetPosition = BWAPI::Positions::Invalid;
    currentlyMovingTowards = BWAPI::Positions::Invalid;
    mineralWalkingPatch = nullptr;
    mineralWalkingTargetArea = nullptr;
    mineralWalkingStartPosition = BWAPI::Positions::Invalid;

    // If the unit is already in the same area, or the target doesn't have an area, just move it directly
    auto targetArea = bwemMap.GetArea(BWAPI::WalkPosition(position));
    if (!targetArea || targetArea == bwemMap.GetArea(BWAPI::WalkPosition(unit->getPosition())))
    {
        move(position);
        return true;
    }

    // Get the BWEM path
    // TODO: Consider narrow chokes
    auto& path = PathFinding::GetChokePointPath(
        unit->getPosition(),
        position,
        unit->getType(),
        PathFinding::PathFindingOptions::UseNearestBWEMArea);
    if (path.empty()) return false;

    for (const BWEM::ChokePoint * chokepoint : path)
        waypoints.push_back(chokepoint);

    // Start moving
    targetPosition = position;
    moveToNextWaypoint();
    return true;
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

    if (mineralWalkingPatch)
    {
        mineralWalk();
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

    const BWEM::ChokePoint * nextWaypoint = *waypoints.begin();

    // Check if the next waypoint needs to be mineral walked
    auto choke = Map::choke(nextWaypoint);
    if (choke->requiresMineralWalk)
    {
        // Determine which of the two areas accessible by the choke we are moving towards.
        // We do this by looking at the waypoint after the next one and seeing which area they share,
        // or by looking at the area of the target position if there are no more waypoints.
        if (waypoints.size() == 1)
        {
            mineralWalkingTargetArea = bwemMap.GetNearestArea(BWAPI::WalkPosition(targetPosition));
        }
        else
        {
            mineralWalkingTargetArea = nextWaypoint->GetAreas().second;

            if (nextWaypoint->GetAreas().first == waypoints[1]->GetAreas().first ||
                nextWaypoint->GetAreas().first == waypoints[1]->GetAreas().second)
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
        mineralWalk();
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

void MyUnit::mineralWalk()
{
    // If we're close to the patch, or if the patch is null and we've moved beyond the choke,
    // we're done mineral walking
    if ((mineralWalkingPatch && unit->getDistance(mineralWalkingPatch) < 32) ||
        (!mineralWalkingPatch &&
            bwemMap.GetArea(unit->getTilePosition()) == mineralWalkingTargetArea &&
            unit->getDistance(BWAPI::Position(waypoints[0]->Center())) > 100))
    {
        mineralWalkingPatch = nullptr;
        mineralWalkingTargetArea = nullptr;
        mineralWalkingStartPosition = BWAPI::Positions::Invalid;

        // Move to the next waypoint
        waypoints.pop_front();
        moveToNextWaypoint();
        return;
    }

    // Re-issue orders every second
    if (BWAPI::Broodwar->getFrameCount() - lastMoveFrame < 24) return;

    // If the patch is null, click on any visible patch on the correct side of the choke
    if (!mineralWalkingPatch)
    {
        for (const auto staticNeutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (!staticNeutral->getType().isMineralField()) continue;
            if (!staticNeutral->exists() || !staticNeutral->isVisible()) continue;

            // The path to this mineral field should cross the choke we're mineral walking
            for (auto choke : PathFinding::GetChokePointPath(
                unit->getPosition(),
                staticNeutral->getInitialPosition(),
                unit->getType(),
                PathFinding::PathFindingOptions::UseNearestBWEMArea))
            {
                if (choke == *waypoints.begin())
                {
                    // The path went through the choke, let's use this field
                    rightClick(staticNeutral);
                    lastMoveFrame = BWAPI::Broodwar->getFrameCount();
                    return;
                }
            }
        }

        // We couldn't find any suitable visible mineral patch, warn and abort
        Log::Debug() << "Error: Unable to find mineral patch to use for mineral walking";

        waypoints.clear();
        targetPosition = BWAPI::Positions::Invalid;
        currentlyMovingTowards = BWAPI::Positions::Invalid;
        mineralWalkingPatch = nullptr;
        mineralWalkingTargetArea = nullptr;
        mineralWalkingStartPosition = BWAPI::Positions::Invalid;
        return;
    }

    // If the patch is visible, click on it
    if (mineralWalkingPatch->exists() && mineralWalkingPatch->isVisible())
    {
        rightClick(mineralWalkingPatch);
        lastMoveFrame = BWAPI::Broodwar->getFrameCount();
        return;
    }

    // If we have a start location defined, click on it
    if (mineralWalkingStartPosition.isValid())
    {
        move(mineralWalkingStartPosition);
        return;
    }

    Log::Debug() << "Error: Unable to find tile to mineral walk from";

    waypoints.clear();
    targetPosition = BWAPI::Positions::Invalid;
    currentlyMovingTowards = BWAPI::Positions::Invalid;
    mineralWalkingPatch = nullptr;
    mineralWalkingTargetArea = nullptr;
    mineralWalkingStartPosition = BWAPI::Positions::Invalid;

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
}

void MyUnit::fleeFrom(BWAPI::Position position)
{
    // TODO: Use a threat matrix, maybe it's actually best to move towards the position sometimes

    // Our current angle relative to the target
    BWAPI::Position delta(position - unit->getPosition());
    double angleToTarget = atan2(delta.y, delta.x);

    const auto verifyPosition = [](BWAPI::Position position) {
        // Valid
        if (!position.isValid()) return false;

        // Not blocked by a building
        if (BWEB::Map::getUsedTiles().find(BWAPI::TilePosition(position)) != BWEB::Map::getUsedTiles().end())
            return false;

        // Walkable
        BWAPI::WalkPosition walk(position);
        if (!walk.isValid()) return false;
        if (!BWAPI::Broodwar->isWalkable(walk)) return false;

        // Not blocked by one of our own units
        if (Players::grid(BWAPI::Broodwar->self()).collision(walk) > 0)
        {
            return false;
        }

        return true;
    };

    // Find a valid position to move to that is two tiles further away from the given position
    // Start by considering fleeing directly away, falling back to other angles if blocked
    BWAPI::Position bestPosition = BWAPI::Positions::Invalid;
    for (int i = 0; i <= 3; i++)
        for (int sign = -1; i == 0 ? sign == -1 : sign <= 1; sign += 2)
        {
            // Compute the position two tiles away
            double a = angleToTarget + (i * sign * pi / 6);
            BWAPI::Position position(
                unit->getPosition().x - (int)std::round(64.0 * std::cos(a)),
                unit->getPosition().y - (int)std::round(64.0 * std::sin(a)));

            // Verify it and positions around it
            if (!verifyPosition(position) ||
                !verifyPosition(position + BWAPI::Position(-16, -16)) ||
                !verifyPosition(position + BWAPI::Position(16, -16)) ||
                !verifyPosition(position + BWAPI::Position(16, 16)) ||
                !verifyPosition(position + BWAPI::Position(-16, 16)))
            {
                continue;
            }

            bestPosition = position;
            goto breakOuterLoop;
        }
breakOuterLoop:;

    if (bestPosition.isValid())
    {
        move(bestPosition);
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

bool MyUnit::isReady() const
{
    if (unit->getType() != BWAPI::UnitTypes::Protoss_Dragoon) return true;

    // Compute delta between current frame and when the last attack started / will start
    int attackFrameDelta = BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt;

    // Always give the dragoon some frames to perform their attack before getting another order
    if (attackFrameDelta >= 0 && attackFrameDelta <= DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        return false;
    }

    // The unit might attack within our remaining latency frames
    if (attackFrameDelta < 0 && attackFrameDelta > -BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        BWAPI::Unit target = unit->getLastCommand().getTarget();

        // Allow switching targets if the current target no longer exists
        if (!target || !target->exists() || !target->isVisible() || !target->getPosition().isValid()) return true;

        // Otherwise only allow switching targets if the current one is expected to be too far away to attack
        int distTarget = unit->getDistance(target);
        int distTargetPos = unit->getPosition().getApproxDistance(target->getPosition());

        BWAPI::Position myPredictedPosition = UnitUtil::PredictPosition(unit, -attackFrameDelta);
        BWAPI::Position targetPredictedPosition = UnitUtil::PredictPosition(target, -attackFrameDelta);

        // This is approximate, as we are computing the distance between the center points of the units, while
        // range calculations use the edges. To compensate we add the difference in observed target distance vs. position distance.
        int predictedDistance = myPredictedPosition.getApproxDistance(targetPredictedPosition) - (distTargetPos - distTarget);

        return predictedDistance > (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) ? 6 : 4) * 32;
    }

    return true;
}

bool MyUnit::isStuck() const
{
    if (unit->isStuck()) return true;

    return unit->isMoving() && potentiallyStuckSince > 0 &&
        potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10);
}

void MyUnit::move(BWAPI::Position position)
{
    if (issuedOrderThisFrame) return;
    if (!position.isValid()) return;

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (!unit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Move &&
        currentCommand.getTargetPosition() == position &&
        unit->isMoving())
    {
        return;
    }

    issuedOrderThisFrame = unit->move(position);
}

void MyUnit::attack(BWAPI::Unit target)
{
    if (issuedOrderThisFrame) return;
    if (!target || !target->exists()) return;

    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
    if (!unit->isStuck() &&
        currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        currentCommand.getTarget() == target)
    {
        return;
    }

    issuedOrderThisFrame = unit->attack(target);
}

void MyUnit::rightClick(BWAPI::Unit target)
{
    if (issuedOrderThisFrame) return;
    if (!target || !target->exists()) return;

    // Unless is it a mineral field, don't click on the same target again
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(unit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Right_Click_Unit &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return;
        }
    }

    issuedOrderThisFrame = unit->rightClick(target);
}

void MyUnit::gather(BWAPI::Unit target)
{
    if (issuedOrderThisFrame) return;
    if (!target || !target->exists()) return;

    // Unless is it a mineral field, don't gather on the same target again
    if (!target->getType().isMineralField())
    {
        BWAPI::UnitCommand currentCommand(unit->getLastCommand());
        if (currentCommand.getType() == BWAPI::UnitCommandTypes::Gather &&
            currentCommand.getTargetPosition() == target->getPosition())
        {
            return;
        }
    }

    issuedOrderThisFrame = unit->gather(target);
}

void MyUnit::returnCargo()
{
    if (issuedOrderThisFrame) return;

    issuedOrderThisFrame = unit->returnCargo();
}

bool MyUnit::build(BWAPI::UnitType type, BWAPI::TilePosition tile)
{
    if (issuedOrderThisFrame) return false;
    if (!tile.isValid()) return false;

    return issuedOrderThisFrame = unit->build(type, tile);
}

void MyUnit::stop()
{
    if (issuedOrderThisFrame) return;

    issuedOrderThisFrame = unit->stop();
}

void MyUnit::updateGoon()
{
    // If we're not currently in an attack, determine the frame when the next attack will start
    if (lastAttackStartedAt >= BWAPI::Broodwar->getFrameCount() ||
        BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt > DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        lastAttackStartedAt = 0;

        // If this is the start of the attack, set it to now
        if (unit->isStartingAttack())
            lastAttackStartedAt = BWAPI::Broodwar->getFrameCount();

        // Otherwise predict when an attack command might result in the start of an attack
        else if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
            unit->getLastCommand().getTarget() && unit->getLastCommand().getTarget()->exists() &&
            unit->getLastCommand().getTarget()->isVisible() && unit->getLastCommand().getTarget()->getPosition().isValid())
        {
            lastAttackStartedAt = std::max(BWAPI::Broodwar->getFrameCount() + 1, std::max(
                unit->getLastCommandFrame() + BWAPI::Broodwar->getLatencyFrames(),
                BWAPI::Broodwar->getFrameCount() + unit->getGroundWeaponCooldown()));
        }
    }

    // Determine if this unit is stuck

    // If isMoving==false, the unit isn't stuck
    if (!unit->isMoving())
        potentiallyStuckSince = 0;

    // If the unit's position has changed after potentially being stuck, it is no longer stuck
    else if (potentiallyStuckSince > 0 && unit->getPosition() != lastPosition)
        potentiallyStuckSince = 0;

    // If we have issued a stop command to the unit on the last turn, it will no longer be stuck after the command is executed
    else if (potentiallyStuckSince > 0 &&
        unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Stop &&
        BWAPI::Broodwar->getRemainingLatencyFrames() == BWAPI::Broodwar->getLatencyFrames())
    {
        potentiallyStuckSince = 0;
    }

    // Otherwise it might have been stuck since the last frame where isAttackFrame==true
    else if (unit->isAttackFrame())
        potentiallyStuckSince = BWAPI::Broodwar->getFrameCount();
}
