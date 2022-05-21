#include "MyWorker.h"

#include "PathFinding.h"
#include "Geo.h"
#include "Map.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
//    BWAPI::Position perpendicularPosition(BWAPI::Position myPosition,
//                                          BWAPI::Position targetPosition,
//                                          BWAPI::Position myCurrentMoveTarget,
//                                          int myCurrentHeading,
//                                          int length)
//    {
//        auto attackVector = Geo::PerpendicularVector(targetPosition - myPosition, length);
//        if (attackVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;
//
//        // Pick the preferred perpendicular position
//        auto first = targetPosition + attackVector;
//        auto second = targetPosition - attackVector;
//
//        // Start by checking if only one is walkable
//        auto firstWalkable = first.isValid() && Map::isWalkable(BWAPI::TilePosition(first));
//        auto secondWalkable = second.isValid() && Map::isWalkable(BWAPI::TilePosition(second));
//        if (!firstWalkable)
//        {
//            if (secondWalkable)
//            {
//                return second;
//            }
//        }
//        else if (!secondWalkable)
//        {
//            return first;
//        }
//        else if (myCurrentMoveTarget.isValid())
//        {
//            auto firstDist = first.getApproxDistance(myCurrentMoveTarget);
//            auto secondDist = second.getApproxDistance(myCurrentMoveTarget);
//            return (secondDist < firstDist) ? second : first;
//        }
//        else
//        {
//            // Otherwise take the position closest to where we are currently pointing
//            auto firstAngleDiff = Geo::BWAngleDiff(Geo::BWDirection(first - myPosition), myCurrentHeading);
//            auto secondAngleDiff = Geo::BWAngleDiff(Geo::BWDirection(second - myPosition), myCurrentHeading);
//            return (secondAngleDiff < firstAngleDiff) ? second : first;
//        }
//
//        return BWAPI::Positions::Invalid;
//    }
}

