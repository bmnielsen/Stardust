#pragma once

#include "Common.h"

#include "Grid.h"

namespace Players
{
    void initialize();

    void update();

    Grid &grid(BWAPI::Player player);

    int weaponDamage(BWAPI::Player player, BWAPI::WeaponType wpn);

    int weaponRange(BWAPI::Player player, BWAPI::WeaponType wpn);

    int unitCooldown(BWAPI::Player player, BWAPI::UnitType type);

    double unitTopSpeed(BWAPI::Player player, BWAPI::UnitType type);

    int unitArmor(BWAPI::Player player, BWAPI::UnitType type);

    int unitSightRange(BWAPI::Player player, BWAPI::UnitType type);

    int attackDamage(BWAPI::Player firstPlayer, BWAPI::UnitType firstUnit, BWAPI::Player secondPlayer, BWAPI::UnitType secondUnit);

    bool hasResearched(BWAPI::Player player, BWAPI::TechType type);

    int upgradeLevel(BWAPI::Player player, BWAPI::UpgradeType type);

    void setWeaponRange(BWAPI::Player player, BWAPI::WeaponType wpn, int range);
}
