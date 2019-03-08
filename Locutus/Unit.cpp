#include "Unit.h"

#include "UnitUtil.h"
#include "Geo.h"
#include "Players.h"

Unit::Unit(BWAPI::Unit unit)
    : unit(unit)
    , player(unit->getPlayer())
    , tilePositionX(unit->getPosition().x << 3)
    , tilePositionY(unit->getPosition().y << 3)
    , lastSeen(BWAPI::Broodwar->getFrameCount())
    , type(unit->getType())
    , id(unit->getID())
    , lastPosition(unit->getPosition())
    , lastPositionValid(true)
    , lastHealth(unit->getHitPoints())
    , lastShields(unit->getShields())
    , completed(unit->isCompleted())
    , estimatedCompletionFrame(-1)
    , isFlying(unit->isFlying())
    , groundCooldownUntil(BWAPI::Broodwar->getFrameCount() + unit->getGroundWeaponCooldown())
    , airCooldownUntil(BWAPI::Broodwar->getFrameCount() + unit->getAirWeaponCooldown())
    , undetected(UnitUtil::IsUndetected(unit))
    , doomed(false)
{
    update(unit);
}

void Unit::update(BWAPI::Unit unit) const
{
    if (!unit || !unit->exists()) return;

    lastSeen = BWAPI::Broodwar->getFrameCount();
    lastPosition = unit->getPosition();
    lastPositionValid = true;

    // Cloaked units show up with 0 hit points and shields, so default to max and otherwise don't touch them
    undetected = UnitUtil::IsUndetected(unit);
    if (!undetected)
    {
        lastHealth = unit->getHitPoints();
        lastShields = unit->getShields();
    }
    else if (lastHealth == 0)
    {
        lastHealth = unit->getType().maxHitPoints();
        lastShields = unit->getType().maxShields();
    }

    completed = unit->isCompleted();
    computeCompletionFrame(unit);

    isFlying = unit->isFlying();

    if (unit->isVisible())
    {
        groundCooldownUntil = BWAPI::Broodwar->getFrameCount() + unit->getGroundWeaponCooldown();
        airCooldownUntil = BWAPI::Broodwar->getFrameCount() + unit->getAirWeaponCooldown();
    }

    // Process upcoming attacks
    int upcomingDamage = 0;
    for (auto it = upcomingAttacks.begin(); it != upcomingAttacks.end(); )
    {
        // Remove bullets when they have hit
        if (it->bullet && (!it->bullet->exists() || it->bullet->getID() != it->bulletId 
                || it->bullet->getPosition().getApproxDistance(it->bullet->getTargetPosition()) == 0))
        {
            it = upcomingAttacks.erase(it);
            continue;
        }

        // Clear if the attacker is dead, no longer visible, or out of range
        else if (!it->attacker || !it->attacker->exists() || !it->attacker->isVisible() || !UnitUtil::IsInWeaponRange(it->attacker, unit))
        {
            it = upcomingAttacks.erase(it);
            continue;
        }

        // Clear when the attacker has finished making an attack
        else if ((isFlying ? it->attacker->getAirWeaponCooldown() : it->attacker->getGroundWeaponCooldown()) > BWAPI::Broodwar->getRemainingLatencyFrames()
            && !it->attacker->isAttackFrame())
        {
            it = upcomingAttacks.erase(it);
            continue;
        }

        upcomingDamage += it->damage;
        it++;
    }

    doomed = (upcomingDamage >= (lastHealth + lastShields));
}

void Unit::updateLastPositionValidity() const
{
    // Skip if not applicable
    if (!lastPositionValid ||
        !unit ||
        unit->isVisible()) return;

    // If the last position is now visible, the unit is gone
    if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(lastPosition)))
    {
        lastPositionValid = false;
    }

    // If the unit is a sieged tank, assume it is gone from its last position
    // if we haven't seen it in 10 seconds and have a unit that it would otherwise
    // fire upon
    else if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode &&
        lastSeen < (BWAPI::Broodwar->getFrameCount() - 240))
    {
        // Look for a unit inside the tank's sight range
        // TODO: Could also look for a unit inside the tank's weapon range that the enemy can see
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
            if (Geo::EdgeToEdgeDistance(
                unit->getType(),
                unit->getPosition(),
                BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode,
                lastPosition) <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.sightRange())
            {
                lastPositionValid = false;
                break;
            }
    }
}

void Unit::addUpcomingAttack(BWAPI::Unit attacker, BWAPI::Bullet bullet) const
{
    // Remove any existing upcoming attack from this attacker
    for (auto it = upcomingAttacks.begin(); it != upcomingAttacks.end(); )
    {
        if (it->attacker == attacker)
            it = upcomingAttacks.erase(it);
        else
            it++;
    }

    upcomingAttacks.emplace_back(attacker, bullet, Players::attackDamage(bullet->getSource()->getPlayer(), bullet->getSource()->getType(), player, type));
}

void Unit::computeCompletionFrame(BWAPI::Unit unit) const
{
    if (!unit->getType().isBuilding() || unit->isCompleted())
        estimatedCompletionFrame = -1;

    int remainingHitPoints = unit->getType().maxHitPoints() - unit->getHitPoints();
    double hitPointsPerFrame = (unit->getType().maxHitPoints() * 0.9) / unit->getType().buildTime();
    estimatedCompletionFrame = BWAPI::Broodwar->getFrameCount() + (int)(remainingHitPoints / hitPointsPerFrame);
}
