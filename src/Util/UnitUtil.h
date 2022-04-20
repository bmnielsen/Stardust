#pragma once

#include "Common.h"

namespace UnitUtil
{
    bool Powers(BWAPI::TilePosition pylonTile, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType);

    int BuildTime(BWAPI::UnitType type);

    BWAPI::WeaponType GetGroundWeapon(BWAPI::UnitType attacker);

    BWAPI::WeaponType GetAirWeapon(BWAPI::UnitType attacker);

    bool CanAttackGround(BWAPI::UnitType attackerType);

    bool CanAttackAir(BWAPI::UnitType attackerType);

    bool IsRangedUnit(BWAPI::UnitType type);

    bool IsCombatUnit(BWAPI::UnitType type);

    // Whether the type typically remains in the same position as long as it can attack
    // Beyond static defense buildings, this includes sieged tanks and lurkers
    bool IsStationaryAttacker(BWAPI::UnitType type);

    std::pair<BWAPI::UnitType, int> MorphsFrom(BWAPI::UnitType type);

    int MineralCost(BWAPI::UnitType type);

    int GasCost(BWAPI::UnitType type);

    int GroundWeaponAngle(BWAPI::UnitType type);

    int AirWeaponAngle(BWAPI::UnitType type);
}