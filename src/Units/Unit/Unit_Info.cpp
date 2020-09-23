#include "Unit.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Players.h"

std::ostream &operator<<(std::ostream &os, const UnitImpl &unit)
{
    os << unit.type << ":" << unit.id << "@" << BWAPI::WalkPosition(unit.getTilePosition());
    return os;
}

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

bool UnitImpl::isStaticGroundDefense() const
{
    if (!UnitUtil::IsStationaryAttacker(type)) return false;
    if (!UnitUtil::CanAttackGround(type)) return false;
    if (type == BWAPI::UnitTypes::Zerg_Lurker && !burrowed) return false;

    return true;
}

bool UnitImpl::isTransport() const
{
    return type == BWAPI::UnitTypes::Protoss_Shuttle ||
           type == BWAPI::UnitTypes::Terran_Dropship ||
           (type == BWAPI::UnitTypes::Zerg_Overlord && Players::upgradeLevel(player, BWAPI::UpgradeTypes::Ventral_Sacs) > 0);
}

BWAPI::WeaponType UnitImpl::getWeapon(const Unit &target) const
{
    auto weaponUnitType = type;
    if (type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;

    return target->isFlying ? UnitUtil::GetAirWeapon(weaponUnitType) : UnitUtil::GetGroundWeapon(weaponUnitType);
}

bool UnitImpl::isInOurWeaponRange(const Unit &target, BWAPI::Position predictedTargetPosition, int buffer) const
{
    auto weaponUnitType = type;
    if (type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;

    int range = Players::weaponRange(player, target->isFlying ? weaponUnitType.airWeapon() : weaponUnitType.groundWeapon());
    if (type == BWAPI::UnitTypes::Terran_Bunker) range += 32;
    return getDistance(target, predictedTargetPosition) <= (range + buffer);
}

bool UnitImpl::isInEnemyWeaponRange(const Unit &attacker, BWAPI::Position predictedAttackerPosition, int buffer) const
{
    auto weaponUnitType = attacker->type;
    if (attacker->type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;

    int range = Players::weaponRange(attacker->player, isFlying ? weaponUnitType.airWeapon() : weaponUnitType.groundWeapon());
    if (attacker->type == BWAPI::UnitTypes::Terran_Bunker) range += 32;
    return getDistance(attacker, predictedAttackerPosition) <= (range + buffer);
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
    if (!bwapiUnit || !bwapiUnit->exists() || !bwapiUnit->isVisible() || type.topSpeed() < 0.001) return lastPosition;

    return bwapiUnit->getPosition() + BWAPI::Position((int) (frames * bwapiUnit->getVelocityX()), (int) (frames * bwapiUnit->getVelocityY()));
}

// Computes the intercept point of a unit targeting another one, assuming the interceptor
// is going at full speed and the target remains on its current trajectory and speed.
// Returns invalid if the target cannot be intercepted.
// Uses the formula derived here: http://jaran.de/goodbits/2011/07/17/calculating-an-intercept-course-to-a-target-with-constant-direction-and-velocity-in-a-2-dimensional-plane/
BWAPI::Position UnitImpl::intercept(const Unit &target) const
{
    if (!target->bwapiUnit || !target->bwapiUnit->exists() || !target->bwapiUnit->isVisible() || target->type.topSpeed() < 0.001)
    {
        return target->lastPosition;
    }

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
