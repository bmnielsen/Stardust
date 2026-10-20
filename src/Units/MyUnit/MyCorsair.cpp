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

    /* Not ready if we are in our attack animation */
    if (lastSeenAttacking > 0 && (lastSeenAttacking + ATTACK_FRAMES) > (frameNow + BWAPI::Broodwar->getRemainingLatencyFrames()))
    {
        return false;
    }

    return true;
}

void MyCorsair::attackUnit(const Unit &target,
                           std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
                           bool clusterAttacking,
                           int enemyAoeRadius)
{
    /* If the enemy is a long way away, move to it */
    int span = getDistance(target);
    if (span > 320 || !target->rawUnit->isVisible())
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Attack: Moving to target @ " << BWAPI::WalkPosition(target->simPosition);
#endif
        moveTo(target->simPosition);
        return;
    }

    auto validateAttack = [&](int framesInFuture)
    {
        auto myPos = (framesInFuture == 0) ? prevPos : simulatePosition(framesInFuture);
        auto targetPos = (framesInFuture == 0) ? target->prevPos : target->predictPosition(framesInFuture);

        auto commandDist = Geom::EdgeToEdgeDistance(type, myPos, target->type, targetPos);
        if (commandDist > airRange()) return false;

        auto myHeading = (framesInFuture == 0) ? BWHeading() : simulateHeading(framesInFuture);
        int angleDiff = Geom::BWAngleDiff(Geom::BWDirection(targetPos - myPos), myHeading);
        return angleDiff <= type.turnRadius();
    };

    /* If we have sent an attack order recently, check if we still expect the attack to take place */
    /* We don't need to consider when we are in our attack animation, as that is handled by isReady */
    if (rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        lastCommandFrame > (frameNow - BWAPI::Broodwar->getLatencyFrames() - 1))
    {
        int lastCommandTakesEffect = lastCommandFrame + BWAPI::Broodwar->getLatencyFrames() - frameNow;
        if (validateAttack(lastCommandTakesEffect))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Attack: Attack is pending and valid; don't touch";
#endif
            return;
        }

#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Attack: Attack is not expected to succeed, fall through to attack logic";
#endif
    }

    /* Determine whether to move or attack */
    /* We move in the following situations: */
    /* - We want to kite the target */
    /* - We are on cooldown */
    /* - We have just finished our attack animation and need to issue a move command to either accelerate or decelerate during the next attack */
    /* - We want to attack but are not in range or do not have the correct attack angle */

    BWAPI::Position moveTarget;
    bool accelerate;
    bool decelerate;

    /* Start by determining if we should kite */
    /* We only kite scourge that might be trying to attack us */
    auto shouldKite = [&]()
    {
        /* Only kite scourge that are moving towards us */
        if (target->type != BWAPI::UnitTypes::Zerg_Scourge) return false;
        if (target->predictPosition(1).getApproxDistance(prevPos) > target->prevPos.getApproxDistance(prevPos)) return false;

        /* Don't kite if another friendly unit is closer to it */
        bool otherCloser = false;
        for (auto &otherUnitAndTarget : unitsAndTargets)
        {
            if (otherUnitAndTarget.first->ident == ident) continue;
            int otherDist = otherUnitAndTarget.first->getDistance(target, target->simPosition);
            if (otherDist < span)
            {
                otherCloser = true;
                break;
            }
        }
        if (otherCloser) return false;

        /* Kite if we are too close */
        if (span < 32) return true;

        /* Kite if we aren't near our top speed */
        int speed = BWSpeed();
        int bwTopSpeed = PlayerInfo::unitBWTopSpeed(player, type);
        if ((speed * 10) < (bwTopSpeed * 9)) return true;

        return false;
    };
    if (shouldKite())
    {
        moveTarget = prevPos + (prevPos - target->prevPos);
        accelerate = true;
        decelerate = true;

#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Moving to kite @ " << BWAPI::WalkPosition(moveTarget);
#endif
    }
    else
    {
        /* Determine if we want to accelerate */

        /* Small helper to return if the first speed is higher than the second, given some buffer */
        auto faster = [](int first, int second, int buffer)
        {
            return first + buffer > second;
        };

        /* We want to accelerate if the enemy is moving away from us and is fast */
        accelerate = faster(PlayerInfo::unitBWTopSpeed(target->player, target->type), BWSpeed(), 10) &&
                     target->predictPosition(1).getApproxDistance(prevPos) > target->prevPos.getApproxDistance(prevPos) &&
                     (faster(target->BWSpeed(), BWSpeed(), 250) || span > 96);

        /* We want to decelerate if the enemy is slower than us and we are in range */
        decelerate = faster(BWSpeed(), PlayerInfo::unitBWTopSpeed(target->player, target->type), -100) && isInOurWeaponRange(target);

        Telemetry::log(ident) << "Target top speed: " << PlayerInfo::unitBWTopSpeed(target->player, target->type)
            << "; my top speed: " << PlayerInfo::unitBWTopSpeed(player, type)
            << "; my speed: " << BWSpeed()
            << "; target moving away: " << (target->predictPosition(1).getApproxDistance(prevPos) > target->prevPos.getApproxDistance(prevPos))
            << "; target speed: " << target->BWSpeed()
            << "; dist: " << span
            << "; accelerate: " << accelerate
            << "; decelerate: " << decelerate
            << "; accelerating: " << rawUnit->isAccelerating();

        /* If this is the frame that our attack animation ended, move if we need to change speed */
        if ((accelerate || (decelerate && rawUnit->isAccelerating()))
            && (lastSeenAttacking + ATTACK_FRAMES) == (frameNow + BWAPI::Broodwar->getRemainingLatencyFrames()))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Attack: Moving for one frame before continuing attack";
#endif
        }
        else if (decelerate || (cooldownUntil - frameNow <= BWAPI::Broodwar->getRemainingLatencyFrames() &&
                 validateAttack(BWAPI::Broodwar->getRemainingLatencyFrames())))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Sending attack command";
#endif
            attack(target->rawUnit, true);
            return;
        }

        moveTarget = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());

#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Attack: Moving to target @ " << BWAPI::WalkPosition(moveTarget);
#endif
    }

    if (accelerate)
    {
        /* Scale to ensure we move further than our halt distance */
        auto vector = Geom::ScaleVector(moveTarget - prevPos, UnitHelper::HaltDistance(type) + 192);
        if (vector != BWAPI::Positions::Invalid)
        {
            moveTarget = prevPos + vector;
        }
    }

    if (decelerate)
    {
        /* Scale depending on where we expect to be */
        auto myPos = frameLastMoved == frameNow ? simulatePosition(BWAPI::Broodwar->getRemainingLatencyFrames()) : prevPos;
        auto targetPos = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
        auto vector = Geom::ScaleVector(targetPos - myPos, 8);
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
        Telemetry::log(ident) << "Attack: No valid move target; moving to main";
#endif
        move(Map::getMyMain()->getPosition());
    }
}
