#include "Unit.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Players.h"
#include "PathFinding.h"

std::ostream &operator<<(std::ostream &os, const UnitImpl &gameUnit)
{
    os << gameUnit.type << ":" << gameUnit.ident << "@" << BWAPI::WalkPosition(gameUnit.prevPos);
    if (gameUnit.simPosition != gameUnit.prevPos) os << "->" << BWAPI::WalkPosition(gameUnit.simPosition);
    return os;
}

BWAPI::TilePosition UnitImpl::getTilePosition() const
{
    if (buildTile.isValid()) return buildTile;
    return BWAPI::TilePosition{tilePositionX, tilePositionY};
}

int UnitImpl::BWHeading() const
{
    if (!bwHeadingUpdated)
    {
        /* Do the reverse of what BWAPI does in UnitUpdate to get BW's heading */
        bwHeading = (int)round((lastAngle * 128.0) / 3.14159265358979323846);
        bwHeading += 64;
        if (bwHeading > 127) bwHeading -= 256;

        bwHeadingUpdated = true;
    }
    return bwHeading;
}

int UnitImpl::BWSpeed() const
{
    if (!bwSpeedUpdated)
    {
        double speed = sqrt(rawUnit->getVelocityX() * rawUnit->getVelocityX() + rawUnit->getVelocityY() * rawUnit->getVelocityY());
        bwSpeed = (int)(speed * 256.0);
        bwSpeedUpdated = true;
    }

    return bwSpeed;
}

bool UnitImpl::isTraining() const
{
    if (!exists() || !completed ) return false;
    if (rawUnit->isTraining()) return true;

    if (rawUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Train &&
        (frameNow - lastCommandFrame - 1) <= BWAPI::Broodwar->getLatencyFrames())
    {
        return true;
    }

    return false;
}

bool UnitImpl::isAttackable() const
{
    return exists() &&
           !undetected &&
           !rawUnit->isStasised();
}

