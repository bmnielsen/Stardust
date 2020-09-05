#include "MyDragoon.h"

#include "UnitUtil.h"
#include "Units.h"
#include "Geo.h"
#include "Map.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    const int DRAGOON_ATTACK_FRAMES = 6;

    // Boid parameters for kiting
    // TODO: These parameters need to be tuned
    const double targetWeight = 32.0;
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
    if (BWAPI::Broodwar->getFrameCount() + cooldown - cooldownUntil > 1)
    {
        lastAttackStartedAt = BWAPI::Broodwar->getFrameCount();
        potentiallyStuckSince = BWAPI::Broodwar->getFrameCount() + DRAGOON_ATTACK_FRAMES;
    }

    // Clear potentiallyStuckSince if the unit has moved this frame or has stopped
    if (potentiallyStuckSince <= BWAPI::Broodwar->getFrameCount() &&
        (!unit->isMoving() || unit->getPosition() != lastPosition))
    {
        potentiallyStuckSince = 0;
    }

    MyUnitImpl::update(unit);

    // If we're not currently in an attack, predict the frame when the next attack will start
    if (BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt > DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        nextAttackPredictedAt = 0;

        if (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
            bwapiUnit->getLastCommand().getTarget() && bwapiUnit->getLastCommand().getTarget()->exists() &&
            bwapiUnit->getLastCommand().getTarget()->isVisible() && bwapiUnit->getLastCommand().getTarget()->getPosition().isValid())
        {
            nextAttackPredictedAt = std::max(
                    BWAPI::Broodwar->getFrameCount() + 1, std::max(
                            bwapiUnit->getLastCommandFrame() + BWAPI::Broodwar->getLatencyFrames(),
                            cooldownUntil));
        }
    }
}

bool MyDragoon::unstick()
{
    if (MyUnitImpl::unstick()) return true;

    // This checks for the case of cancelled attacks sticking a dragoon
    if (bwapiUnit->isMoving() && potentiallyStuckSince > 0
        && potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10))
    {
        stop();
        unstickUntil = BWAPI::Broodwar->getFrameCount() + BWAPI::Broodwar->getRemainingLatencyFrames();
        potentiallyStuckSince = 0;
        return true;
    }

    return false;
}

bool MyDragoon::isReady() const
{
    // If the last attack has just started, give the dragoon some frames to complete it
    if (BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt + BWAPI::Broodwar->getRemainingLatencyFrames() <= DRAGOON_ATTACK_FRAMES)
    {
        return false;
    }

    // If the next attack is predicted to happen within remaining latency frames, we may want to leave the dragoon alone
    int framesToNextAttack = nextAttackPredictedAt - BWAPI::Broodwar->getFrameCount();
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
        if (myPredictedPosition.isValid() && targetPredictedPosition.isValid())
        {
            int predictedDistance = Geo::EdgeToEdgeDistance(type, myPredictedPosition, targetUnit->type, targetPredictedPosition);
            return predictedDistance > range(targetUnit);
        }

        return false;
    }

    return true;
}

