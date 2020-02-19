#include "UnitUtil.h"


namespace UnitUtil
{
    namespace
    {
        const int WARP_IN_FRAMES = 71;
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

    BWAPI::WeaponType GetGroundWeapon(BWAPI::UnitType attacker)
    {
        // Assume bunkers have marines in them
        if (attacker == BWAPI::UnitTypes::Terran_Bunker)
        {
            return GetGroundWeapon(BWAPI::UnitTypes::Terran_Marine);
        }

        if (attacker == BWAPI::UnitTypes::Protoss_Carrier)
        {
            return GetGroundWeapon(BWAPI::UnitTypes::Protoss_Interceptor);
        }

        if (attacker == BWAPI::UnitTypes::Protoss_Reaver)
        {
            return GetGroundWeapon(BWAPI::UnitTypes::Protoss_Scarab);
        }

        return attacker.groundWeapon();
    }

    BWAPI::WeaponType GetAirWeapon(BWAPI::UnitType attacker)
    {
        // Assume bunkers have marines in them
        if (attacker == BWAPI::UnitTypes::Terran_Bunker)
        {
            return GetAirWeapon(BWAPI::UnitTypes::Terran_Marine);
        }

        if (attacker == BWAPI::UnitTypes::Protoss_Carrier)
        {
            return GetAirWeapon(BWAPI::UnitTypes::Protoss_Interceptor);
        }

        return attacker.airWeapon();
    }

    bool CanAttackGround(BWAPI::UnitType attackerType)
    {
        return attackerType.groundWeapon() != BWAPI::WeaponTypes::None ||
               attackerType == BWAPI::UnitTypes::Protoss_Carrier ||
               attackerType == BWAPI::UnitTypes::Protoss_Reaver ||
               attackerType == BWAPI::UnitTypes::Terran_Bunker;
    }

    bool CanAttackAir(BWAPI::UnitType attackerType)
    {
        return attackerType.airWeapon() != BWAPI::WeaponTypes::None ||
               attackerType == BWAPI::UnitTypes::Protoss_Carrier ||
               attackerType == BWAPI::UnitTypes::Terran_Bunker;
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
}
