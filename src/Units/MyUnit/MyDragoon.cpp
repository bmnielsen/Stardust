#include "MyDragoon.h"

#include <BWEB.h>
#include "UnitUtil.h"
#include "Players.h"

const int DRAGOON_ATTACK_FRAMES = 6;
const double pi = 3.14159265358979323846;

MyDragoon::MyDragoon(BWAPI::Unit unit)
        : MyUnit(unit)
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
    {
        potentiallyStuckSince = 0;
    }

        // If the unit's position has changed after potentially being stuck, it is no longer stuck
    else if (potentiallyStuckSince > 0 && unit->getPosition() != lastPosition)
    {
        potentiallyStuckSince = 0;
    }

        // If we have issued a stop command to the unit on the last turn, it will no longer be stuck after the command is executed
    else if (potentiallyStuckSince > 0 &&
             unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Stop &&
             BWAPI::Broodwar->getRemainingLatencyFrames() == BWAPI::Broodwar->getLatencyFrames())
    {
        potentiallyStuckSince = 0;
    }

        // Otherwise it might have been stuck since the last frame where isAttackFrame==true
    else if (unit->isAttackFrame())
    {
        potentiallyStuckSince = BWAPI::Broodwar->getFrameCount();
    }

    lastPosition = unit->getPosition();
}

bool MyDragoon::isStuck() const
{
    if (MyUnit::isStuck()) return true;

    return unit->isMoving() && potentiallyStuckSince > 0 &&
           potentiallyStuckSince < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 10);
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

void MyDragoon::attackUnit(BWAPI::Unit target)
{
    // Special case for sieged tanks: kite them in the opposite direction, so we get within their minimum range
    // TODO: Use threat grid to determine whether it is actually safer close to the tank
    if (target->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
    {
        if (unit->getGroundWeaponCooldown() > BWAPI::Broodwar->getRemainingLatencyFrames())
        {
            move(target->getPosition());
        }
        else
        {
            MyUnit::attackUnit(target);
        }

        return;
    }

    if (shouldKite(target))
    {
        kiteFrom(target->getPosition());
    }
    else
    {
        MyUnit::attackUnit(target);
    }
}

bool MyDragoon::shouldKite(BWAPI::Unit target)
{
    // Don't kite if the target can't attack back
    if (!UnitUtil::CanAttack(target, unit))
    {
        return false;
    }

    // Don't kite if the enemy's range is longer than ours.
    int range = Players::weaponRange(BWAPI::Broodwar->self(), target->isFlying() ? unit->getType().airWeapon() : unit->getType().groundWeapon());
    int targetRange = Players::weaponDamage(target->getPlayer(), target->getType().groundWeapon());
    if (range < targetRange)
    {
        return false;
    }

    // Don't kite if we are soon ready to shoot
    int cooldown = target->isFlying() ? unit->getAirWeaponCooldown() : unit->getGroundWeaponCooldown();
    int dist = unit->getDistance(target);
    int framesToFiringRange = (int) ((double) std::max(0, dist - range) / unit->getType().topSpeed());
    if ((cooldown - BWAPI::Broodwar->getRemainingLatencyFrames() - 2) <= framesToFiringRange)
    {
        return false;
    }

    // Don't kite if the enemy is moving away from us
    BWAPI::Position predictedPosition = UnitUtil::PredictPosition(target, 1);
    if (predictedPosition.isValid() && unit->getDistance(predictedPosition) > unit->getDistance(target->getPosition()))
    {
        return false;
    }

    return true;
}

void MyDragoon::kiteFrom(BWAPI::Position position)
{
    // TODO: Use a threat matrix, maybe it's actually best to move towards the position sometimes

    // Our current angle relative to the target
    BWAPI::Position delta(position - unit->getPosition());
    double angleToTarget = atan2(delta.y, delta.x);

    const auto verifyPosition = [](BWAPI::Position position)
    {
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
    {
        for (int sign = -1; i == 0 ? sign == -1 : sign <= 1; sign += 2)
        {
            // Compute the position two tiles away
            double a = angleToTarget + (i * sign * pi / 6);
            BWAPI::Position currentPosition(
                    unit->getPosition().x - (int) std::round(64.0 * std::cos(a)),
                    unit->getPosition().y - (int) std::round(64.0 * std::sin(a)));

            // Verify it and positions around it
            if (!verifyPosition(currentPosition) ||
                !verifyPosition(currentPosition + BWAPI::Position(-16, -16)) ||
                !verifyPosition(currentPosition + BWAPI::Position(16, -16)) ||
                !verifyPosition(currentPosition + BWAPI::Position(16, 16)) ||
                !verifyPosition(currentPosition + BWAPI::Position(-16, 16)))
            {
                continue;
            }

            bestPosition = currentPosition;
            goto breakOuterLoop;
        }
    }
    breakOuterLoop:;

    if (bestPosition.isValid())
    {
        move(bestPosition);
    }
}
