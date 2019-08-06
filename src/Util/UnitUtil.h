#pragma once

#include "Common.h"

namespace UnitUtil
{
    bool IsUndetected(BWAPI::Unit unit);

    BWAPI::Position PredictPosition(BWAPI::Unit unit, int frames);

    bool Powers(BWAPI::TilePosition pylonTile, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType);

    int BuildTime(BWAPI::UnitType type);

    bool IsInWeaponRange(BWAPI::Unit attacker, BWAPI::Unit target);

    bool CanAttack(BWAPI::Unit attacker, BWAPI::Unit target);
    bool CanAttackAir(BWAPI::Unit attacker);
    bool CanAttackGround(BWAPI::Unit attacker);
    bool IsRangedUnit(BWAPI::UnitType type);
    bool IsCombatUnit(BWAPI::UnitType type);

    BWAPI::WeaponType GetWeapon(BWAPI::UnitType attacker, BWAPI::Unit target);
}