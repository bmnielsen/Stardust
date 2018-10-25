#pragma once

#include <tuple>
#include <cstdint>

#include "BWAPI.h"

namespace FAP {
  using TagRepr = std::uint64_t;
  enum struct UnitValues : TagRepr {
    x                       = 1ull << 0,
    y                       = 1ull << 1,
    health                  = 1ull << 2,
    maxHealth               = 1ull << 3,
    armor                   = 1ull << 4,
    shields                 = 1ull << 5,
    maxShields              = 1ull << 6,
    speed                   = 1ull << 7,
    flying                  = 1ull << 8,
    elevation               = 1ull << 9,
    groundDamage            = 1ull << 10,
    groundCooldown          = 1ull << 11,
    groundMaxRange          = 1ull << 12,
    groundMinRange          = 1ull << 13,
    groundDamageType        = 1ull << 14,
    airDamage               = 1ull << 15,
    airMinRange             = 1ull << 16,
    airCooldown             = 1ull << 17,
    airMaxRange             = 1ull << 18,
    airDamageType           = 1ull << 19,
    unitType                = 1ull << 20,
    unitSize                = 1ull << 21,
    isOrganic               = 1ull << 22,
    armorUpgrades           = 1ull << 23,
    attackUpgrades          = 1ull << 24,
    shieldUpgrades          = 1ull << 25,
    speedUpgrade            = 1ull << 26,
    attackCooldownRemaining = 1ull << 27,
    attackSpeedUpgrade      = 1ull << 28,
    stimmed                 = 1ull << 29,
    rangeUpgrade            = 1ull << 30,
    attackerCount           = 1ull << 31,
    data                    = 1ull << 32,
  };

  template<typename UnitExtension = std::tuple<>>
  struct FAPUnit {
    int x, y;

    int health;
    int maxHealth;
    int armor;

    int shields;
    int maxShields;
    int shieldArmor;

    float speed;
    float speedSquared;
    bool flying;
    int elevation;

    int groundDamage;
    int groundCooldown;
    int groundMaxRangeSquared;
    int groundMinRangeSquared;
    BWAPI::DamageType groundDamageType;

    int airDamage;
    int airCooldown;
    int airMaxRangeSquared;
    int airMinRangeSquared;
    BWAPI::DamageType airDamageType;

    BWAPI::UnitType unitType;
    BWAPI::UnitSizeType unitSize;
    bool isOrganic;
    bool didHealThisFrame = false;

    int numAttackers;
    int attackCooldownRemaining;

    UnitExtension data;
  };

  template<UnitValues values = UnitValues{}, typename UnitExtension = std::tuple<>>
  struct Unit {
    constexpr Unit() = default;

    auto constexpr setPosition(BWAPI::Position pos) && {
      return std::move(*this).setX(pos.x).setY(pos.y);
    }

    auto constexpr setX(int x) && {
      unit.x = x;
      return std::move(*this).template addFlag<UnitValues::x>();
    }

    auto constexpr setY(int y) && {
      unit.y = y;
      return std::move(*this).template addFlag<UnitValues::y>();
    }

    auto constexpr setHealth(int health) && {
      unit.health = health << 8;
      return std::move(*this).template addFlag<UnitValues::health>();
    }

    auto constexpr setMaxHealth(int maxHealth) && {
      unit.maxHealth = maxHealth << 8;
      return std::move(*this).template addFlag<UnitValues::maxHealth>();
    }

    auto constexpr setArmor(int armor) && {
      unit.armor = armor;
      return std::move(*this).template addFlag<UnitValues::armor>();
    }

    auto constexpr setArmorUpgrades(int armorUpgrades) && {
      static_assert(hasFlag(UnitValues::armor), "Set armor before setting armor upgrades");
      unit.armor += armorUpgrades;
      return std::move(*this).template addFlag<UnitValues::armorUpgrades>();
    }

    auto constexpr setShields(int shields) && {
      unit.shields = shields << 8;
      return std::move(*this).template addFlag<UnitValues::shields>();
    }

    auto constexpr setMaxShields(int maxShields) && {
      unit.maxShields = maxShields << 8;
      return std::move(*this).template addFlag<UnitValues::maxShields>();
    }

    auto constexpr setShieldUpgrades(int shieldUpgrades) && {
      unit.shieldArmor = shieldUpgrades;
      return std::move(*this).template addFlag<UnitValues::shieldUpgrades>();
    }

    auto constexpr setSpeed(float pixelsPerFrame) && {
      unit.speed = pixelsPerFrame;
      return std::move(*this).template addFlag<UnitValues::speed>();
    }