void MyDragoon::attackUnit(const Unit &target, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, bool clusterAttacking)
{
    int cooldown = target->isFlying ? bwapiUnit->getAirWeaponCooldown() : bwapiUnit->getGroundWeaponCooldown();

    int myRange = range(target);
    int targetRange = target->groundRange();
    bool rangingBunker = target->type == BWAPI::UnitTypes::Terran_Bunker && myRange > targetRange;

    int currentDistanceToTarget = getDistance(target);

    // Attack if we should do so:
    // - We are not on cooldown
    // - We are not ranging a bunker, or are ranging a bunker and are at an appropriate range
    if (cooldown <= (BWAPI::Broodwar->getRemainingLatencyFrames() + 2) &&
        (!rangingBunker || (currentDistanceToTarget <= myRange
                            && currentDistanceToTarget > targetRange)))
    {
        MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking);
        return;
    }

    BWAPI::Position predictedTargetPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
    int predictedDistanceToTarget;
    if (predictedTargetPosition.isValid())
    {
        predictedDistanceToTarget = getDistance(target, predictedTargetPosition);
    }
    else
    {
        predictedTargetPosition = target->lastPosition;
        predictedDistanceToTarget = currentDistanceToTarget;
    }

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
        moveTo(target->lastPosition, true);
        return;
    }

    // For targets moving away from us, desire to be closer so we don't kite out of range and never catch them
    if (currentDistanceToTarget < predictedDistanceToTarget)
    {
        desiredDistance = std::min(myRange - 48, targetRange + 16);
    }

    else if (rangingBunker)
    {
        desiredDistance = currentDistanceToTarget > myRange ? (myRange - 24) : myRange;
    }

        // The target is stationary or moving towards us, so kite it if we can
    else if (myRange >= targetRange)
    {
        int cooldownDistance = (int) ((double) (cooldown - BWAPI::Broodwar->getRemainingLatencyFrames() - 2) * type.topSpeed());
        desiredDistance = std::min(myRange, myRange + (cooldownDistance - (predictedDistanceToTarget - myRange)) / 2);
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Kiting: cdwn=" << cooldown << "; dist=" << predictedDistanceToTarget << "; range=" << myRange << "; des="
                           << desiredDistance;
#endif

        // If the target's range is much lower than ours, keep a bit closer
        if (targetRange <= (myRange - 64)) desiredDistance -= 32;
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
    int separationX = 0;
    int separationY = 0;
    for (const auto &other : unitsAndTargets)
    {
        if (other.first->id == id) continue;
        if (!other.second) continue;

        // Check if the other unit is within our detection limit
        auto dist = getDistance(other.first);
        double detectionLimit = std::max(type.width(), other.first->type.width()) * separationDetectionLimitFactor;
        if (dist >= (int) detectionLimit) continue;

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

        // Push away with maximum force at 0 distance, no force at detection limit
        double distFactor = 1.0 - (double) dist / detectionLimit;
        int centerDist = Geo::ApproximateDistance(lastPosition.x, other.first->lastPosition.x, lastPosition.y, other.first->lastPosition.y);
        double scalingFactor = distFactor * distFactor * separationWeight / centerDist;
        separationX -= (int) ((double) (other.first->lastPosition.x - lastPosition.x) * scalingFactor);
        separationY -= (int) ((double) (other.first->lastPosition.y - lastPosition.y) * scalingFactor);
    }

    // Clear the separation boid when ranging bunkers if it would take us into bunker range
    if (rangingBunker && currentDistanceToTarget > targetRange && (separationX != 0 || separationY != 0))
    {
        auto separationPos = lastPosition + BWAPI::Position(separationX, separationY);
        int separationDist = Geo::EdgeToEdgeDistance(type,
                                                     separationPos,
                                                     target->type,
                                                     predictedTargetPosition);
        if (separationDist < myRange)
        {
            // Push the separation position out of the target's range
            // This will allow us to move a bit laterally but not towards the bunker
            auto vector = separationPos - predictedTargetPosition;
            auto scaledVector = Geo::ScaleVector(vector, Geo::ApproximateDistance(vector.x, 0, vector.y, 0) + myRange - separationDist);
            CherryVis::log(id) << "vector: " << vector << "; scale to="
                               << (Geo::ApproximateDistance(vector.x, 0, vector.y, 0) + myRange - separationDist)
                               << "; scaled=" << scaledVector;

            if (scaledVector != BWAPI::Positions::Invalid)
            {
                CherryVis::log(id) << "Adjusting separation: originally (" << separationX << "," << separationY << "), "
                                   << "dist=" << separationDist
                                   << ", now (" << (lastPosition.x - (predictedTargetPosition.x + scaledVector.x))
                                   << "," << (lastPosition.y - (predictedTargetPosition.y + scaledVector.y))
                                   << ")";
                CherryVis::log(id) << "Original pos: " << BWAPI::WalkPosition(separationPos)
                << "; scaled pos: " << BWAPI::WalkPosition(predictedTargetPosition + scaledVector)
                << "; new pos: " << BWAPI::WalkPosition(lastPosition + BWAPI::Position(predictedTargetPosition.x + scaledVector.x - lastPosition.x, predictedTargetPosition.y + scaledVector.y - lastPosition.y));

                separationX = predictedTargetPosition.x + scaledVector.x - lastPosition.x;
                separationY = predictedTargetPosition.y + scaledVector.y - lastPosition.y;
            }
            else
            {
                separationX = 0;
                separationY = 0;
            }
        }
    }

    // TODO: Collision with buildings / terrain

    // Put them all together to get the target direction
    int totalX = targetX + separationX;
    int totalY = targetY + separationY;

    auto pos = BWAPI::Position(lastPosition.x + totalX, lastPosition.y + totalY);
#if DEBUG_UNIT_ORDERS
    CherryVis::log(id) << "Kiting boids; target=" << BWAPI::WalkPosition(lastPosition + BWAPI::Position(targetX, targetY))
                       << "; separation=" << BWAPI::WalkPosition(lastPosition + BWAPI::Position(separationX, separationY))
                       << "; target=" << BWAPI::WalkPosition(pos);
#endif

    if (pos.isValid())
    {
        moveTo(pos, true);
    }
    else
    {
        MyUnitImpl::attackUnit(target, unitsAndTargets, clusterAttacking);
    }
}