MyWorker::MyWorker(BWAPI::Unit unit)
        : MyUnitImpl(unit)
        , mineralWalkingPatch(nullptr)
        , mineralWalkingTargetArea(nullptr)
        , mineralWalkingStartPosition(BWAPI::Positions::Invalid)
        , nextAttackPredictedAt(-1)
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
    // Disable new code for now
    MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
    return;

    // If we aren't close to the target (or can't see it), just fall through to normal attack
    int dist = getDistance(target);
    if (dist > 96 || !target->bwapiUnit->isVisible())
    {
        MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
        return;
    }

    auto myPredictedPosition = predictPosition(BWAPI::Broodwar->getLatencyFrames());
    if (!myPredictedPosition.isValid()) myPredictedPosition = lastPosition;

    auto targetPredictedPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames());
    if (!targetPredictedPosition.isValid()) targetPredictedPosition = target->lastPosition;

    auto predictedDist = Geo::EdgeToEdgeDistance(type, myPredictedPosition, target->type, targetPredictedPosition);

    int cooldownFrames = std::max(0, cooldownUntil - currentFrame);

    // Compute the expected number of frames until we are in range if we start an attack now
    auto currentHeading = BWHeading();
    int angleDiff = Geo::BWAngleDiff(Geo::BWDirection(targetPredictedPosition - myPredictedPosition), currentHeading);
    int framesToRange =
            std::max(0, (int)std::ceil((predictedDist - groundRange()) / type.topSpeed()))
            + BWAPI::Broodwar->getLatencyFrames()
            + ((angleDiff / type.turnRadius()) * 2);

    CherryVis::log(id)
            << "dist=" << getDistance(target)
            << ", predictedDist=" << predictedDist
            << ", framesToRange=" << framesToRange
            << ", framesToAngle=" << (angleDiff / type.turnRadius())
            << ", cooldownFrames=" << cooldownFrames
            << ", targetDirection=" << Geo::BWDirection(target->lastPosition - lastPosition)
            << ", heading=" << currentHeading
            << ", angleDiff=" << angleDiff
            << ", currentAngleDiff=" << Geo::BWAngleDiff(Geo::BWDirection(target->lastPosition - lastPosition), currentHeading);

    // Check if we should transition to attack
    auto shouldTransitionToAttack = [&]()
    {
        if (framesToRange < (cooldownFrames - BWAPI::Broodwar->getLatencyFrames())) return false;

        if (dist < 11) return false;

        // If we have an estimation of the enemy order timer, try to time our attack so the enemy's order timer will not allow it to attack
        // while we are in range
        if (target->orderProcessTimer == -1) return true;
        if ((BWAPI::Broodwar->getFrameCount() - 8) % 150 > 135) return true; // timers will be reset soon, so we can't use them for prediction

        int timerAtRange = target->orderProcessTimer - framesToRange;
        while (timerAtRange < 0) timerAtRange += 9;
        CherryVis::log(id) << "Timer @ range: " << timerAtRange;
        return timerAtRange > 6;
    };
    if (nextAttackPredictedAt < (currentFrame - 1) && shouldTransitionToAttack())
    {
        nextAttackPredictedAt = currentFrame + framesToRange;

#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Transitioning to attack: cooldownFrames=" << cooldownFrames
                           << "; framesToRange=" << framesToRange;
#endif
    }

    // Get where we want to move to depending on whether we are attacking or not
    BWAPI::Position moveTarget = BWAPI::Positions::Invalid;

    // We are attacking if the next attack is expected in the future (adjusted for latency)
    if (nextAttackPredictedAt >= (currentFrame + BWAPI::Broodwar->getLatencyFrames()))
    {
        // Send the attack command when we expect to be in range and pointing at the target
        if (cooldownFrames <= BWAPI::Broodwar->getLatencyFrames() && predictedDist <= groundRange() &&
                angleDiff < UnitUtil::GroundWeaponAngle(type) + type.turnRadius())
        {
            nextAttackPredictedAt = currentFrame + BWAPI::Broodwar->getLatencyFrames();
#if DEBUG_UNIT_ORDERS
            CherryVis::log(id) << "Sending attack command: predicted dist at frame " << nextAttackPredictedAt << " is " << predictedDist
                << " and predicted angle diff is " << Geo::BWAngleDiff(Geo::BWDirection(targetPredictedPosition - myPredictedPosition), BWHeading());
#endif
            attack(target->bwapiUnit, true);
            return;
        }

        nextAttackPredictedAt = currentFrame + std::max(BWAPI::Broodwar->getLatencyFrames() + 1, cooldownFrames);

        // If we are on approach to a target that is stationary or moving towards us, try to move to pass it
        // We turn towards to target when we are 2*latency out
//        if (lastPosition.getApproxDistance(targetPredictedPosition) <= lastPosition.getApproxDistance(target->lastPosition) &&
//            (framesToRange > 2*BWAPI::Broodwar->getLatencyFrames() || cooldownFrames > BWAPI::Broodwar->getLatencyFrames()*2))
//        {
//            moveTarget = perpendicularPosition(
//                    lastPosition,
//                    target->lastPosition,
//                    (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Move) ? bwapiUnit->getLastCommand().getTargetPosition()
//                                                                                             : BWAPI::Positions::Invalid,
//                    currentHeading,
//                    groundRange() + (int)(type.topSpeed() * (double)BWAPI::Broodwar->getLatencyFrames()));
//
//#if DEBUG_UNIT_ORDERS
//            if (moveTarget != BWAPI::Positions::Invalid)
//            {
//                CherryVis::log(id) << "Moving to attack @ " << BWAPI::WalkPosition(moveTarget);
//            }
//#endif
//        }

        // Otherwise just move to intercept it
        if (moveTarget == BWAPI::Positions::Invalid)
        {
            moveTarget = intercept(target);
            if (!moveTarget.isValid()) moveTarget = targetPredictedPosition;
#if DEBUG_UNIT_ORDERS
            CherryVis::log(id) << "Moving to intercept @ " << BWAPI::WalkPosition(moveTarget);
#endif
        }
    }
    else
    {
        // We want to move away from the target
        // If we are still pointing towards the target, kite towards the perpendicular point closest to our heading
        // Otherwise just move away from the target
//        if (angleDiff < 0)
//        {
//            moveTarget = perpendicularPosition(
//                    targetPredictedPosition,
//                    myPredictedPosition,
//                    (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Move) ? bwapiUnit->getLastCommand().getTargetPosition()
//                                                                                             : BWAPI::Positions::Invalid,
//                    currentHeading,
//                    48);
//        }
//        else
//        {
            moveTarget = lastPosition + (lastPosition - target->lastPosition);
//        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Moving to kite @ " << BWAPI::WalkPosition(moveTarget);
#endif
    }

    if (moveTarget.isValid())
    {
        // Scale to ensure we move further than our halt distance
        auto vector = Geo::ScaleVector(moveTarget - lastPosition, UnitUtil::HaltDistance(type) + 16);
        if (vector != BWAPI::Positions::Invalid)
        {
            moveTarget = lastPosition + vector;
            if (moveTarget.isValid())
            {
                move(moveTarget);
                return;
            }
        }
    }

    // Fallback if we couldn't generate a valid move position
#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "No valid move target; moving to main";
#endif
    move(Map::getMyMain()->getPosition());
}
