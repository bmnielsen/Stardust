#include "MyWorker.h"

#include "PathFinding.h"
#include "Geo.h"
#include "Map.h"
#include "UnitUtil.h"
#include "OrderProcessTimer.h"

#include "DebugFlag_UnitOrders.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_ORDERPROCESSTIMER false
#define VALIDATE_ORDERPROCESSTIMER true
#endif

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

MyWorkerImpl::MyWorkerImpl(BWAPI::Unit unit)
        : MyUnitImpl(unit)
        , carryingResource(unit->isCarryingMinerals() || unit->isCarryingGas())
        , lastCarryingResourceChange(-1)
        , lastStartedMining(-1)
        , lastTransitionedToMiningOrder(-1)
        , lastTransitionedToWaitForMineralsOrder(-1)
        , spawnPosition(BWAPI::Positions::Invalid)
        , horizontalSpeed8b(to8bSpeed(unit->getVelocityX()))
        , verticalSpeed8b(to8bSpeed(unit->getVelocityY()))
        , heading8b(to8bHeading(unit->getAngle()))
        , mineralWalkingPatch(nullptr)
        , mineralWalkingTargetArea(nullptr)
        , mineralWalkingStartPosition(BWAPI::Positions::Invalid)
        , nextAttackPredictedAt(-1)
        , previousOrder(unit->getOrder())
{
}

