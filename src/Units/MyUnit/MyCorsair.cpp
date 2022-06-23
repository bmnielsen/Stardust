#include "MyCorsair.h"

#include "Geo.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Players.h"

#include "DebugFlag_UnitOrders.h"

#define ATTACK_FRAMES 8

bool MyCorsair::isReady() const
{
    if (!MyUnitImpl::isReady()) return false;

    // Not ready if we are in our attack animation
    if (lastSeenAttacking > 0 && (lastSeenAttacking + ATTACK_FRAMES) > (currentFrame + BWAPI::Broodwar->getRemainingLatencyFrames()))
    {
        return false;
    }

    return true;
}

void MyCorsair::attackUnit(const Unit &target,
                           std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                           bool clusterAttacking,
                           int enemyAoeRadius)
{
    // If the enemy is a long way away, move to it
    int dist = getDistance(target);
    if (dist > 320 || !target->bwapiUnit->isVisible())
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Attack: Moving to target @ " << BWAPI::WalkPosition(target->lastPosition);
#endif
        moveTo(target->lastPosition);
        return;
    }

    auto validateAttack = [&](int framesInFuture)
    {
        auto myPos = (framesInFuture == 0) ? lastPosition : simulatePosition(framesInFuture);
        auto targetPos = (framesInFuture == 0) ? target->lastPosition : target->predictPosition(framesInFuture);

        auto commandDist = Geo::EdgeToEdgeDistance(type, myPos, target->type, targetPos);
        if (commandDist > airRange()) return false;

        auto myHeading = (framesInFuture == 0) ? BWHeading() : simulateHeading(framesInFuture);
        int angleDiff = Geo::BWAngleDiff(Geo::BWDirection(targetPos - myPos), myHeading);
        return angleDiff <= type.turnRadius();
    };

    // If we have sent an attack order recently, check if we still expect the attack to take place
    // We don't need to consider when we are in our attack animation, as that is handled by isReady
    if (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        lastCommandFrame > (currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1))
    {
        int lastCommandTakesEffect = lastCommandFrame + BWAPI::Broodwar->getLatencyFrames() - currentFrame;
        if (validateAttack(lastCommandTakesEffect))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(id) << "Attack: Attack is pending and valid; don't touch";
#endif
            return;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Attack: Attack is not expected to succeed, fall through to attack logic";
#endif
    }

    // Determine whether to move or attack
    // We move in the following situations:
    // - We want to kite the target
    // - We are on cooldown
    // - We have just finished our attack animation and need to issue a move command to either accelerate or decelerate during the next attack
    // - We want to attack but are not in range or do not have the correct attack angle

    BWAPI::Position moveTarget;
    bool accelerate;
    bool decelerate;

    // Start by determining if we should kite
    // We only kite scourge that might be trying to attack us
    auto shouldKite = [&]()
    {
        // Only kite scourge that are moving towards us
        if (target->type != BWAPI::UnitTypes::Zerg_Scourge) return false;
        if (target->predictPosition(1).getApproxDistance(lastPosition) > target->lastPosition.getApproxDistance(lastPosition)) return false;

        // Don't kite if another friendly unit is closer to it
        bool otherCloser = false;
        for (auto &otherUnitAndTarget : unitsAndTargets)
        {
            if (otherUnitAndTarget.first->id == id) continue;
            int otherDist = otherUnitAndTarget.first->getDistance(target);
            if (otherDist < dist)
            {
                otherCloser = true;
                break;
            }
        }
        if (otherCloser) return false;

        // Kite if we are too close
        if (dist < 32) return true;

        // Kite if we aren't near our top speed
        int speed = BWSpeed();
        int bwTopSpeed = Players::unitBWTopSpeed(player, type);
        if ((speed * 10) < (bwTopSpeed * 9)) return true;

        return false;
    };
    if (shouldKite())
    {
        moveTarget = lastPosition + (lastPosition - target->lastPosition);
        accelerate = true;
        decelerate = true;

#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Moving to kite @ " << BWAPI::WalkPosition(moveTarget);
#endif
    }
    else
    {
        // Determine if we want to accelerate

        // Small helper to return if the first speed is higher than the second, given some buffer
        auto faster = [](int first, int second, int buffer)
        {
            return first + buffer > second;
        };

        // We want to accelerate if the enemy is moving away from us and is fast
        accelerate = faster(Players::unitBWTopSpeed(target->player, target->type), BWSpeed(), 10) &&
                     target->predictPosition(1).getApproxDistance(lastPosition) > target->lastPosition.getApproxDistance(lastPosition) &&
                     (faster(target->BWSpeed(), BWSpeed(), 250) || dist > 96);

        // We want to decelerate if the enemy is slower than us and we are in range
        decelerate = faster(BWSpeed(), Players::unitBWTopSpeed(target->player, target->type), -100) && isInOurWeaponRange(target);

        CherryVis::log(id) << "Target top speed: " << Players::unitBWTopSpeed(target->player, target->type)
            << "; my top speed: " << Players::unitBWTopSpeed(player, type)
            << "; my speed: " << BWSpeed()
            << "; target moving away: " << (target->predictPosition(1).getApproxDistance(lastPosition) > target->lastPosition.getApproxDistance(lastPosition))
            << "; target speed: " << target->BWSpeed()
            << "; dist: " << dist
            << "; accelerate: " << accelerate
            << "; decelerate: " << decelerate
            << "; accelerating: " << bwapiUnit->isAccelerating();

        // If this is the frame that our attack animation ended, move if we need to change speed
        if ((accelerate || (decelerate && bwapiUnit->isAccelerating()))
            && (lastSeenAttacking + ATTACK_FRAMES) == (currentFrame + BWAPI::Broodwar->getRemainingLatencyFrames()))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(id) << "Attack: Moving for one frame before continuing attack";
#endif
        }
        else if (decelerate || (cooldownUntil - currentFrame <= BWAPI::Broodwar->getRemainingLatencyFrames() &&
                 validateAttack(BWAPI::Broodwar->getRemainingLatencyFrames())))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(id) << "Sending attack command";
#endif
            attack(target->bwapiUnit, true);
            return;
        }

        moveTarget = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());

#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Attack: Moving to target @ " << BWAPI::WalkPosition(moveTarget);
#endif
    }

    if (accelerate)
    {
        // Scale to ensure we move further than our halt distance
        auto vector = Geo::ScaleVector(moveTarget - lastPosition, UnitUtil::HaltDistance(type) + 192);
        if (vector != BWAPI::Positions::Invalid)
        {
            moveTarget = lastPosition + vector;
        }
    }

    if (decelerate)
    {
        // Scale depending on where we expect to be
        auto myPos = frameLastMoved == currentFrame ? simulatePosition(BWAPI::Broodwar->getRemainingLatencyFrames()) : lastPosition;
        auto targetPos = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
        auto vector = Geo::ScaleVector(targetPos - myPos, 8);
        if (vector != BWAPI::Positions::Invalid)
        {
            auto potentialMoveTarget = myPos + vector;
            if (potentialMoveTarget.isValid())
            {
                moveTarget = potentialMoveTarget;
            }
        }
    }

    if (moveTarget.isValid())
    {
        move(moveTarget);
    }
    else
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Attack: No valid move target; moving to main";
#endif
        move(Map::getMyMain()->getPosition());
    }
}
