#include "MyDragoon.h"

#include "UnitUtil.h"
#include "Units.h"
#include "Geo.h"
#include "Boids.h"
#include "Map.h"
#include "NoGoAreas.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    const int DRAGOON_ATTACK_FRAMES = 6;

    // Boid parameters for kiting
    // TODO: These parameters need to be tuned
    const double targetWeight = 64.0;
    const double separationDetectionLimitFactor = 1.5;
    const double separationWeight = 96.0;
}

MyDragoon::MyDragoon(BWAPI::Unit unit)
        : MyUnitImpl(unit)
        , lastAttackStartedAt(0)
        , nextAttackPredictedAt(0)
        , potentiallyStuckSince(0)
{
}

void MyDragoon::update(BWAPI::Unit unit)
{
    if (!unit || !unit->exists()) return;

    // Set lastAttackStartedAt on the frame where our cooldown starts
    int cooldown = std::max(unit->getGroundWeaponCooldown(), unit->getAirWeaponCooldown());
    if (currentFrame + cooldown - cooldownUntil > 1)
    {
        lastAttackStartedAt = currentFrame;
        potentiallyStuckSince = currentFrame + DRAGOON_ATTACK_FRAMES;
    }

    // Clear potentiallyStuckSince if the unit has moved this frame or has stopped
    if (potentiallyStuckSince <= currentFrame &&
        (!unit->isMoving() || unit->getPosition() != lastPosition))
    {
        potentiallyStuckSince = 0;
    }

    MyUnitImpl::update(unit);

    // If we're not currently in an attack, predict the frame when the next attack will start
    if (currentFrame - lastAttackStartedAt > DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        nextAttackPredictedAt = 0;

        if (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
            bwapiUnit->getLastCommand().getTarget() && bwapiUnit->getLastCommand().getTarget()->exists() &&
            bwapiUnit->getLastCommand().getTarget()->isVisible() && bwapiUnit->getLastCommand().getTarget()->getPosition().isValid())
        {
            nextAttackPredictedAt = std::max(
                    currentFrame + 1, std::max(
                            lastCommandFrame + BWAPI::Broodwar->getLatencyFrames(),
                            cooldownUntil));
        }
    }
}

bool MyDragoon::unstick()
{
    if (MyUnitImpl::unstick()) return true;

    // This checks for the case of cancelled attacks sticking a dragoon
    if (bwapiUnit->isMoving() && potentiallyStuckSince > 0
        && potentiallyStuckSince < (currentFrame - BWAPI::Broodwar->getLatencyFrames() - 10))
    {
        stop();
        unstickUntil = currentFrame + BWAPI::Broodwar->getRemainingLatencyFrames();
        potentiallyStuckSince = 0;
        return true;
    }

    return false;
}

bool MyDragoon::isReady() const
{
    if (!MyUnitImpl::isReady()) return false;

    // If the last attack has just started, give the dragoon some frames to complete it
    if (currentFrame - lastAttackStartedAt + BWAPI::Broodwar->getRemainingLatencyFrames() <= DRAGOON_ATTACK_FRAMES)
    {
        return false;
    }

    // If the next attack is predicted to happen within remaining latency frames, we may want to leave the dragoon alone
    int framesToNextAttack = nextAttackPredictedAt - currentFrame;
    if (framesToNextAttack > 0 && framesToNextAttack <= BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        BWAPI::Unit target = bwapiUnit->getLastCommand().getTarget();

        // Allow switching targets if the current target no longer exists
        if (!target || !target->exists() || !target->isVisible() || !target->getPosition().isValid()) return true;

        // Also allow switching targets if we don't have a unit entry for it
        Unit targetUnit = Units::get(target);
        if (!targetUnit) return true;

        // Otherwise only allow switching targets if the current one is expected to be too far away to attack
        BWAPI::Position myPredictedPosition = predictPosition(framesToNextAttack);
        BWAPI::Position targetPredictedPosition = targetUnit->predictPosition(framesToNextAttack);
        int predictedDistance = Geo::EdgeToEdgeDistance(type, myPredictedPosition, targetUnit->type, targetPredictedPosition);
        return predictedDistance > range(targetUnit);
    }

    return true;
}

