#include "Players.h"

namespace Players
{
#ifndef _DEBUG
    namespace
    {
#endif

        struct UpgradeTracker
        {
            std::map<BWAPI::WeaponType, int>    weaponDamage;
            std::map<BWAPI::WeaponType, int>    weaponRange;
            std::map<BWAPI::UnitType, int>      unitCooldown;
            std::map<BWAPI::UnitType, double>   unitTopSpeed;
            std::map<BWAPI::UnitType, int>      unitArmor;

            // Updates the items that have been queried previously
            void update(BWAPI::Player player)
            {
                for (auto& weaponAndDamage : weaponDamage)
                {
                    int current = player->damage(weaponAndDamage.first);
                    if (current > weaponAndDamage.second)
                    {
                        weaponAndDamage.second = current;
                    }
                }

                for (auto& weaponAndRange : weaponRange)
                {
                    int current = player->weaponMaxRange(weaponAndRange.first);
                    if (current > weaponAndRange.second)
                    {
                        weaponAndRange.second = current;
                    }
                }

                for (auto& unitAndCooldown : unitCooldown)
                {
                    int current = player->weaponDamageCooldown(unitAndCooldown.first);
                    if (current > unitAndCooldown.second)
                    {
                        unitAndCooldown.second = current;
                    }
                }

                for (auto& unitAndTopSpeed : unitTopSpeed)
                {
                    int current = player->topSpeed(unitAndTopSpeed.first);
                    if (current > unitAndTopSpeed.second)
                    {
                        unitAndTopSpeed.second = current;
                    }
                }

                for (auto& unitAndArmor : unitArmor)
                {
                    int current = player->armor(unitAndArmor.first);
                    if (current > unitAndArmor.second)
                    {
                        unitAndArmor.second = current;
                    }
                }
            }
        };

        std::map<BWAPI::Player, UpgradeTracker> playerToUpgradeTracker;

#ifndef _DEBUG
    }
#endif

    void update()
    {
        for (auto & playerAndUpgradeTracker : playerToUpgradeTracker)
        {
            playerAndUpgradeTracker.second.update(playerAndUpgradeTracker.first);
        }
    }

    int weaponDamage(BWAPI::Player player, BWAPI::WeaponType wpn)
    {
        auto & upgradeTracker = playerToUpgradeTracker[player];

        auto weaponDamageIt = upgradeTracker.weaponDamage.find(wpn);
        if (weaponDamageIt != upgradeTracker.weaponDamage.end())
        {
            return weaponDamageIt->second;
        }

        int current = player->damage(wpn);
        upgradeTracker.weaponDamage[wpn] = current;
        return current;
    }

    int weaponRange(BWAPI::Player player, BWAPI::WeaponType wpn)
    {
        auto & upgradeTracker = playerToUpgradeTracker[player];

        auto weaponRangeIt = upgradeTracker.weaponRange.find(wpn);
        if (weaponRangeIt != upgradeTracker.weaponRange.end())
        {
            return weaponRangeIt->second;
        }

        int current = player->weaponMaxRange(wpn);
        upgradeTracker.weaponRange[wpn] = current;
        return current;

    }

    int unitCooldown(BWAPI::Player player, BWAPI::UnitType type)
    {
        auto & upgradeTracker = playerToUpgradeTracker[player];

        auto unitCooldownIt = upgradeTracker.unitCooldown.find(type);
        if (unitCooldownIt != upgradeTracker.unitCooldown.end())
        {
            return unitCooldownIt->second;
        }

        int current = player->weaponDamageCooldown(type);
        upgradeTracker.unitCooldown[type] = current;
        return current;
    }
    
    double unitTopSpeed(BWAPI::Player player, BWAPI::UnitType type)
    {
        auto & upgradeTracker = playerToUpgradeTracker[player];

        auto unitTopSpeedIt = upgradeTracker.unitTopSpeed.find(type);
        if (unitTopSpeedIt != upgradeTracker.unitTopSpeed.end())
        {
            return unitTopSpeedIt->second;
        }

        int current = player->topSpeed(type);
        upgradeTracker.unitTopSpeed[type] = current;
        return current;
    }
    
    int unitArmor(BWAPI::Player player, BWAPI::UnitType type)
    {
        auto & upgradeTracker = playerToUpgradeTracker[player];

        auto unitArmorIt = upgradeTracker.unitArmor.find(type);
        if (unitArmorIt != upgradeTracker.unitArmor.end())
        {
            return unitArmorIt->second;
        }

        int current = player->armor(type);
        upgradeTracker.unitArmor[type] = current;
        return current;
    }

    int attackDamage(BWAPI::Player attackingPlayer, BWAPI::UnitType attackingUnit, BWAPI::Player targetPlayer, BWAPI::UnitType targetUnit)
    {
        auto weapon = targetUnit.isFlyer() ? attackingUnit.airWeapon() : attackingUnit.groundWeapon();
        if (weapon == BWAPI::WeaponTypes::None) return 0;

        int damage = (weaponDamage(attackingPlayer, weapon) - unitArmor(targetPlayer, targetUnit)) * weapon.damageFactor();

        if (weapon.damageType() == BWAPI::DamageTypes::Concussive)
        {
            if (targetUnit.size() == BWAPI::UnitSizeTypes::Large)
            {
                damage /= 4;
            }
            else if (targetUnit.size() == BWAPI::UnitSizeTypes::Medium)
            {
                damage /= 2;
            }
        }
        else if (weapon.damageType() == BWAPI::DamageTypes::Explosive)
        {
            if (targetUnit.size() == BWAPI::UnitSizeTypes::Small)
            {
                damage /= 2;
            }
            else if (targetUnit.size() == BWAPI::UnitSizeTypes::Medium)
            {
                damage = (damage * 3) / 4;
            }
        }

        return std::max(128, damage);
    }
}
