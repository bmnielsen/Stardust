#include "MyDragoon.h"

#include "UnitUtil.h"
#include "Units.h"
#include "Geo.h"
#include "Boids.h"
#include "Map.h"
#include "NoGoAreas.h"
#include "Config.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    const int DRAGOON_ATTACK_FRAMES = 6;

    /* Boid parameters for kiting */
    /* TODO: These parameters need to be tuned */
    const double targetWeight = 64.0;
    const double separationDetectionLimitFactor = 1.5;
    const double separationWeight = 96.0;
}

MyDragoon::MyDragoon(BWAPI::Unit gameUnit)
        : MyUnitImpl(gameUnit)
        , lastAttackStartedAt(0)
        , nextAttackPredictedAt(0)
        , potentiallyStuckSince(0)
        , stopGlitchGuardUntil(0)
{
}

void MyDragoon::update(BWAPI::Unit gameUnit)
{
    if (!gameUnit || !gameUnit->exists()) return;

    /* Set lastAttackStartedAt on the frame where our cooldown starts */
    int cooldown = std::max(gameUnit->getGroundWeaponCooldown(), gameUnit->getAirWeaponCooldown());
    if (frameNow + cooldown - cooldownUntil > 1)
    {
        lastAttackStartedAt = frameNow;
        potentiallyStuckSince = frameNow + DRAGOON_ATTACK_FRAMES;
    }

    /* Clear potentiallyStuckSince if the unit has moved this frame or has stopped */
    if (potentiallyStuckSince <= frameNow &&
        (!gameUnit->isMoving() || gameUnit->getPosition() != prevPos))
    {
        potentiallyStuckSince = 0;
    }

    MyUnitImpl::update(gameUnit);

    /* If we're not currently in an attack, predict the frame when the next attack will start */
    if (frameNow - lastAttackStartedAt > DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        nextAttackPredictedAt = 0;

        if (rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
            rawUnit->getLastCommand().getTarget() && rawUnit->getLastCommand().getTarget()->exists() &&
            rawUnit->getLastCommand().getTarget()->isVisible() && rawUnit->getLastCommand().getTarget()->getPosition().isValid())
        {
            nextAttackPredictedAt = std::max(
                    frameNow + 1, std::max(
                            lastCommandFrame + BWAPI::Broodwar->getLatencyFrames(),
                            cooldownUntil));
        }
    }
}

bool MyDragoon::unstick()
{
    if (MyUnitImpl::unstick()) return true;

    /* This checks for the case of cancelled attacks sticking a dragoon */
    if (rawUnit->isMoving() && potentiallyStuckSince > 0
        && potentiallyStuckSince < (frameNow - BWAPI::Broodwar->getLatencyFrames() - 10))
    {
        stop();
        unstickUntil = frameNow + BWAPI::Broodwar->getRemainingLatencyFrames();
        potentiallyStuckSince = 0;
        return true;
    }

    return false;
}

bool MyDragoon::isReady() const
{
    if (!MyUnitImpl::isReady()) return false;

    /* If the last attack has just started, give the dragoon some frames to complete it */
    if (frameNow - lastAttackStartedAt + BWAPI::Broodwar->getRemainingLatencyFrames() <= DRAGOON_ATTACK_FRAMES)
    {
        return false;
    }

    /* If the next attack is predicted to happen within remaining latency frames, we may want to leave the dragoon alone */
    int framesToNextAttack = nextAttackPredictedAt - frameNow;
    if (framesToNextAttack > 0 && framesToNextAttack <= BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        BWAPI::Unit target = rawUnit->getLastCommand().getTarget();

        /* Allow switching targets if the current target no longer exists */
        if (!target || !target->exists() || !target->isVisible() || !target->getPosition().isValid()) return true;

        /* Also allow switching targets if we don't have a unit entry for it */
        Unit targetUnit = UnitMgr::get(target);
        if (!targetUnit) return true;

        /* Otherwise only allow switching targets if the current one is expected to be too far away to attack */
        BWAPI::Position myPredictedPosition = predictPosition(framesToNextAttack);
        BWAPI::Position targetPredictedPosition = targetUnit->predictPosition(framesToNextAttack);
        int predictedDistance = Geom::EdgeToEdgeDistance(type, myPredictedPosition, targetUnit->type, targetPredictedPosition);
        return predictedDistance > range(targetUnit);
    }

    return true;
}