    auto constexpr setSpeedUpgrade(bool enable) && {
      static_assert(hasFlag(UnitValues::unitType), "Set unit type before setting speed upgrade");
      static_assert(hasFlag(UnitValues::speed), "Set speed before setting speed upgrade");
      if (enable) {
        switch (unit.unitType) {
        case BWAPI::UnitTypes::Zerg_Zergling:
        case BWAPI::UnitTypes::Zerg_Hydralisk:
        case BWAPI::UnitTypes::Zerg_Ultralisk:
        case BWAPI::UnitTypes::Protoss_Shuttle:
        case BWAPI::UnitTypes::Protoss_Observer:
        case BWAPI::UnitTypes::Protoss_Zealot:
        case BWAPI::UnitTypes::Terran_Vulture:
          unit.speed *= 1.5f;
          break;

        case BWAPI::UnitTypes::Protoss_Scout:
          unit.speed *= 4.0f / 3.0f;
          break;

        case BWAPI::UnitTypes::Zerg_Overlord:
          unit.speed *= 4.0f;
          break;

        default:
          break;
        }
      }
      unit.speedSquared = unit.speed * unit.speed;
      return std::move(*this).template addFlag<UnitValues::speedUpgrade>();
    }

    auto constexpr setFlying(bool flying) && {
      unit.flying = flying;
      return std::move(*this).template addFlag<UnitValues::flying>();
    }

    auto constexpr setElevation(int elevation = -1) && {
      unit.elevation = elevation;
      return std::move(*this).template addFlag<UnitValues::elevation>();
    }

    auto constexpr setOrganic(bool organic) && {
      unit.isOrganic = organic;
      return std::move(*this).template addFlag<UnitValues::isOrganic>();
    }

    auto constexpr setGroundDamage(int groundDamage) && {
      unit.groundDamage = groundDamage;
      return std::move(*this).template addFlag<UnitValues::groundDamage>();
    }

    auto constexpr setGroundCooldown(int groundCooldown) && {
      unit.groundCooldown = groundCooldown;
      return std::move(*this).template addFlag<UnitValues::groundCooldown>();
    }

    auto constexpr setGroundMaxRange(int groundMaxRange) && {
      unit.groundMaxRangeSquared = groundMaxRange;
      return std::move(*this).template addFlag<UnitValues::groundMaxRange>();
    }

    auto constexpr setGroundMinRange(int groundMinRange) && {
      unit.groundMinRangeSquared = groundMinRange;
      return std::move(*this).template addFlag<UnitValues::groundMinRange>();
    }

    auto constexpr setGroundDamageType(BWAPI::DamageType groundDamageType) && {
      unit.groundDamageType = groundDamageType;
      return std::move(*this).template addFlag<UnitValues::groundDamageType>();
    }

    auto constexpr setAirDamage(int airDamage) && {
      unit.airDamage = airDamage;
      return std::move(*this).template addFlag<UnitValues::airDamage>();
    }

    auto constexpr setAirCooldown(int airCooldown) && {
      unit.airCooldown = airCooldown;
      return std::move(*this).template addFlag<UnitValues::airCooldown>();
    }

    auto constexpr setAirMaxRange(int airMaxRange) && {
      unit.airMaxRangeSquared = airMaxRange;
      return std::move(*this).template addFlag<UnitValues::airMaxRange>();
    }

    auto constexpr setAirMinRange(int airMinRange) && {
      unit.airMinRangeSquared = airMinRange;
      return std::move(*this).template addFlag<UnitValues::airMinRange>();
    }

    auto constexpr setAirDamageType(BWAPI::DamageType airDamageType) && {
      unit.airDamageType = airDamageType;
      return std::move(*this).template addFlag<UnitValues::airDamageType>();
    }

    auto constexpr setAttackUpgrades(int upgradeLevel) && {
      static_assert(hasFlag(UnitValues::unitType), "Set unit type before setting attack upgrades");
      static_assert(hasFlag(UnitValues::groundDamage) && hasFlag(UnitValues::airDamage), "Set damage before setting attack upgrades");
      static_assert(hasFlag(UnitValues::attackerCount), "Set attacker count before setting attack upgrades");
      if (unit.groundDamage || unit.airDamage) {
        switch (unit.unitType) {
        case BWAPI::UnitTypes::Protoss_Carrier:
          unit.groundDamage = BWAPI::UnitTypes::Protoss_Interceptor.groundWeapon().damageAmount() + BWAPI::UnitTypes::Protoss_Interceptor.groundWeapon().damageBonus() * upgradeLevel;
          unit.airDamage = BWAPI::UnitTypes::Protoss_Interceptor.airWeapon().damageAmount() + BWAPI::UnitTypes::Protoss_Interceptor.airWeapon().damageBonus() * upgradeLevel;
          break;
        case BWAPI::UnitTypes::Terran_Bunker:
          unit.groundDamage = BWAPI::UnitTypes::Terran_Marine.groundWeapon().damageAmount() + BWAPI::UnitTypes::Terran_Marine.groundWeapon().damageBonus() * upgradeLevel;
          unit.airDamage = BWAPI::UnitTypes::Terran_Marine.airWeapon().damageAmount() + BWAPI::UnitTypes::Terran_Marine.airWeapon().damageBonus() * upgradeLevel;
          break;
        case BWAPI::UnitTypes::Protoss_Reaver:
          unit.groundDamage = BWAPI::UnitTypes::Protoss_Scarab.groundWeapon().damageAmount() + BWAPI::UnitTypes::Protoss_Scarab.groundWeapon().damageBonus() * upgradeLevel;
          break;
        default:
          unit.groundDamage += unit.unitType.groundWeapon().damageBonus() * upgradeLevel;
          unit.airDamage += unit.unitType.airWeapon().damageBonus() * upgradeLevel;
          break;
        }
      }
      return std::move(*this).template addFlag<UnitValues::attackUpgrades>();
    }