void MyWorkerImpl::update(BWAPI::Unit unit)
{
    if (!unit || !unit->exists()) return;

    if (!completed && unit->isCompleted())
    {
        spawnPosition = unit->getPosition();
    }

    MyUnitImpl::update(unit);

    // We store an integer representation of the worker's velocity and heading
    // This is used for mining optimizations
    horizontalSpeed8b = to8bSpeed(unit->getVelocityX());
    verticalSpeed8b = to8bSpeed(unit->getVelocityY());
    heading8b = to8bHeading(unit->getAngle());

    // Set the order process timer for gathering workers

    auto isResetFrame = OrderProcessTimer::isResetFrame();
    bool specialPatchLockUnknownCase = false;

    // We know the order process timer is 0 when the worker starts mining, finishes mining, and delivers the resource
    if (carryingResource != (bwapiUnit->isCarryingMinerals() || bwapiUnit->isCarryingGas()))
    {
        carryingResource = (bwapiUnit->isCarryingMinerals() || bwapiUnit->isCarryingGas());
        lastCarryingResourceChange = currentFrame;
        if (!isResetFrame)
        {
            orderProcessTimer = 0;
        }
    }
    else if (bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
    {
        // Record the transition to mining order
        if (previousOrder != BWAPI::Orders::MiningMinerals)
        {
            lastTransitionedToMiningOrder = currentFrame;
            if (!isResetFrame)
            {
                // There is one exception to this: in the case where the worker has patch locked, but hasn't rotated completely towards the patch
                // yet, its order timer will go to 8 instead of 0. This happens exceptionally rarely and isn't trivial to detect, so we just accept
                // that the order timer is off by one in this case.
                orderProcessTimer = 0;
            }
        }

        // Record when mining actually starts
        if (bwapiUnit->getOrderTimer() == 75)
        {
            lastStartedMining = currentFrame;

            // First case here covers patch lock, where it overrides a reset
            if (lastTransitionedToMiningOrder == currentFrame || !isResetFrame)
            {
                orderProcessTimer = 8;
            }
        }
    }
    else if ((gatherCommandFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames()) && !isResetFrame)
          || gatherCommandFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1)
          || (returnCommandFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames()) && !isResetFrame))
    {
        orderProcessTimer = 0;
    }
    else if ((gatherCommandFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames() - 2) && !isResetFrame)
          || returnCommandFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1))
    {
        orderProcessTimer = 8;
    }
    else if (bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
    {
        if (previousOrder != BWAPI::Orders::WaitForMinerals)
        {
            // On first frame the order process timer stays at 0
            lastTransitionedToWaitForMineralsOrder = currentFrame;
            if (!OrderProcessTimer::isResetFrame())
            {
                orderProcessTimer = 0;
            }
        }
        else if (lastTransitionedToWaitForMineralsOrder == (currentFrame - 1))
        {
            // Second frame indicates patch locking has just occurred, in which case the order process timer also stays at 0 for one additional frame
            if (!OrderProcessTimer::isResetFrame())
            {
                orderProcessTimer = 0;
            }
            else
            {
                // If there is a reset frame on the second WaitForMinerals frame, we get a state where the extra zero frame is deferred by the
                // reset. Since we have no way of knowing when the extra frame will happen, we set the possible values to all 9 to indicate we
                // don't actually know anything about the value
                specialPatchLockUnknownCase = true;
            }
        }
    }

    // Update the possible order process timer values
    // If the order process timer is known, set it directly
    if (orderProcessTimer != -1)
    {
        possibleOrderProcessTimerValues = {orderProcessTimer};
    }
    else
    {
        // If this is a reset frame, start by setting the possible values to what they were after the reset (but before the worker's orders were
        // processed)
        if (OrderProcessTimer::isResetFrame())
        {
            possibleOrderProcessTimerValues = {0, 1, 2, 3, 4, 5, 6, 7};

            // Exception for the special patch lock case described above, where we add on the final possible value
            if (specialPatchLockUnknownCase)
            {
                possibleOrderProcessTimerValues.insert(8);
            }
        }

        // We can exclude the order timer having been zero on this frame if we know the worker's order wasn't processed
        // Currently we are only tracking this for end of mining, since it is trivial to compute based on the worker's current state
        // We could also do it for states like waiting to transition to mine or transition to WaitForMinerals, but we don't have a solid use case for
        // them right now
        if (possibleOrderProcessTimerValues.contains(0) && bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals && bwapiUnit->getOrderTimer() == 0)
        {
            possibleOrderProcessTimerValues.erase(0);
        }

        // Run the order process timer cycle on each value
        std::multiset<int> newPossibleOrderProcessTimerValues;
        for (auto value : possibleOrderProcessTimerValues)
        {
            if (value == 0)
            {
                newPossibleOrderProcessTimerValues.insert(8);
            }
            else
            {
                newPossibleOrderProcessTimerValues.insert(value - 1);
            }
        }
        possibleOrderProcessTimerValues = std::move(newPossibleOrderProcessTimerValues);
    }

#if DEBUG_ORDERPROCESSTIMER || VALIDATE_ORDERPROCESSTIMER
    std::ostringstream values;
    std::string sep;
    for (auto value : possibleOrderProcessTimerValues)
    {
        values << sep << value;
        sep = ",";
    }
#if DEBUG_ORDERPROCESSTIMER
    CherryVis::log(id) << "Timer: actual=" << bwapiUnit->getOrderProcessTimer() << "; predicted=[" << values.str() << "]";
#endif

#if VALIDATE_ORDERPROCESSTIMER
    if (orderProcessTimer < 9 && !possibleOrderProcessTimerValues.empty())
    {
        if (!possibleOrderProcessTimerValues.contains(bwapiUnit->getOrderProcessTimer()))
        {
            Log::Get() << "Order process timer wrong, " << bwapiUnit->getOrderProcessTimer() << " not in [" << values.str() << "]"
                       << "; worker " << id << " @ " << getTilePosition();
        }
    }
#endif
#endif
    
    previousOrder = bwapiUnit->getOrder();
}

int8_t MyWorkerImpl::to8bSpeed(double value)
{
    // Worker top speed is 5, so multiplying by 25 maps this to -125 to 125, fitting into an 8-bit integer
    return int8_t(value * 25.0);
}

uint8_t MyWorkerImpl::to8bHeading(double value)
{
    // Headings are from 0 to 2pi, so multiplying by 40 maps this to 0 to 252, fitting into an 8-bit unsigned integer
    return uint8_t(value * 40.0);
}

void MyWorkerImpl::resetMoveData()
{
    MyUnitImpl::resetMoveData();
    mineralWalkingPatch = nullptr;
    mineralWalkingTargetArea = nullptr;
    mineralWalkingStartPosition = BWAPI::Positions::Invalid;
}


bool MyWorkerImpl::mineralWalk(const Choke *choke)
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

void MyWorkerImpl::attackUnit(const Unit &target,
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
    auto targetPredictedPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames());
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
        if (OrderProcessTimer::framesToNextReset() < 15) return true; // timers will be reset soon, so we can't use them for prediction

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
