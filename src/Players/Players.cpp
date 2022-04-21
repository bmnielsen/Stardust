#include "Players.h"

#include "UpgradeTracker.h"

namespace Players
{
    namespace
    {
        std::map<BWAPI::Player, std::shared_ptr<UpgradeTracker>> playerToUpgradeTracker;
        std::map<BWAPI::Player, Grid> playerToGrid;

        std::shared_ptr<UpgradeTracker> &getUpgradeTracker(BWAPI::Player player)
        {
            auto it = playerToUpgradeTracker.find(player);
            if (it != playerToUpgradeTracker.end())
            {
                return it->second;
            }

            auto result = playerToUpgradeTracker.try_emplace(player, std::make_shared<UpgradeTracker>(player));
            return result.first->second;
        }
    }

    void initialize()
    {
        playerToUpgradeTracker.clear();
        playerToGrid.clear();
    }

    void update()
    {
        for (auto &playerAndUpgradeTracker : playerToUpgradeTracker)
        {
            playerAndUpgradeTracker.second->update(grid(playerAndUpgradeTracker.first));
        }
    }

    Grid &grid(BWAPI::Player player)
    {
        auto it = playerToGrid.find(player);
        if (it != playerToGrid.end()) return it->second;

        auto result = playerToGrid.try_emplace(player, getUpgradeTracker(player));
        return result.first->second;
    }

    int weaponDamage(BWAPI::Player player, BWAPI::WeaponType wpn)
    {
        return getUpgradeTracker(player)->weaponDamage(wpn);
    }

    int weaponRange(BWAPI::Player player, BWAPI::WeaponType wpn)
    {
        return getUpgradeTracker(player)->weaponRange(wpn);
    }

    int unitCooldown(BWAPI::Player player, BWAPI::UnitType type)
    {
        // Handle weird units that don't have proper cooldowns
        if (type == BWAPI::UnitTypes::Protoss_Scarab || type == BWAPI::UnitTypes::Protoss_Reaver)
        {
            return 60;
        }
        if (type == BWAPI::UnitTypes::Protoss_Interceptor || type == BWAPI::UnitTypes::Protoss_Carrier)
        {
            return 38;
        }

        return getUpgradeTracker(player)->unitCooldown(type);
    }

    double unitTopSpeed(BWAPI::Player player, BWAPI::UnitType type)
    {
        return getUpgradeTracker(player)->unitTopSpeed(type);
    }

    int unitArmor(BWAPI::Player player, BWAPI::UnitType type)
    {
        return getUpgradeTracker(player)->unitArmor(type);
    }

    int unitSightRange(BWAPI::Player player, BWAPI::UnitType type)
    {
        return getUpgradeTracker(player)->unitSightRange(type);
    }

    int attackDamage(BWAPI::Player attackingPlayer, BWAPI::UnitType attackingUnit, BWAPI::Player targetPlayer, BWAPI::UnitType targetUnit)
    {
        auto weapon = targetUnit.isFlyer() ? attackingUnit.airWeapon() : attackingUnit.groundWeapon();
        if (weapon == BWAPI::WeaponTypes::None) return 0;

        int damage = (weaponDamage(attackingPlayer, weapon) - unitArmor(targetPlayer, targetUnit))
                     * (targetUnit.isFlyer() ? attackingUnit.maxAirHits() : attackingUnit.maxGroundHits());

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

        return std::min(128, damage);
    }

    bool hasResearched(BWAPI::Player player, BWAPI::TechType type)
    {

        return getUpgradeTracker(player)->hasResearched(type);
    }

    void setHasResearched(BWAPI::Player player, BWAPI::TechType type)
    {
#if INSTRUMENTATION_ENABLED
        if (player == BWAPI::Broodwar->enemy() && !getUpgradeTracker(player)->hasResearched(type))
        {
            Log::Get() << "Enemy has researched " << type;
        }
#endif
        getUpgradeTracker(player)->setHasResearched(type);
    }

    int upgradeLevel(BWAPI::Player player, BWAPI::UpgradeType type)
    {
        return getUpgradeTracker(player)->upgradeLevel(type);
    }

    void setWeaponRange(BWAPI::Player player, BWAPI::WeaponType wpn, int range)
    {
        getUpgradeTracker(player)->setWeaponRange(wpn, range, grid(player));
    }
}
