#pragma once

#include "Common.h"

class Grid;
class UpgradeTracker
{
public:
    UpgradeTracker(BWAPI::Player player) : player(player) {}

    // Updates the items that have been queried previously
    void update(Grid & grid);

    int weaponDamage(BWAPI::WeaponType wpn);
    int weaponRange(BWAPI::WeaponType wpn);
    int unitCooldown(BWAPI::UnitType type);
    double unitTopSpeed(BWAPI::UnitType type);
    int unitArmor(BWAPI::UnitType type);

private:
    BWAPI::Player player;

    std::map<BWAPI::WeaponType, int>    _weaponDamage;
    std::map<BWAPI::WeaponType, int>    _weaponRange;
    std::map<BWAPI::UnitType, int>      _unitCooldown;
    std::map<BWAPI::UnitType, double>   _unitTopSpeed;
    std::map<BWAPI::UnitType, int>      _unitArmor;
};