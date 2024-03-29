#include "UnitUtil.h"

#include <array>

namespace UnitUtil
{
    namespace
    {
        const int WARP_IN_FRAMES = 71;

        // Weapon angles indexed by weapon ID
        const std::array<int, 134> weaponAngles = {
                16, 16, 16, 16, 64, 64, 128, 16, 32, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
                16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 128, 128, 128, 128, 16, 16, 16, 16, 16, 16, 16, 16, 16,
                16, 128, 128, 16, 16, 16, 16, 16, 16, 32, 16, 32, 16, 16, 16, 128, 128, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 128, 128, 96, 64, 16,
                32, 32, 32, 16, 16, 16, 32, 16, 16, 16, 16, 16, 0, 16, 16, 16, 16, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
        };

        // Halt distances indexed by unit ID
        // Some are adjusted slightly based on observed behaviour
        const std::array<int, 234> haltDistances = {
                0, 0, 57, 0, 0, 0, 0, 48, 85, 20,
                0, 147, 30, 0, 4309, 0, 0, 0, 0, 57,
                0, 85, 20, 0, 0, 0, 0, 30, 30, 30,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 48, 3, 85, 30, 85, 0, 53, 0, 85,
                0, 0, 0, 0, 0, 85, 30, 3, 86, 0,
                67+64, 53, 67, 20, 48, 0, 0, 53, 20, 147,
                67, 97, 53, 53, 53, 53, 20, 0, 0, 53,
                67, 0, 53, 0, 53, 0, 97, 53, 67, 0,
                0, 0, 0, 0, 200, 0, 0, 0, 67, 0,
                0, 0, 30, 0, 0, 0, 11, 0, 0, 0,
                11, 11, 0, 11, 11, 0, 11, 0, 0, 0,
                0, 0, 11, 0, 0, 11, 0, 0, 0, 0,
                11, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0
        };
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
        if (buildingType.tileWidth() == 2)
        {
            return (offsetX >= -7 && offsetX <= 7);
        }
        return (offsetX >= -8 && offsetX <= 7);
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
               attackerType == BWAPI::UnitTypes::Terran_Science_Vessel ||
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
        if (IsStationaryAttacker(type)) return true; // So we include static defense
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

    bool IsStationaryAttacker(BWAPI::UnitType type)
    {
        return type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
               type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
               type == BWAPI::UnitTypes::Zerg_Spore_Colony ||
               type == BWAPI::UnitTypes::Zerg_Lurker ||
               type == BWAPI::UnitTypes::Terran_Missile_Turret ||
               type == BWAPI::UnitTypes::Terran_Bunker ||
               type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
    }

    std::pair<BWAPI::UnitType, int> MorphsFrom(BWAPI::UnitType type)
    {
        // Anything built by a drone is a morph
        if (type.whatBuilds().first == BWAPI::UnitTypes::Zerg_Drone) return type.whatBuilds();

        // A building "built" by another building is a morph, unless it is an add-on
        if (type.isBuilding() && !type.isAddon() && type.whatBuilds().first.isBuilding()) return type.whatBuilds();

        // A unit "built" by another unit is a morph unless it is an interceptor or scarab
        if (!type.isBuilding() && !type.whatBuilds().first.isBuilding() &&
            type != BWAPI::UnitTypes::Protoss_Interceptor && type != BWAPI::UnitTypes::Protoss_Scarab)
        {
            return type.whatBuilds();
        }

        return std::make_pair(BWAPI::UnitTypes::None, 0);
    }

    int MineralCost(BWAPI::UnitType type)
    {
        if (type == BWAPI::UnitTypes::None || type == BWAPI::UnitTypes::Unknown) return 0;

        int minerals = type.mineralPrice();

        // BWAPI lists some units as having mineral cost 1 instead of 0, e.g. Larva
        if (minerals == 1) minerals = 0;

        if (type.isTwoUnitsInOneEgg()) minerals /= 2;

        auto morphsFrom = MorphsFrom(type);
        if (morphsFrom.second > 0) minerals += morphsFrom.second * MineralCost(morphsFrom.first);

        return minerals;
    }

    int GasCost(BWAPI::UnitType type)
    {
        if (type == BWAPI::UnitTypes::None || type == BWAPI::UnitTypes::Unknown || type == BWAPI::UnitTypes::Zerg_Larva) return 0;

        int gas = type.gasPrice();

        // BWAPI lists some units as having gas cost 1 instead of 0, e.g. Larva
        if (gas == 1) gas = 0;

        if (type.isTwoUnitsInOneEgg()) gas /= 2;

        auto morphsFrom = MorphsFrom(type);
        if (morphsFrom.second > 0) gas += morphsFrom.second * GasCost(morphsFrom.first);

        return gas;
    }

    int GroundWeaponAngle(BWAPI::UnitType type)
    {
        if (type.groundWeapon().getID() >= weaponAngles.size()) return 0;
        return weaponAngles[type.groundWeapon().getID()];
    }

    int AirWeaponAngle(BWAPI::UnitType type)
    {
        if (type.airWeapon().getID() >= weaponAngles.size()) return 0;
        return weaponAngles[type.airWeapon().getID()];
    }

    int HaltDistance(BWAPI::UnitType type)
    {
        if (type.getID() >= haltDistances.size()) return 0;
        return haltDistances[type.getID()];
    }

    int Acceleration(BWAPI::UnitType type, double currentTopSpeed)
    {
        // Acceleration is doubled when speed is upgraded
        if (currentTopSpeed > type.topSpeed())
        {
            return type.acceleration() << 1;
        }
        return type.acceleration();
    }
}
