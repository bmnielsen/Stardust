#include "UnitUtil.h"

#include "Geo.h"
#include "Players.h"

namespace UnitUtil
{
    namespace {
        const int WARP_IN_FRAMES = 71;
    }

    bool IsUndetected(BWAPI::Unit unit)
    {
        return (unit->isBurrowed() || unit->isCloaked() || unit->getType().hasPermanentCloak()) && !unit->isDetected();
    }

    BWAPI::Position PredictPosition(BWAPI::Unit unit, int frames)
    {
        if (!unit || !unit->exists() || !unit->isVisible()) return BWAPI::Positions::Invalid;

        return unit->getPosition() + BWAPI::Position((int)(frames * unit->getVelocityX()), (int)(frames * unit->getVelocityY()));
    }

    bool Powers(BWAPI::TilePosition pylonTile, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType)
    {
        int offsetY = buildingTile.y - pylonTile.y;
        int offsetX = buildingTile.x - pylonTile.x;

        if (buildingType.tileWidth() == 4)
        {
            if (offsetY < -5 || offsetY > 4) return false;
            if ((offsetY == -5 || offsetY == 4) && (offsetX < -4 || offsetX > 1)) return false;
            if ((offsetY == -4 || offsetY == 3) && (offsetX < -7 || offsetX > 4)) return false;
            if ((offsetY == -3 || offsetY == 2) && (offsetX < -8 || offsetX > 5)) return false;
            return (offsetX >= -8 && offsetX <= 6);
        }

        if (offsetY < -4 || offsetY > 4) return false;
        if (offsetY == 4 && (offsetX < -3 || offsetX > 2)) return false;
        if ((offsetY == -4 || offsetY == 3) && (offsetX < -6 || offsetX > 5)) return false;
        if ((offsetY == -3 || offsetY == 2) && (offsetX < -7 || offsetX > 6)) return false;
        return (offsetX >= -7 && offsetX <= 7);
    }

    int BuildTime(BWAPI::UnitType type)
    {
        // TODO: Technically zerg and terran also have an animation that needs to play after finished construction
        if (type.getRace() == BWAPI::Races::Protoss && type.isBuilding())
        {
            return type.buildTime() + WARP_IN_FRAMES;
        }

        return type.buildTime();
    }

    bool IsInWeaponRange(BWAPI::Unit attacker, BWAPI::Unit target)
    {
        int range = Players::weaponRange(attacker->getPlayer(), target->isFlying() ? attacker->getType().airWeapon() : attacker->getType().groundWeapon());
        int dist = Geo::EdgeToEdgeDistance(attacker->getType(), attacker->getPosition(), target->getType(), target->getPosition());

        return dist <= range;
    }

    bool CanAttack(BWAPI::Unit attacker, BWAPI::Unit target)
    {
        return target->isVisible() &&
            target->isDetected() &&
            !target->isStasised() &&
            (target->isFlying() ? CanAttackAir(attacker) : CanAttackGround(attacker));
    }

    bool CanAttackAir(BWAPI::Unit attacker)
    {
        return attacker->getType().airWeapon() != BWAPI::WeaponTypes::None ||
            attacker->getType() == BWAPI::UnitTypes::Protoss_Carrier ||
            attacker->getType() == BWAPI::UnitTypes::Terran_Bunker; // TODO: Track whether bunkers actually have units in them
    }

    bool CanAttackGround(BWAPI::Unit attacker)
    {
        return attacker->getType().groundWeapon() != BWAPI::WeaponTypes::None ||
            attacker->getType() == BWAPI::UnitTypes::Protoss_Carrier ||
            attacker->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
            attacker->getType() == BWAPI::UnitTypes::Terran_Bunker; // TODO: Track whether bunkers actually have units in them
    }

    bool IsRangedUnit(BWAPI::UnitType type)
    {
        return
            type.groundWeapon().maxRange() > 32 ||
            type.isFlyer() ||
            type == BWAPI::UnitTypes::Protoss_Reaver;
    }

    bool IsCombatUnit(BWAPI::UnitType type)
    {
        if (type.isWorker() || type.isBuilding()) return false;

        return
                type.canAttack() ||
                type.isDetector() ||
                type == BWAPI::UnitTypes::Zerg_Queen ||
                type == BWAPI::UnitTypes::Zerg_Defiler ||
                type == BWAPI::UnitTypes::Terran_Medic ||
                type == BWAPI::UnitTypes::Protoss_High_Templar ||
                type == BWAPI::UnitTypes::Protoss_Dark_Archon ||
                (type.isFlyer() && type.spaceProvided() > 0);
    }

    BWAPI::WeaponType GetWeapon(BWAPI::UnitType attacker, BWAPI::Unit target)
    {
        // We pretend that a bunker has marines in it. It's only a guess.
        if (attacker == BWAPI::UnitTypes::Terran_Bunker)
        {
            return GetWeapon(BWAPI::UnitTypes::Terran_Marine, target);
        }
        if (attacker == BWAPI::UnitTypes::Protoss_Carrier)
        {
            return GetWeapon(BWAPI::UnitTypes::Protoss_Interceptor, target);
        }
        if (attacker == BWAPI::UnitTypes::Protoss_Reaver)
        {
            return GetWeapon(BWAPI::UnitTypes::Protoss_Scarab, target);
        }
        return target->isFlying() ? attacker.airWeapon() : attacker.groundWeapon();

    }
}
