#pragma once

#include "Common.h"

class Grid;

class UpgradeTracker
{
public:
    UpgradeTracker (const UpgradeTracker&) = delete;
    UpgradeTracker &operator=(const UpgradeTracker&) = delete;

    explicit UpgradeTracker(BWAPI::Player player) : player(player) {}

    // Updates the items that have been queried previously
    void update(Grid &grid);

    int weaponDamage(BWAPI::WeaponType wpn);

    int weaponRange(BWAPI::WeaponType wpn);

    int unitGroundCooldown(BWAPI::UnitType type);

    int unitAirCooldown(BWAPI::UnitType type);

    double unitTopSpeed(BWAPI::UnitType type);

    int unitBWTopSpeed(BWAPI::UnitType type);

    int unitArmor(BWAPI::UnitType type);

    int unitSightRange(BWAPI::UnitType type);

    bool hasResearched(BWAPI::TechType type);

    void setHasResearched(BWAPI::TechType type);

    int upgradeLevel(BWAPI::UpgradeType type);

    void setWeaponRange(BWAPI::WeaponType wpn, int range, Grid &grid);

private:
    BWAPI::Player player;

    std::map<BWAPI::WeaponType, int> _weaponDamage;
    std::map<BWAPI::WeaponType, int> _weaponRange;
    std::map<BWAPI::UnitType, int> _unitGroundCooldown;
    std::map<BWAPI::UnitType, int> _unitAirCooldown;
    std::map<BWAPI::UnitType, double> _unitTopSpeed;
    std::map<BWAPI::UnitType, int> _unitBWTopSpeed;
    std::map<BWAPI::UnitType, int> _unitArmor;
    std::map<BWAPI::UnitType, int> _unitSightRange;

    std::map<BWAPI::TechType, bool> _hasResearched;
    std::map<BWAPI::UpgradeType, int> _upgradeLevel;
};