    auto constexpr setAttackSpeedUpgrade(bool enabled) && {
      static_assert(hasFlag(UnitValues::groundCooldown) && hasFlag(UnitValues::airCooldown), "Set attack cooldown before setting attack speed upgrade");
      unit.groundCooldown -= unit.groundCooldown / 4 * static_cast<int>(enabled);
      unit.airCooldown -= unit.airCooldown / 4 * static_cast<int>(enabled);
      return std::move(*this).template addFlag<UnitValues::attackSpeedUpgrade>();
    } 

    auto constexpr setStimmed(bool stimmed) && {
      static_assert(hasFlag(UnitValues::groundCooldown) && hasFlag(UnitValues::airCooldown), "Set attack cooldown before setting stim status");
      static_assert(hasFlag(UnitValues::speed), "Set unit speed before setting stim status");
      unit.groundCooldown >>= static_cast<int>(stimmed);
      unit.airCooldown >>= static_cast<int>(stimmed);
      unit.speed *= 1.0f + 0.5f * static_cast<int>(stimmed);
      unit.speedSquared = unit.speed * unit.speed;
      return std::move(*this).template addFlag<UnitValues::stimmed>();
    }

    auto constexpr setGroundWeapon(BWAPI::WeaponType groundWeapon, int groundHits) && {
      return std::move(*this)
        .setGroundDamage(groundWeapon.damageAmount())
        .setGroundCooldown(groundWeapon.damageCooldown()
          / std::max(groundHits * groundWeapon.damageFactor(), 1))
        .setGroundDamageType(groundWeapon.damageType())
        .setGroundMinRange(groundWeapon.minRange())
        .setGroundMaxRange(groundWeapon.maxRange());
    }

    auto constexpr setAirWeapon(BWAPI::WeaponType airWeapon, int airHits) && {
      return std::move(*this)
        .setAirDamage(airWeapon.damageAmount())
        .setAirCooldown(airWeapon.damageCooldown()
          / std::max(airHits * airWeapon.damageFactor(), 1))
        .setAirDamageType(airWeapon.damageType())
        .setAirMinRange(airWeapon.minRange())
        .setAirMaxRange(airWeapon.maxRange());
    }
    
    auto constexpr setUnitSize(BWAPI::UnitSizeType unitSize) && {
      unit.unitSize = unitSize;
      return std::move(*this).template addFlag<UnitValues::unitSize>();
    }

    auto constexpr setOnlyUnitType(BWAPI::UnitType type) && {
      unit.unitType = type;
      return std::move(*this).template addFlag<UnitValues::unitType>();
    }

    auto constexpr setUnitType(BWAPI::UnitType type) && {
      return std::move(*this)
        .setOnlyUnitType(type)
        .setMaxHealth(type.maxHitPoints())
        .setMaxShields(type.maxShields())
        .setOrganic(type.isOrganic())
        .setSpeed(static_cast<float>(type.topSpeed()))
        .setArmor(type.armor())
        .setGroundWeapon(type.groundWeapon(), type.maxGroundHits())
        .setAirWeapon(type.airWeapon(), type.maxAirHits())
        .setUnitSize(type.size());
    }

    auto constexpr setAttackCooldownRemaining(int attackCooldownRemaining = 0) && {
      unit.attackCooldownRemaining = attackCooldownRemaining;
      return std::move(*this).template addFlag<UnitValues::attackCooldownRemaining>();
    }

    template<typename T = UnitExtension>
    auto constexpr setData(T &&val) {
      unit.data = val;
      return std::move(*this).template addFlag<UnitValues::data>();
    }