bool UnitImpl::isCliffedTank(const Unit &attacker) const
{
    if (!attacker) return false;
    if (type != BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) return false;

    /* For now let's assume the tank is not directly attackable if a narrow choke divides it from the rest of the cluster */
    return nullptr != Pathfinder::SeparatingNarrowChoke(prevPos,
                                                         attacker->prevPos,
                                                         attacker->type,
                                                         Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
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
    return !immobile && UnitHelper::CanAttackGround(type);
}

bool UnitImpl::canAttackAir() const
{
    return !immobile && UnitHelper::CanAttackAir(type);
}

bool UnitImpl::isStaticGroundDefense() const
{
    if (!UnitHelper::IsStationaryAttacker(type)) return false;
    if (!UnitHelper::CanAttackGround(type)) return false;
    if (type == BWAPI::UnitTypes::Zerg_Lurker && !burrowed) return false;

    return true;
}

bool UnitImpl::isTransport() const
{
    return type == BWAPI::UnitTypes::Protoss_Shuttle ||
           type == BWAPI::UnitTypes::Terran_Dropship ||
           (type == BWAPI::UnitTypes::Zerg_Overlord && PlayerInfo::upgradeLevel(player, BWAPI::UpgradeTypes::Ventral_Sacs) > 0);
}

bool UnitImpl::needsDetection() const
{
    if (type == BWAPI::UnitTypes::Zerg_Lurker || type == BWAPI::UnitTypes::Zerg_Lurker_Egg) return true;
    if (type.hasPermanentCloak()) return true;
    if (type.isCloakable() && PlayerInfo::hasResearched(player, type.cloakingTech())) return true;
    if (type.isBurrowable() && PlayerInfo::hasResearched(player, BWAPI::TechTypes::Burrowing)) return true;

    return false;
}

int UnitImpl::groundRange() const
{
    if (type == BWAPI::UnitTypes::Protoss_Carrier || type == BWAPI::UnitTypes::Protoss_Reaver) return 256;
    if (type == BWAPI::UnitTypes::Terran_Bunker)
    {
        return PlayerInfo::weaponRange(player, BWAPI::UnitTypes::Terran_Marine.groundWeapon()) + 48;
    }

    return PlayerInfo::weaponRange(player, type.groundWeapon());
}

int UnitImpl::airRange() const
{
    if (type == BWAPI::UnitTypes::Protoss_Carrier) return 256;
    if (type == BWAPI::UnitTypes::Terran_Bunker)
    {
        return PlayerInfo::weaponRange(player, BWAPI::UnitTypes::Terran_Marine.airWeapon()) + 48;
    }

    return PlayerInfo::weaponRange(player, type.airWeapon());
}

int UnitImpl::range(const Unit &target) const
{
    return target->isFlying ? airRange() : groundRange();
}

int UnitImpl::groundDamage() const
{
    if (type != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine &&
        ((burrowed && type != BWAPI::UnitTypes::Zerg_Lurker) ||
         (!burrowed && type == BWAPI::UnitTypes::Zerg_Lurker)))
    {
        return 0;
    }

    return PlayerInfo::weaponDamage(player, groundWeapon()) * type.maxGroundHits();
}

int UnitImpl::airDamage() const
{
    return PlayerInfo::weaponDamage(player, airWeapon()) * type.maxAirHits();
}

BWAPI::WeaponType UnitImpl::groundWeapon() const
{
    return UnitHelper::GetGroundWeapon(type);
}

BWAPI::WeaponType UnitImpl::airWeapon() const
{
    return UnitHelper::GetAirWeapon(type);
}

BWAPI::WeaponType UnitImpl::getWeapon(const Unit &target) const
{
    return target->isFlying ? airWeapon() : groundWeapon();
}

bool UnitImpl::isInOurWeaponRange(const Unit &target, BWAPI::Position predictedTargetPosition, int buffer) const
{
    int range = target->isFlying ? airRange() : groundRange();
    return getDistance(target, predictedTargetPosition) <= (range + buffer);
}

bool UnitImpl::isInEnemyWeaponRange(const Unit &attacker, BWAPI::Position predictedAttackerPosition, int buffer) const
{
    int range = isFlying ? attacker->airRange() : attacker->groundRange();
    return getDistance(attacker, predictedAttackerPosition) <= (range + buffer);
}

int UnitImpl::getDistance(BWAPI::Position position) const
{
    return Geom::EdgeToPointDistance(type, prevPos, position);
}

int UnitImpl::getDistance(const Unit &other, BWAPI::Position predictedOtherPosition) const
{
    return Geom::EdgeToEdgeDistance(type, prevPos, other->type, predictedOtherPosition.isValid() ? predictedOtherPosition : other->prevPos);
}

BWAPI::Position UnitImpl::predictPosition(int frames) const
{
    updatePredictedPositions();
    return predictedPositions[frames-1];
}

/* Computes the intercept point of a unit targeting another one, assuming the interceptor */
/* is going at full speed and the target remains on its current trajectory and speed. */
/* Returns invalid if the target cannot be intercepted. */
/* Uses the formula derived here: http://jaran.de/goodbits/2011/07/17/calculating-an-intercept-course-to-a-target-with-constant-direction-and-velocity-in-a-2-dimensional-plane/ */
BWAPI::Position UnitImpl::intercept(const Unit &target) const
{
    if (!target->rawUnit || !target->rawUnit->exists() || !target->rawUnit->isVisible() || target->type.topSpeed() < 0.001)
    {
        return target->prevPos;
    }

    double speed = type.topSpeed();

    double diffX = (double) target->prevPos.x - prevPos.x;
    double diffY = (double) target->prevPos.y - prevPos.y;

    double diffSpeed = target->rawUnit->getVelocityX() * target->rawUnit->getVelocityX()
                       + target->rawUnit->getVelocityY() * target->rawUnit->getVelocityY() - speed * speed;
    double diffDist = diffX * target->rawUnit->getVelocityX() + diffY * target->rawUnit->getVelocityY();

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
    if (t > 5000) return BWAPI::Positions::Invalid;

    return {target->prevPos.x + (int) (t * target->rawUnit->getVelocityX()),
            target->prevPos.y + (int) (t * target->rawUnit->getVelocityY())};
}
