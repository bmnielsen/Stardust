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
}