void MyDragoon::attackUnit(const Unit &target,
                           std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                           bool clusterAttacking,
                           int enemyAoeRadius)
{
    int cooldown = target->isFlying ? bwapiUnit->getAirWeaponCooldown() : bwapiUnit->getGroundWeaponCooldown();

    int myRange = range(target);
    int targetRange = target->groundRange();
    if (target->type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) targetRange = 96;
    bool rangingBunker = target->type == BWAPI::UnitTypes::Terran_Bunker && myRange > targetRange;

    // If we are not on cooldown, defer to normal unit attack unless we are ranging a bunker
    if (!rangingBunker && cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames() + 2)
    {
        // Special case, if we are in a no-go area, move out of it
        if (NoGoAreas::isNoGo(tilePositionX, tilePositionY))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(id) << "Attack: Moving to avoid no-go area";
#endif
            moveTo(Boids::AvoidNoGoArea(this));
            return;
        }

        MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
        return;
    }

    int currentDistanceToTarget = getDistance(target, target->simPosition);

    // Handle ranging a bunker and not on cooldown
    if (rangingBunker && cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames() + 2)
    {
        // What we do depends on where we are and where we're going

        auto myPredictedPosition = predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames());
        int predictedDistanceToTarget = Geo::EdgeToEdgeDistance(type, myPredictedPosition, target->type, target->simPosition);

        // Well out of range: attack
        if (predictedDistanceToTarget > myRange)
        {
            MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
            return;
        }

        // Expect momentum to carry us into range: stop
        if (currentDistanceToTarget > myRange && predictedDistanceToTarget <= myRange)
        {
            stop();
            return;
        }

        // In range, out of range of the bunker, bunker is visible: attack
        if (currentDistanceToTarget > targetRange && currentDistanceToTarget <= myRange && predictedDistanceToTarget >= currentDistanceToTarget &&
            target->lastPositionVisible)
        {
            MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
            return;
        }

        // Otherwise fall through to move boids
    }

    BWAPI::Position predictedTargetPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
    int predictedDistanceToTarget = getDistance(target, predictedTargetPosition);

    // Compute our preferred distance to the target
    int desiredDistance;

    // Move towards our target if the cluster is attacking and any of the following is true:
    // - The target is a sieged tank
    // - The target cannot attack us
    // - We are inside a narrow choke and aren't ranging down a bunker
    // TODO: Perhaps allow kiting in chokes if this unit doesn't block others
    if (clusterAttacking &&
        (target->type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
         !canBeAttackedBy(target) ||
         (Map::isInNarrowChoke(getTilePosition()) && !rangingBunker)))
    {
        // Just short-circuit and move towards the target
        // TODO: Consider other threats
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Skipping kiting and moving towards " << target->type;
#endif
        moveTo(target->simPosition, true);
        return;
    }

    // For targets moving away from us, desire to be closer so we don't kite out of range and never catch them
    if (currentDistanceToTarget < predictedDistanceToTarget)
    {
        desiredDistance = std::min(myRange - 48, targetRange + 16);
    }

    else if (rangingBunker)
    {
        // Move closer, then back up a bit to the desired range
        desiredDistance = currentDistanceToTarget > myRange ? (myRange - 12) : myRange;
    }

    else if (myRange >= targetRange)
    {
        // The target is stationary or moving towards us, so kite it if we can
        int cooldownDistance = (int) ((double) (cooldown - BWAPI::Broodwar->getRemainingLatencyFrames() - 2) * type.topSpeed());
        desiredDistance = std::min(myRange, myRange + (cooldownDistance - (predictedDistanceToTarget - myRange)) / 2);

        // If the target's range is much lower than ours, keep a bit closer
        // Exception for SCVs since they might be repairing something dangerous
        if (target->type != BWAPI::UnitTypes::Terran_SCV && targetRange <= (myRange - 64)) desiredDistance -= 32;

#if DEBUG_UNIT_BOIDS
        CherryVis::log(id) << "Kiting: cdwn=" << cooldown << "; dist=" << predictedDistanceToTarget << "; range=" << myRange << "; des="
                           << desiredDistance;
#endif
    }

        // All others: desire to be at our range
    else
    {
        desiredDistance = myRange;
    }

    // Compute boids

    // Target boid: tries to get us to our desired distance from the target
    int targetX = predictedTargetPosition.x - lastPosition.x;
    int targetY = predictedTargetPosition.y - lastPosition.y;
    if (targetX != 0 || targetY != 0)
    {
        double targetScale = std::max(-targetWeight, std::min(targetWeight, (double) predictedDistanceToTarget - desiredDistance))
                             / (double) Geo::ApproximateDistance(targetX, 0, targetY, 0);
        targetX = (int) ((double) targetX * targetScale);
        targetY = (int) ((double) targetY * targetScale);
    }

    // Separation boid: don't block friendly units that are not in range of their targets
    // Skipped for SCVs as they might be repairing something we don't want to get closer to
    int separationX = 0;
    int separationY = 0;
    if (target->type != BWAPI::UnitTypes::Terran_SCV)
    {
        for (const auto &other : unitsAndTargets)
        {
            if (other.first->id == id) continue;
            if (!other.second) continue;

            // If we are ranging a bunker and are currently in range, skip separation except from units that are caught too close to the bunker
            if (rangingBunker && currentDistanceToTarget <= myRange)
            {
                if (other.second->type != BWAPI::UnitTypes::Terran_Bunker || !other.first->isInEnemyWeaponRange(other.second))
                {
                    continue;
                }
            }
                // Otherwise skip units in range of their targets
            else if (other.first->isInOurWeaponRange(other.second))
            {
                continue;
            }

            Boids::AddSeparation(this, other.first, separationDetectionLimitFactor, separationWeight, separationX, separationY);
        }
    }

    auto pos = Boids::ComputePosition(this, {targetX, separationX}, {targetY, separationY}, 0, 16, true);

#if DEBUG_UNIT_BOIDS
    CherryVis::log(id) << "Kiting boids; target=" << BWAPI::WalkPosition(lastPosition + BWAPI::Position(targetX, targetY))
                       << "; separation=" << BWAPI::WalkPosition(lastPosition + BWAPI::Position(separationX, separationY))
                       << "; target=" << BWAPI::WalkPosition(pos);
#endif

    // If the unit can't move in the desired direction, attack the target instead
    if (pos == BWAPI::Positions::Invalid)
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Attack boid invalid; attacking";
#endif
        MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking, enemyAoeRadius);
    }
    else
    {
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Attack boids: Moving to " << BWAPI::WalkPosition(pos);
#endif
        moveTo(pos, true);
    }
}