void MyDragoon::attackUnit(const Unit &target,
                           std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
                           bool clusterAttacking,
                           int enemyAoeRadius)
{
    int cooldown = target->isFlying ? rawUnit->getAirWeaponCooldown() : rawUnit->getGroundWeaponCooldown();

    /* BB-port stop-glitch guard (S7/S8 backlog item): a queued Attack_Unit whose */
    /* attack animation hasn't started yet gets interrupted by any new move order, */
    /* leaving the dragoon doing nothing for the rest of the cooldown. When an */
    /* attack command is pending and we're still on cooldown, stop() once so the */
    /* unit holds position until the attack starts; the guard window expires with */
    /* the cooldown so normal kiting resumes. Mirrors BananaBrain Micro.cpp. */
    if (rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        cooldown > BWAPI::Broodwar->getRemainingLatencyFrames() + 2 &&
        stopGlitchGuardUntil < frameNow)
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Dragoon stop-glitch guard: stop() to preserve pending attack";
#endif
        stop();
        stopGlitchGuardUntil = frameNow + cooldown - BWAPI::Broodwar->getRemainingLatencyFrames();
        return;
    }

    int myRange = range(target);
    int targetRange = target->groundRange();
    if (target->type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) targetRange = 96;
    bool rangingBunker = target->type == BWAPI::UnitTypes::Terran_Bunker && myRange > targetRange;

    /* If we are not on cooldown, defer to normal unit attack unless we are ranging a bunker */
    if (!rangingBunker && cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames() + 2)
    {
        /* Special case, if we are in a no-go area, move out of it */
        if (NoGoAreas::isNoGo(tilePositionX, tilePositionY))
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(ident) << "Attack: Moving to avoid no-go area";
#endif
            moveTo(Boids::AvoidNoGoArea(this));
            return;
        }

        MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
        return;
    }

    int currentDistanceToTarget = getDistance(target, target->simPosition);

    /* Handle ranging a bunker and not on cooldown */
    if (rangingBunker && cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames() + 2)
    {
        /* What we do depends on where we are and where we're going */

        auto myPredictedPosition = predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
        int predictedDistanceToTarget = Geom::EdgeToEdgeDistance(type, myPredictedPosition, target->type, target->simPosition);

        /* Well out of range: attack */
        if (predictedDistanceToTarget > myRange)
        {
            MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
            return;
        }

        /* Expect momentum to carry us into range: stop */
        if (currentDistanceToTarget > myRange && predictedDistanceToTarget <= myRange)
        {
            stop();
            return;
        }

        /* In range, out of range of the bunker, bunker is visible: attack */
        if (currentDistanceToTarget > targetRange && currentDistanceToTarget <= myRange && predictedDistanceToTarget >= currentDistanceToTarget &&
            target->lastPositionVisible)
        {
            MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
            return;
        }

        /* Otherwise fall through to move boids */
    }

    BWAPI::Position predictedTargetPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
    int predictedDistanceToTarget = getDistance(target, predictedTargetPosition);

    /* Compute our preferred distance to the target */
    int desiredDistance;

    /* Move towards our target if the cluster is attacking and any of the following is true: */
    /* - The target is a sieged tank */
    /* - The target cannot attack us */
    /* - We are inside a narrow choke and aren't ranging down a bunker */
    /* TODO: Perhaps allow kiting in chokes if this unit doesn't block others */
    if (clusterAttacking &&
        (target->type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
         !canBeAttackedBy(target) ||
         (Map::isInNarrowChoke(getTilePosition()) && !rangingBunker)))
    {
        /* Just short-circuit and move towards the target */
        /* TODO: Consider other threats */
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Skipping kiting and moving towards " << target->type;
#endif
        moveTo(target->simPosition, true);
        return;
    }

    /* For targets moving away from us, desire to be closer so we don't kite out of range and never catch them */
    if (currentDistanceToTarget < predictedDistanceToTarget)
    {
        desiredDistance = std::min(myRange - 48, targetRange + 16);
    }

    else if (rangingBunker)
    {
        /* Move closer, then back up a bit to the desired range */
        desiredDistance = currentDistanceToTarget > myRange ? (myRange - 12) : myRange;
    }

    else if (myRange >= targetRange)
    {
        /* The target is stationary or moving towards us, so kite it if we can */
        int cooldownDistance = (int) ((double) (cooldown - BWAPI::Broodwar->getRemainingLatencyFrames() - 2) * type.topSpeed());
        desiredDistance = std::min(myRange, myRange + (cooldownDistance - (predictedDistanceToTarget - myRange)) / 2);

        /* If the target's range is much lower than ours, keep a bit closer */
        /* Exception for SCVs since they might be repairing something dangerous */
        if (target->type != BWAPI::UnitTypes::Terran_SCV && targetRange <= (myRange - 64)) desiredDistance -= 32;

#if DEBUG_UNIT_BOIDS
        Telemetry::log(ident) << "Kiting: cdwn=" << cooldown << "; dist=" << predictedDistanceToTarget << "; range=" << myRange << "; des="
                           << desiredDistance;
#endif
    }

        /* All others: desire to be at our range */
    else
    {
        desiredDistance = myRange;
    }

    /* Compute boids */

    /* Target boid: tries to get us to our desired distance from the target */
    int targetX = predictedTargetPosition.x - prevPos.x;
    int targetY = predictedTargetPosition.y - prevPos.y;
    if (targetX != 0 || targetY != 0)
    {
        double targetScale = std::max(-targetWeight, std::min(targetWeight, (double) predictedDistanceToTarget - desiredDistance))
                             / (double) Geom::ApproximateDistance(targetX, 0, targetY, 0);
        targetX = (int) ((double) targetX * targetScale);
        targetY = (int) ((double) targetY * targetScale);
    }

    /* Separation boid: don't block friendly units that are not in range of their targets */
    /* Skipped for SCVs as they might be repairing something we don't want to get closer to */
    /* Direction B (2026-09-26 battle forensics Q4): when the enemy has splash (reaver */
    /* scarabs / storm - 446 enemy reavers over 60 games), clustered dragoons are the */
    /* perfect target. Scale the separation parameters with the same formula the Ball */
    /* formation uses, and stop skipping units that are already in range - against */
    /* splash, in-range clustering is the death condition, not a safe state. */
    int separationX = 0;
    int separationY = 0;
    double sepDetection = separationDetectionLimitFactor;
    double sepWeight = separationWeight;
    bool aoeSpread = Config::battleMicroEnabled() && enemyAoeRadius > 0;
    if (aoeSpread)
    {
        sepDetection = ((double) enemyAoeRadius + 16.0) / 32.0;
        sepWeight = 160.0;
    }
    if (target->type != BWAPI::UnitTypes::Terran_SCV)
    {
        for (const auto &other : unitsAndTargets)
        {
            if (other.first->ident == ident) continue;
            if (!other.second) continue;

            /* If we are ranging a bunker and are currently in range, skip separation except from units that are caught too close to the bunker */
            if (rangingBunker && currentDistanceToTarget <= myRange)
            {
                if (other.second->type != BWAPI::UnitTypes::Terran_Bunker || !other.first->isInEnemyWeaponRange(other.second))
                {
                    continue;
                }
            }
                /* Otherwise skip units in range of their targets - unless spreading vs splash */
            else if (!aoeSpread && other.first->isInOurWeaponRange(other.second))
            {
                continue;
            }

            Boids::AddSeparation(this, other.first, sepDetection, sepWeight, separationX, separationY);
        }
    }

    auto pt = Boids::ComputePosition(this, {targetX, separationX}, {targetY, separationY}, 0, 16, true);

#if DEBUG_UNIT_BOIDS
    Telemetry::log(ident) << "Kiting boids; target=" << BWAPI::WalkPosition(prevPos + BWAPI::Position(targetX, targetY))
                       << "; separation=" << BWAPI::WalkPosition(prevPos + BWAPI::Position(separationX, separationY))
                       << "; target=" << BWAPI::WalkPosition(pt);
#endif

    /* If the unit can't move in the desired direction, attack the target instead */
    if (pt == BWAPI::Positions::Invalid)
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Attack boid invalid; attacking";
#endif
        MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
    }
    else
    {
#if DEBUG_UNIT_ORDERS
        Telemetry::log(ident) << "Attack boids: Moving to " << BWAPI::WalkPosition(pt);
#endif
        moveTo(pt, true);
    }
}