    auto constexpr setRangeUpgrade(bool rangeUpgraded) && {
      static_assert(hasFlag(UnitValues::groundMaxRange) && hasFlag(UnitValues::airMaxRange) &&
                    hasFlag(UnitValues::unitType), "Set type and max range before setting range upgrade level");
      auto extraRange = [&](BWAPI::WeaponType weapon) {
        switch(weapon) {
        // Charon Boosters
        case BWAPI::WeaponTypes::Hellfire_Missile_Pack:
          return 32 * 3;

        // Singularity Charge
        case BWAPI::WeaponTypes::Phase_Disruptor:
          return 32 * 2;

        // U-238 and Grooved Spines
        case BWAPI::WeaponTypes::Gauss_Rifle:
        case BWAPI::WeaponTypes::Needle_Spines:
          return 32 * 1;
        default:
          return 0;
        }
      };

      int const groundRange[] = { unit.groundMaxRangeSquared, unit.groundMaxRangeSquared + extraRange(unit.unitType.groundWeapon()), 5 * 32, 6 * 32, 32 * 8, 32 * 8 };
      unit.groundMaxRangeSquared = groundRange[rangeUpgraded + (unit.unitType == BWAPI::UnitTypes::Terran_Bunker) * 2 + (unit.unitType == BWAPI::UnitTypes::Protoss_Carrier) * 4];

      int const airRange[] = { unit.airMaxRangeSquared, unit.airMaxRangeSquared + extraRange(unit.unitType.airWeapon()), 5 * 32, 6 * 32, 32 * 8, 32 * 8 };
      unit.airMaxRangeSquared = airRange[rangeUpgraded + (unit.unitType == BWAPI::UnitTypes::Terran_Bunker) * 2 + (unit.unitType == BWAPI::UnitTypes::Protoss_Carrier) * 4];

      // Store squares of ranges
      unit.groundMinRangeSquared *= unit.groundMinRangeSquared;
      unit.airMinRangeSquared *= unit.airMinRangeSquared;
      unit.groundMaxRangeSquared *= unit.groundMaxRangeSquared;
      unit.airMaxRangeSquared *= unit.airMaxRangeSquared;

      return std::move(*this).template addFlag<UnitValues::rangeUpgrade>();
    }

    // If the unit is a carrier (interceptor count) or bunker (marine count)
    auto constexpr setAttackerCount(int numAttackers) && {
      static_assert(hasFlag(UnitValues::unitType) && hasFlag(UnitValues::groundDamage) && hasFlag(UnitValues::airDamage), "Set unit type and damage before setting number of attackers");
      unit.numAttackers = numAttackers;
      if (numAttackers) {
        if (unit.unitType == BWAPI::UnitTypes::Protoss_Carrier) {
          unit.groundDamage = unit.airDamage = BWAPI::UnitTypes::Protoss_Interceptor.groundWeapon().damageAmount();
          unit.airCooldown = unit.groundCooldown = static_cast<int>(round(37.4f / numAttackers));
        }

        if (unit.unitType == BWAPI::UnitTypes::Terran_Bunker) {
          unit.groundDamage = unit.airDamage = BWAPI::UnitTypes::Terran_Marine.groundWeapon().damageAmount();
          unit.groundCooldown = unit.airCooldown = BWAPI::UnitTypes::Terran_Marine.groundWeapon().damageCooldown() / numAttackers;
        }
      }
      else if((unit.unitType == BWAPI::UnitTypes::Protoss_Carrier) | (unit.unitType == BWAPI::UnitTypes::Terran_Bunker)) {
        unit.groundDamage = unit.airDamage = 0;
      }

      if(unit.unitType == BWAPI::UnitTypes::Protoss_Reaver) {
        unit.groundDamage = BWAPI::UnitTypes::Protoss_Scarab.groundWeapon().damageAmount();
      }
      return std::move(*this).template addFlag<UnitValues::attackerCount>();
    }

  private:
    constexpr Unit(FAPUnit<UnitExtension> &&u) : unit{ u } { }
    template<UnitValues otherValues, typename Extension>
    friend struct Unit;
    template<typename Extension>
    friend struct FastAPproximation;
    template<UnitValues uv>
    friend constexpr bool AssertValidUnit();
    FAPUnit<UnitExtension> unit;

    template<UnitValues bitToSet>
    auto addFlag() && {
      auto constexpr newTag = static_cast<UnitValues>(static_cast<TagRepr>(values) | static_cast<TagRepr>(bitToSet));
      static_assert(static_cast<TagRepr>(newTag) != static_cast<TagRepr>(values), "Value already set!");
      return Unit<newTag, UnitExtension>{std::move(unit)};
    }

    static bool constexpr hasFlag(UnitValues flag) {
      return static_cast<bool>(static_cast<TagRepr>(values) & static_cast<TagRepr>(flag));
    }
  };
}
