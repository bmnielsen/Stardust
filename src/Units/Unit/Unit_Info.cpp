#include "Unit.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Players.h"

BWAPI::TilePosition UnitImpl::getTilePosition() const
{
    if (buildTile.isValid()) return buildTile;
    return BWAPI::TilePosition{tilePositionX, tilePositionY};
}

bool UnitImpl::isAttackable() const
{
    return bwapiUnit != nullptr &&
           bwapiUnit->exists() &&
           bwapiUnit->isVisible() &&
           !undetected &&
           !bwapiUnit->isStasised();
}

bool UnitImpl::canAttack(const Unit &target) const
{
    return target->isAttackable() && (target->isFlying ? canAttackAir() : canAttackGround());
}

bool UnitImpl::canBeAttackedBy(const Unit &attacker) const
{
    return isAttackable() && (isFlying ? attacker->canAttackAir() : attacker->canAttackGround());
}

bool UnitImpl::canAttackGround() const
{
    return UnitUtil::CanAttackGround(type);
}

bool UnitImpl::canAttackAir() const
{
    return UnitUtil::CanAttackAir(type);
}

BWAPI::WeaponType UnitImpl::getWeapon(const Unit &target) const
{
    return target->isFlying ? UnitUtil::GetAirWeapon(type) : UnitUtil::GetGroundWeapon(type);
}

bool UnitImpl::isInOurWeaponRange(const Unit &target, BWAPI::Position predictedTargetPosition) const
{
    int range = Players::weaponRange(player, target->isFlying ? type.airWeapon() : type.groundWeapon());
    return getDistance(target, predictedTargetPosition) <= range;
}

bool UnitImpl::isInEnemyWeaponRange(const Unit &attacker, BWAPI::Position predictedAttackerPosition) const
{
    int range = Players::weaponRange(attacker->player, isFlying ? attacker->type.airWeapon() : attacker->type.groundWeapon());
    return getDistance(attacker, predictedAttackerPosition) <= range;
}

int UnitImpl::getDistance(BWAPI::Position position) const
{
    return Geo::EdgeToPointDistance(type, lastPosition, position);
}

int UnitImpl::getDistance(const Unit &other, BWAPI::Position predictedOtherPosition) const
{
    return Geo::EdgeToEdgeDistance(type, lastPosition, other->type, predictedOtherPosition.isValid() ? predictedOtherPosition : other->lastPosition);
}

BWAPI::Position UnitImpl::predictPosition(int frames) const
{
    if (!bwapiUnit || !bwapiUnit->exists() || !bwapiUnit->isVisible()) return BWAPI::Positions::Invalid;

    return bwapiUnit->getPosition() + BWAPI::Position((int) (frames * bwapiUnit->getVelocityX()), (int) (frames * bwapiUnit->getVelocityY()));
}

// Computes the intercept point of a unit targeting another one, assuming the interceptor
// is going at full speed and the target remains on its current trajectory and speed.
// Returns invalid if the target cannot be intercepted.
// Uses the formula derived here: http://jaran.de/goodbits/2011/07/17/calculating-an-intercept-course-to-a-target-with-constant-direction-and-velocity-in-a-2-dimensional-plane/
BWAPI::Position UnitImpl::intercept(const Unit &target) const
{
    double speed = type.topSpeed();

    double diffX = (double) target->lastPosition.x - lastPosition.x;
    double diffY = (double) target->lastPosition.y - lastPosition.y;

    double diffSpeed = target->bwapiUnit->getVelocityX() * target->bwapiUnit->getVelocityX()
                       + target->bwapiUnit->getVelocityY() * target->bwapiUnit->getVelocityY() - speed * speed;
    double diffDist = diffX * target->bwapiUnit->getVelocityX() + diffY * target->bwapiUnit->getVelocityY();

    double t;
    if (diffSpeed < 0.0001)
    {
        t = -(diffX * diffX + diffY * diffY) / (2 * diffDist);
    }
    else
    {
        double distSpeedRatio = -diffDist / diffSpeed;
        double d = distSpeedRatio * distSpeedRatio - (diffX * diffX + diffY * diffY) / diffSpeed;
        if (d < 0) return BWAPI::Positions::Invalid;

        double r = sqrt(d);
        t = std::max(distSpeedRatio + r, distSpeedRatio - r);
    }

    if (t < 0) return BWAPI::Positions::Invalid;

    return BWAPI::Position(
            target->lastPosition.x + (int) (t * target->bwapiUnit->getVelocityX()),
            target->lastPosition.y + (int) (t * target->bwapiUnit->getVelocityY()));
}
