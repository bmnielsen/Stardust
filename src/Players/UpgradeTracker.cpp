#include "UpgradeTracker.h"

#include "Units.h"
#include "Grid.h"

void UpgradeTracker::update(Grid &grid)
{
    for (auto &weaponAndDamage : _weaponDamage)
    {
        int current = player->damage(weaponAndDamage.first);
        if (current > weaponAndDamage.second)
        {
            // Update the grid for all known units with this weapon type
            auto updateGrid = [&](const Unit &unit)
            {
                auto weaponUnitType = unit->type;
                if (unit->type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;
                if (unit->type == BWAPI::UnitTypes::Protoss_Carrier) weaponUnitType = BWAPI::UnitTypes::Protoss_Interceptor;

                if (unit->lastPositionValid && !unit->beingManufacturedOrCarried && !unit->immobile &&
                    (weaponUnitType.groundWeapon() == weaponAndDamage.first ||
                     weaponUnitType.airWeapon() == weaponAndDamage.first))
                {
                    grid.unitWeaponDamageUpgraded(unit->type, unit->lastPosition, weaponAndDamage.first, weaponAndDamage.second, current);

#if DEBUG_GRID_UPDATES
                    CherryVis::log(unit->id) << "Grid::weaponDamageUpgraded from " << weaponAndDamage.second << " to " << current;
                    Log::Debug() << *unit << ": Grid::weaponDamageUpgraded from " << weaponAndDamage.second << " to " << current;
#endif
                }
            };
            if (player == BWAPI::Broodwar->self())
            {
                for (auto &unit : Units::allMine())
                { updateGrid(unit); }
            }
            else
            {
                for (auto &unit : Units::allEnemy())
                { updateGrid(unit); }
            }

            weaponAndDamage.second = current;
        }
    }

    for (auto &weaponAndRange : _weaponRange)
    {
        int current = player->weaponMaxRange(weaponAndRange.first);
        if (current > weaponAndRange.second)
        {
            // Update the grid for all known units with this weapon type
            auto updateGrid = [&](const Unit &unit)
            {
                auto weaponUnitType = unit->type;
                if (unit->type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;

                if (unit->lastPositionValid && !unit->beingManufacturedOrCarried && !unit->immobile &&
                    (weaponUnitType.groundWeapon() == weaponAndRange.first ||
                     weaponUnitType.airWeapon() == weaponAndRange.first))
                {
                    grid.unitWeaponRangeUpgraded(unit->type, unit->lastPosition, weaponAndRange.first, weaponAndRange.second, current);

#if DEBUG_GRID_UPDATES
                    CherryVis::log(unit->id) << "Grid::weaponRangeUpgraded from " << weaponAndRange.second << " to " << current;
                    Log::Debug() << *unit << ": Grid::weaponRangeUpgraded from " << weaponAndRange.second << " to " << current;
#endif
                }
            };
            if (player == BWAPI::Broodwar->self())
            {
                for (auto &unit : Units::allMine())
                { updateGrid(unit); }
            }
            else
            {
                for (auto &unit : Units::allEnemy())
                { updateGrid(unit); }
            }

            weaponAndRange.second = current;
        }
    }

    for (auto &unitAndCooldown : _unitCooldown)
    {
        int current = player->weaponDamageCooldown(unitAndCooldown.first);
        if (current > unitAndCooldown.second)
        {
            unitAndCooldown.second = current;
        }
    }

    for (auto &unitAndTopSpeed : _unitTopSpeed)
    {
        double current = player->topSpeed(unitAndTopSpeed.first);
        if (current > unitAndTopSpeed.second)
        {
            unitAndTopSpeed.second = current;
        }
    }

    for (auto &unitAndArmor : _unitArmor)
    {
        int current = player->armor(unitAndArmor.first);
        if (current > unitAndArmor.second)
        {
            unitAndArmor.second = current;
        }
    }

    for (auto &unitAndSightRange : _unitSightRange)
    {
        int current = player->sightRange(unitAndSightRange.first);
        if (current > unitAndSightRange.second)
        {
            // Update the grid for all known units with this type
            auto updateGrid = [&](const Unit &unit)
            {
                if (unit->lastPositionValid && !unit->beingManufacturedOrCarried && !unit->immobile && unit->type == unitAndSightRange.first)
                {
                    grid.unitSightRangeUpgraded(unit->type, unit->lastPosition, unitAndSightRange.second, current);

#if DEBUG_GRID_UPDATES
                    CherryVis::log(unit->id) << "Grid::sightRangeUpgraded from " << unitAndSightRange.second << " to " << current;
                    Log::Debug() << *unit << ": Grid::sightRangeUpgraded from " << unitAndSightRange.second << " to " << current;
#endif
                }
            };
            if (player == BWAPI::Broodwar->self())
            {
                for (auto &unit : Units::allMine())
                { updateGrid(unit); }
            }
            else
            {
                for (auto &unit : Units::allEnemy())
                { updateGrid(unit); }
            }

            unitAndSightRange.second = current;
        }
    }

    for (auto &techAndResearched : _hasResearched)
    {
        bool current = player->hasResearched(techAndResearched.first);
        if (current && !techAndResearched.second)
        {
            techAndResearched.second = current;
        }
    }

    for (auto &upgradeAndLevel : _upgradeLevel)
    {
        bool current = player->getUpgradeLevel(upgradeAndLevel.first);
        if (current && current > upgradeAndLevel.second)
        {
            upgradeAndLevel.second = current;
        }
    }
}

int UpgradeTracker::weaponDamage(BWAPI::WeaponType wpn)
{
    auto weaponDamageIt = _weaponDamage.find(wpn);
    if (weaponDamageIt != _weaponDamage.end())
    {
        return weaponDamageIt->second;
    }

    int current = player->damage(wpn);
    _weaponDamage[wpn] = current;
    return current;
}

int UpgradeTracker::weaponRange(BWAPI::WeaponType wpn)
{
    // For interceptors and scarabs, return the range of the carrier and reaver
    if (wpn == BWAPI::WeaponTypes::Pulse_Cannon || wpn == BWAPI::WeaponTypes::Scarab) return 256;

    auto weaponRangeIt = _weaponRange.find(wpn);
    if (weaponRangeIt != _weaponRange.end())
    {
        return weaponRangeIt->second;
    }

    int current = player->weaponMaxRange(wpn);
    _weaponRange[wpn] = current;
    return current;

}

int UpgradeTracker::unitCooldown(BWAPI::UnitType type)
{
    auto unitCooldownIt = _unitCooldown.find(type);
    if (unitCooldownIt != _unitCooldown.end())
    {
        return unitCooldownIt->second;
    }

    int current = player->weaponDamageCooldown(type);
    _unitCooldown[type] = current;
    return current;
}

double UpgradeTracker::unitTopSpeed(BWAPI::UnitType type)
{
    auto unitTopSpeedIt = _unitTopSpeed.find(type);
    if (unitTopSpeedIt != _unitTopSpeed.end())
    {
        return unitTopSpeedIt->second;
    }

    double current = player->topSpeed(type);
    _unitTopSpeed[type] = current;
    return current;
}

int UpgradeTracker::unitArmor(BWAPI::UnitType type)
{
    auto unitArmorIt = _unitArmor.find(type);
    if (unitArmorIt != _unitArmor.end())
    {
        return unitArmorIt->second;
    }

    int current = player->armor(type);
    _unitArmor[type] = current;
    return current;
}

int UpgradeTracker::unitSightRange(BWAPI::UnitType type)
{
    auto unitSightRangeIt = _unitSightRange.find(type);
    if (unitSightRangeIt != _unitSightRange.end())
    {
        return unitSightRangeIt->second;
    }

    int current = player->sightRange(type);
    _unitSightRange[type] = current;
    return current;
}

bool UpgradeTracker::hasResearched(BWAPI::TechType type)
{
    auto hasResearchedIt = _hasResearched.find(type);
    if (hasResearchedIt != _hasResearched.end())
    {
        return hasResearchedIt->second;
    }

    bool current = player->hasResearched(type);
    _hasResearched[type] = current;
    return current;
}

int UpgradeTracker::upgradeLevel(BWAPI::UpgradeType type)
{
    auto upgradeLevelIt = _upgradeLevel.find(type);
    if (upgradeLevelIt != _upgradeLevel.end())
    {
        return upgradeLevelIt->second;
    }

    int current = player->getUpgradeLevel(type);
    _upgradeLevel[type] = current;
    return current;
}

void UpgradeTracker::setWeaponRange(BWAPI::WeaponType wpn, int range, Grid &grid)
{
    int current = player->weaponMaxRange(wpn);

    auto weaponAndRange = _weaponRange.find(wpn);
    if (weaponAndRange != _weaponRange.end())
    {
        current = weaponAndRange->second;
    }

    if (range <= current) return;

    // Update the grid for all known units with this weapon type
    auto updateGrid = [&](const Unit &unit)
    {
        auto weaponUnitType = unit->type;
        if (unit->type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;

        if (unit->lastPositionValid && !unit->beingManufacturedOrCarried && !unit->immobile &&
            (weaponUnitType.groundWeapon() == wpn ||
             weaponUnitType.airWeapon() == wpn))
        {
            grid.unitWeaponRangeUpgraded(unit->type, unit->lastPosition, wpn, current, range);

#if DEBUG_GRID_UPDATES
            CherryVis::log(unit->id) << "Grid::weaponRangeUpgraded from " << weaponAndRange.second << " to " << current;
                    Log::Debug() << *unit << ": Grid::weaponRangeUpgraded from " << weaponAndRange.second << " to " << current;
#endif
        }
    };
    if (player == BWAPI::Broodwar->self())
    {
        for (auto &unit : Units::allMine())
        { updateGrid(unit); }
    }
    else
    {
        for (auto &unit : Units::allEnemy())
        { updateGrid(unit); }
    }

    _weaponRange[wpn] = range;
}
