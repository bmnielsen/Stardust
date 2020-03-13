#include "MyDragoon.h"

#include "UnitUtil.h"
#include "Players.h"
#include "Units.h"
#include "Geo.h"
#include "Map.h"

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
        , lastPosition(BWAPI::Positions::Invalid)
        , lastAttackStartedAt(0)
        , potentiallyStuckSince(0)
{
}

void MyDragoon::typeSpecificUpdate()
{
    // If we're not currently in an attack, determine the frame when the next attack will start
    if (lastAttackStartedAt >= BWAPI::Broodwar->getFrameCount() ||
        BWAPI::Broodwar->getFrameCount() - lastAttackStartedAt > DRAGOON_ATTACK_FRAMES - BWAPI::Broodwar->getRemainingLatencyFrames())
    {
        lastAttackStartedAt = 0;

        // If this is the start of the attack, set it to now
        if (bwapiUnit->isStartingAttack())
            lastAttackStartedAt = BWAPI::Broodwar->getFrameCount();

            // Otherwise predict when an attack command might result in the start of an attack
        else if (bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
                 bwapiUnit->getLastCommand().getTarget() && bwapiUnit->getLastCommand().getTarget()->exists() &&
                 bwapiUnit->getLastCommand().getTarget()->isVisible() && bwapiUnit->getLastCommand().getTarget()->getPosition().isValid())
        {
            lastAttackStartedAt = std::max(BWAPI::Broodwar->getFrameCount() + 1, std::max(
                    bwapiUnit->getLastCommandFrame() + BWAPI::Broodwar->getLatencyFrames(),
                    BWAPI::Broodwar->getFrameCount() + bwapiUnit->getGroundWeaponCooldown()));
        }
    }

    // Determine if this unit is stuck

    // If isMoving==false, the unit isn't stuck
    if (!bwapiUnit->isMoving())
    {
        potentiallyStuckSince = 0;
    }

        // If the unit's position has changed after potentially being stuck, it is no longer stuck
    else if (potentiallyStuckSince > 0 && bwapiUnit->getPosition() != lastPosition)
    {
        potentiallyStuckSince = 0;
    }

        // If we have issued a stop command to the unit on the last turn, it will no longer be stuck after the command is executed
    else if (potentiallyStuckSince > 0 &&
             bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Stop &&
             BWAPI::Broodwar->getRemainingLatencyFrames() == BWAPI::Broodwar->getLatencyFrames())
    {
        potentiallyStuckSince = 0;
    }

        // Otherwise it might have been stuck since the last frame where isAttackFrame==true
    else if (bwapiUnit->isAttackFrame())
    {
        potentiallyStuckSince = BWAPI::Broodwar->getFrameCount();
    }

    lastPosition = bwapiUnit->getPosition();
}

bool MyDragoon::unstick()
{
    if (MyUnitImpl::unstick()) return true;

    // This checks for the case of cancelled attacks sticking a dragoon
    if (bwapiUnit->isMoving() && potentiallyStuckSince > 0
        && potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10))
    {
        stop();
        lastUnstickFrame = BWAPI::Broodwar->getFrameCount();
        return true;
    }

    return false;
}

bool MyDragoon::isReady() const
{
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
        BWAPI::Unit target = bwapiUnit->getLastCommand().getTarget();

        // Allow switching targets if the current target no longer exists
        if (!target || !target->exists() || !target->isVisible() || !target->getPosition().isValid()) return true;

        // Also allow switching targets if we don't have a unit entry for it
        Unit targetUnit = Units::get(target);
        if (!targetUnit) return true;

        // Otherwise only allow switching targets if the current one is expected to be too far away to attack
        BWAPI::Position myPredictedPosition = predictPosition(-attackFrameDelta);
        BWAPI::Position targetPredictedPosition = targetUnit->predictPosition(-attackFrameDelta);
        int predictedDistance = Geo::EdgeToEdgeDistance(type, myPredictedPosition, targetUnit->type, targetPredictedPosition);
        return predictedDistance > Players::weaponRange(player, getWeapon(targetUnit));
    }

    return true;
}

void MyDragoon::attackUnit(const Unit &target, std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets)
{
    int cooldown = target->isFlying ? bwapiUnit->getAirWeaponCooldown() : bwapiUnit->getGroundWeaponCooldown();

    // If we are not on cooldown, defer to normal unit attack
    if (cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames() + 2)
    {
        MyUnitImpl::attackUnit(target, unitsAndTargets);
        return;
    }

    int range = Players::weaponRange(player, getWeapon(target));
    int targetRange = Players::weaponRange(target->player, UnitUtil::GetGroundWeapon(target->type));
    BWAPI::Position predictedTargetPosition = target->predictPosition(BWAPI::Broodwar->getRemainingLatencyFrames() + 2);
    int currentDistanceToTarget = getDistance(target);
    int predictedDistanceToTarget = getDistance(target, predictedTargetPosition);

    // Compute our preferred distance to the target
    int desiredDistance;

    // Sieged tanks or targets that can't attack us: desire to be close to them
    if (target->type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || !canBeAttackedBy(target))
    {
        // Just short-circuit and move towards the target
        // TODO: Consider other threats
#if DEBUG_UNIT_ORDERS
        CherryVis::log(id) << "Skipping kiting and moving towards " << target->type;
#endif
        move(target->lastPosition);
        return;
    }

        // For targets moving away from us, desire to be closer so we don't kite out of range and never catch them
    else if (currentDistanceToTarget < predictedDistanceToTarget)
    {
        desiredDistance = std::min(range - 48, targetRange + 16);
    }

        // The target is stationary or moving towards us, so kite it if we can
    else if (range >= targetRange)
    {
        int cooldownDistance = (int) ((double) (cooldown - BWAPI::Broodwar->getRemainingLatencyFrames() - 2) * type.topSpeed());
        desiredDistance = std::min(range, range + (cooldownDistance - (predictedDistanceToTarget - range)) / 2);
        CherryVis::log(id) << "Kiting: cdwn=" << cooldown << "; dist=" << predictedDistanceToTarget << "; range=" << range << "; des="
                           << desiredDistance;

        // If the target's range is much lower than ours, keep a bit closer
        if (targetRange <= (range - 64)) desiredDistance -= 32;
    }

        // All others: desire to be at our range
    else
    {
        desiredDistance = range;
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

        // Check if the other unit is in range of its target
        if (other.first->isInOurWeaponRange(other.second)) continue;

        // Push away with maximum force at 0 distance, no force at detection limit
        double distFactor = 1.0 - (double) dist / detectionLimit;
        int centerDist = Geo::ApproximateDistance(lastPosition.x, other.first->lastPosition.x, lastPosition.y, other.first->lastPosition.y);
        double scalingFactor = distFactor * distFactor * separationWeight / centerDist;
        separationX -= (int) ((double) (other.first->lastPosition.x - lastPosition.x) * scalingFactor);
        separationY -= (int) ((double) (other.first->lastPosition.y - lastPosition.y) * scalingFactor);
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
        MyUnitImpl::attackUnit(target, unitsAndTargets);
    }
}
