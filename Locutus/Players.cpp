#include "Players.h"

#include "UpgradeTracker.h"

namespace Players
{
#ifndef _DEBUG
    namespace
    {
#endif

        std::map<BWAPI::Player, std::shared_ptr<UpgradeTracker>> playerToUpgradeTracker;
        std::map<BWAPI::Player, Grid> playerToGrid;

        std::shared_ptr<UpgradeTracker> getUpgradeTracker(BWAPI::Player player)
        {
            auto it = playerToUpgradeTracker.find(player);
            if (it != playerToUpgradeTracker.end())
            {
                return it->second;
            }

            auto upgradeTracker = std::make_shared<UpgradeTracker>(player);
            playerToUpgradeTracker.try_emplace(player, upgradeTracker);
            return upgradeTracker;
        }

#ifndef _DEBUG
    }
#endif

    void update()
    {
        for (auto & playerAndUpgradeTracker : playerToUpgradeTracker)
        {
            playerAndUpgradeTracker.second->update(grid(playerAndUpgradeTracker.first));
        }
    }

    Grid & grid(BWAPI::Player player)
    {
        auto it = playerToGrid.find(player);
        if (it != playerToGrid.end()) return it->second;

        auto result = playerToGrid.try_emplace(player, getUpgradeTracker(player));
        return result.first->second;
    }

    int weaponDamage(BWAPI::Player player, BWAPI::WeaponType wpn)
    {
        return playerToUpgradeTracker[player]->weaponDamage(wpn);
    }

    int weaponRange(BWAPI::Player player, BWAPI::WeaponType wpn)
    {
        return playerToUpgradeTracker[player]->weaponRange(wpn);
    }

    int unitCooldown(BWAPI::Player player, BWAPI::UnitType type)
    {
        return playerToUpgradeTracker[player]->unitCooldown(type);
    }
    
    double unitTopSpeed(BWAPI::Player player, BWAPI::UnitType type)
    {
        return playerToUpgradeTracker[player]->unitCooldown(type);
    }
    
    int unitArmor(BWAPI::Player player, BWAPI::UnitType type)
    {
        return playerToUpgradeTracker[player]->unitArmor(type);
    }

    int attackDamage(BWAPI::Player attackingPlayer, BWAPI::UnitType attackingUnit, BWAPI::Player targetPlayer, BWAPI::UnitType targetUnit)
    {
        auto weapon = targetUnit.isFlyer() ? attackingUnit.airWeapon() : attackingUnit.groundWeapon();
        if (weapon == BWAPI::WeaponTypes::None) return 0;

        int damage = (weaponDamage(attackingPlayer, weapon) - unitArmor(targetPlayer, targetUnit)) 
            * (targetUnit.isFlyer() ? attackingUnit.maxAirHits() : attackingUnit.maxGroundHits())
            * weapon.damageFactor();

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
