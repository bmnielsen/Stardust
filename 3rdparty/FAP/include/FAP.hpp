#pragma once

#include "FAP/Unit.hpp"

#include <vector>

namespace FAP {
  template<typename T = std::tuple<>>
  auto makeUnit() {
    Unit<UnitValues{}, T> v;
    return v;
  }

  template<typename UnitExtension = std::tuple<>>
  struct FastAPproximation {
    /**
     * \brief Adds the unit to the simulator for player 1
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    [[deprecated]] void addUnitPlayer1(Unit<uv, UnitExtension> &&fu);

    /**
     * \brief Adds the unit to the simulator for player 1, only if it is a combat unit
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    void addIfCombatUnitPlayer1(Unit<uv, UnitExtension> &&fu);
    /**
     * \brief Adds the unit to the simulator for player 2
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    [[deprecated]] void addUnitPlayer2(Unit<uv, UnitExtension> &&fu);
    /**
     * \brief Adds the unit to the simulator for player 2, only if it is a combat unit
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    void addIfCombatUnitPlayer2(Unit<uv, UnitExtension> &&fu);

    /**
     * \brief Starts the simulation. You can run this function multiple times. Feel free to run once, get the state and keep running.
     * \param nFrames the number of frames to simulate. A negative number runs the sim until combat is over.
     */
    template<bool tankSplash = false>
    void simulate(int nFrames = 96); // = 24*4, 4 seconds on fastest

    /**
     * \brief Gets the internal state of the simulator. You can use this to get any info about the unit participating in the simulation or edit the state.
     * \return Returns a pair of pointers, where each pointer points to a vector containing that player's units.
     */
    std::pair<std::vector<FAPUnit<UnitExtension>> *, std::vector<FAPUnit<UnitExtension>> *> getState();

    /**
     * \brief Clears the simulation. All units are removed for both players. Equivalent to reconstructing.
     */
    void clear();

  private:
    std::vector<FAPUnit<UnitExtension>> player1, player2;

    // Current approach to collisions: allow two units to share the same grid cell, using half-tile resolution
    // This seems to strike a reasonable balance between improving how large melee armies are simmed and avoiding
    // expensive collision-based pathing calculations
    unsigned char collision[262144] = {};
    void updatePosition(FAPUnit<UnitExtension> &fu, int x, int y);

    bool didSomething = false;
    static void dealDamage(FAPUnit<UnitExtension> &fu, int damage, BWAPI::DamageType damageType);
    static int distSquared(FAPUnit<UnitExtension> const &u1, const FAPUnit<UnitExtension> &u2);
    static int isInRange(FAPUnit<UnitExtension> const &attacker, const FAPUnit<UnitExtension> &target, int range);
    static bool isSuicideUnit(BWAPI::UnitType ut);

    template<bool tankSplash>
    void unitsim(
            FAPUnit<UnitExtension> &fu,
            std::vector<FAPUnit<UnitExtension>> &friendlyUnits,
            std::vector<FAPUnit<UnitExtension>> &enemyUnits);

    static void medicsim(FAPUnit<UnitExtension> &fu, std::vector<FAPUnit<UnitExtension>> &friendlyUnits);
    bool suicideSim(
            FAPUnit<UnitExtension> &fu,
            std::vector<FAPUnit<UnitExtension>> &friendlyUnits,
            std::vector<FAPUnit<UnitExtension>> &enemyUnits);
    
    template<bool tankSplash>
    void isimulate();

    static void unitDeath(
            FAPUnit<UnitExtension> &&fu,
            std::vector<FAPUnit<UnitExtension>> &itsFriendlies,
            std::vector<FAPUnit<UnitExtension>> &itsEnemies);
  
    static auto max(int a, int b) {
      int vars[2] = { a, b };
      return vars[b > a];
    }

    static auto min(int a, int b) {
      int vars[2] = { a, b };
      return vars[a < b];
    }

    static bool isCombatUnit(FAPUnit<UnitExtension> &u) {
      return (u.unitType != BWAPI::UnitTypes::Protoss_Interceptor) &
        (static_cast<bool>(u.airDamage) | static_cast<bool>(u.groundDamage) | static_cast<bool>(u.unitType == BWAPI::UnitTypes::Terran_Medic));
    }
  };

  template<UnitValues uv>
  constexpr bool AssertValidUnit() {
    static_assert(Unit<uv>::hasFlag(UnitValues::x));
    static_assert(Unit<uv>::hasFlag(UnitValues::y));
    static_assert(Unit<uv>::hasFlag(UnitValues::health));
    static_assert(Unit<uv>::hasFlag(UnitValues::maxHealth));
    static_assert(Unit<uv>::hasFlag(UnitValues::armor));
    static_assert(Unit<uv>::hasFlag(UnitValues::shields));
    static_assert(Unit<uv>::hasFlag(UnitValues::maxShields));
    static_assert(Unit<uv>::hasFlag(UnitValues::speed));
    static_assert(Unit<uv>::hasFlag(UnitValues::flying));
    static_assert(Unit<uv>::hasFlag(UnitValues::elevation));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundDamage));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundCooldown));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundMaxRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundMinRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundDamageType));
    static_assert(Unit<uv>::hasFlag(UnitValues::airDamage));
    static_assert(Unit<uv>::hasFlag(UnitValues::airMinRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::airCooldown));
    static_assert(Unit<uv>::hasFlag(UnitValues::airMaxRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::airDamageType));
    static_assert(Unit<uv>::hasFlag(UnitValues::unitType));
    static_assert(Unit<uv>::hasFlag(UnitValues::unitSize));
    static_assert(Unit<uv>::hasFlag(UnitValues::isOrganic));
    //static_assert(Unit<uv>::hasFlag(UnitValues::armorUpgrades));
    //static_assert(Unit<uv>::hasFlag(UnitValues::attackUpgrades));
    static_assert(Unit<uv>::hasFlag(UnitValues::shieldUpgrades));
    static_assert(Unit<uv>::hasFlag(UnitValues::speedUpgrade));
    static_assert(Unit<uv>::hasFlag(UnitValues::attackCooldownRemaining));
    //static_assert(Unit<uv>::hasFlag(UnitValues::attackSpeedUpgrade));
    static_assert(Unit<uv>::hasFlag(UnitValues::stimmed));
    static_assert(Unit<uv>::hasFlag(UnitValues::rangeUpgrade));
    static_assert(Unit<uv>::hasFlag(UnitValues::attackerCount));
    static_assert(Unit<uv>::hasFlag(UnitValues::undetected));
    static_assert(Unit<uv>::hasFlag(UnitValues::id));
    static_assert(Unit<uv>::hasFlag(UnitValues::data));
    return true;
  }

  template<typename UnitExtension>
  template<UnitValues uv>
  void FastAPproximation<UnitExtension>::addUnitPlayer1(Unit<uv, UnitExtension> &&fu) {
    static_assert(AssertValidUnit<uv>());
    if (!fu.unit.flying && fu.unit.unitType != BWAPI::UnitTypes::Terran_Medic) {
      fu.unit.cell = (fu.unit.x >> 4) + ((fu.unit.y >> 4) << 9);
      collision[fu.unit.cell]++;
    }
    player1.emplace_back(fu.unit);
  }

  template<typename UnitExtension>
  template<UnitValues uv>
  void FastAPproximation<UnitExtension>::addIfCombatUnitPlayer1(Unit<uv, UnitExtension> &&fu) {
    if (isCombatUnit(fu.unit))
      addUnitPlayer1(std::move(fu));
  }

  template<typename UnitExtension>
  template<UnitValues uv>
  void FastAPproximation<UnitExtension>::addUnitPlayer2(Unit<uv, UnitExtension> &&fu) {
    static_assert(AssertValidUnit<uv>());
    if (!fu.unit.flying && fu.unit.unitType != BWAPI::UnitTypes::Terran_Medic) {
      fu.unit.cell = (fu.unit.x >> 4) + ((fu.unit.y >> 4) << 9);
      collision[fu.unit.cell]++;
    }
    player2.emplace_back(fu.unit);
  }

  template<typename UnitExtension>
  template<UnitValues uv>
  void FastAPproximation<UnitExtension>::addIfCombatUnitPlayer2(Unit<uv, UnitExtension> &&fu) {
    if (isCombatUnit(fu.unit))
      addUnitPlayer2(std::move(fu));
  }

  template<typename UnitExtension>
  template<bool tankSplash>
  void FastAPproximation<UnitExtension>::simulate(int nFrames) {
    while (nFrames--) {
      if (player1.empty() || player2.empty())
        break;

      didSomething = false;

      isimulate<tankSplash>();

      if (!didSomething)
        break;
    }
  }

  template<typename UnitExtension>
  std::pair<std::vector<FAPUnit<UnitExtension>> *, std::vector<FAPUnit<UnitExtension>> *> FastAPproximation<UnitExtension>::getState() {
    return { &player1, &player2 };
  }

  template<typename UnitExtension>
  void FastAPproximation<UnitExtension>::clear() {
    player1.clear(), player2.clear();
    memset(&collision[0], 0, sizeof(unsigned char) * 262144);
  }

  template<typename UnitExtension>
  void FastAPproximation<UnitExtension>::dealDamage(FAPUnit<UnitExtension> &fu, int damage, BWAPI::DamageType const damageType) {
    damage <<= 8;
    auto const remainingShields = fu.shields - damage + (fu.shieldArmor << 8);
    if (remainingShields > 0) {
      fu.shields = remainingShields;
      return;
    }
    else if (fu.shields) {
      damage -= fu.shields + (fu.shieldArmor << 8);
      fu.shields = 0;
    }

    if (!damage)
      return;

    damage -= fu.armor << 8;

    if (damageType == BWAPI::DamageTypes::Concussive) {
      if (fu.unitSize == BWAPI::UnitSizeTypes::Large)
        damage = damage / 4;
      else if (fu.unitSize == BWAPI::UnitSizeTypes::Medium)
        damage = damage / 2;
    }
    else if (damageType == BWAPI::DamageTypes::Explosive) {
      if (fu.unitSize == BWAPI::UnitSizeTypes::Small)
        damage = damage / 2;
      else if (fu.unitSize == BWAPI::UnitSizeTypes::Medium)
        damage = (damage * 3) / 4;
    }

    fu.health -= max(128, damage);
  }

  template<typename UnitExtension>
  int FastAPproximation<UnitExtension>::distSquared(const FAPUnit<UnitExtension> &u1, const FAPUnit<UnitExtension> &u2) {
    return (u1.x - u2.x) * (u1.x - u2.x) + (u1.y - u2.y) * (u1.y - u2.y);
  }

  template<typename UnitExtension>
  int FastAPproximation<UnitExtension>::isInRange(FAPUnit<UnitExtension> const &attacker, const FAPUnit<UnitExtension> &target, int range) {
    // Compute edge-to-edge x and y offsets
    int xDist =
      attacker.x > target.x
      ? ((attacker.x - attacker.unitType.dimensionLeft()) - (target.x + target.unitType.dimensionRight()) - 1)
      : ((target.x - target.unitType.dimensionLeft()) - (attacker.x + attacker.unitType.dimensionRight()) - 1);
    int yDist =
      attacker.y > target.y
      ? ((attacker.y - attacker.unitType.dimensionUp()) - (target.y + target.unitType.dimensionDown()) - 1)
      : ((target.y - target.unitType.dimensionUp()) - (attacker.y + attacker.unitType.dimensionDown()) - 1);
    if (xDist < 0) xDist = 0;
    if (yDist < 0) yDist = 0;

    // Do the BW approximate distance calculation
    if (xDist < yDist)
    {
      if (xDist < (yDist >> 2))
        return yDist <= range;

      unsigned int minCalc = (3 * xDist) >> 3;
      return ((minCalc >> 5) + minCalc + yDist - (yDist >> 4) - (yDist >> 6)) <= range;
    }

    if (yDist < (xDist >> 2))
      return xDist <= range;

    unsigned int minCalc = (3 * yDist) >> 3;
    return ((minCalc >> 5) + minCalc + xDist - (xDist >> 4) - (xDist >> 6)) <= range;
  }

  template<typename UnitExtension>
  bool FastAPproximation<UnitExtension>::isSuicideUnit(BWAPI::UnitType const ut) {
    return (ut == BWAPI::UnitTypes::Zerg_Scourge ||
      ut == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
      ut == BWAPI::UnitTypes::Zerg_Infested_Terran ||
      ut == BWAPI::UnitTypes::Protoss_Scarab);
  }

  template<typename UnitExtension>
  void FastAPproximation<UnitExtension>::updatePosition(FAPUnit<UnitExtension> &fu, int x, int y)
  {
    if (fu.flying) {
      fu.x = x;
      fu.y = y;
      return;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > 8191) x = 8191;
    if (y > 8191) y = 8191;

    int cell = (x >> 4) + ((y >> 4) << 9);
    if (cell == fu.cell) {
      fu.x = x;
      fu.y = y;
      return;
    }

    if (collision[cell] > 1) return;

    collision[fu.cell]--;
    collision[cell]++;
    fu.x = x;
    fu.y = y;
    fu.cell = cell;
  }

  template<typename UnitExtension>
  template<bool tankSplash>
  void FastAPproximation<UnitExtension>::unitsim(
          FAPUnit<UnitExtension> &fu,
          std::vector<FAPUnit<UnitExtension>> &friendlyUnits,
          std::vector<FAPUnit<UnitExtension>> &enemyUnits) {
    bool kite = false;
    if (fu.attackCooldownRemaining) {
      if (fu.unitType == BWAPI::UnitTypes::Terran_Vulture ||
          (fu.unitType == BWAPI::UnitTypes::Protoss_Dragoon &&
          fu.attackCooldownRemaining <= BWAPI::UnitTypes::Protoss_Dragoon.groundWeapon().damageCooldown() - 6))
      {
        kite = true;
      }
      else
      {
        didSomething = true;
        return;
      }
    }

    if(!(fu.groundDamage || fu.airDamage)) {
      return;
    }

    // If the unit already has a target, find it
    auto closestEnemy = enemyUnits.end();
    int closestDistSquared;
    if (fu.target) {
      for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); ++enemyIt) {
        if (enemyIt->id == fu.target) {
          closestEnemy = enemyIt;
          closestDistSquared = distSquared(fu, *enemyIt);
          break;
        }
      }
    }

    // Otherwise select the target
    if (closestEnemy == enemyUnits.end()) {
      for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); ++enemyIt) {
        if (enemyIt->undetected) continue;
        if (enemyIt->flying) {
          if (fu.airDamage) {
            auto const d = distSquared(fu, *enemyIt);
            if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
              d >= fu.airMinRangeSquared) {
              closestDistSquared = d;
              closestEnemy = enemyIt;
            }
          }
        }
        else {
          if (fu.groundDamage) {
            auto const d = distSquared(fu, *enemyIt);
            if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
              d >= fu.groundMinRangeSquared) {
              closestDistSquared = d;
              closestEnemy = enemyIt;
            }
          }
        }
      }

      if (closestEnemy != enemyUnits.end()) fu.target = closestEnemy->id;
    }

    // Kite is true for dragoons and vultures that are on their attack cooldown.
    // - If the enemy is not in our attack range, move towards it (usual behaviour for units not on their attack cooldown).
    // - If the enemy is in our attack range, move away from it unless its range is equivalent or larger than ours
    //   or our attack cooldown is about to expire.
    if (kite) {
      if (closestEnemy != enemyUnits.end()) {
        if (!isInRange(fu, *closestEnemy, (closestEnemy->flying ? fu.airMaxRange : fu.groundMaxRange))) {
          auto const dx = closestEnemy->x - fu.x;
          auto const dy = closestEnemy->y - fu.y;
          updatePosition(
            fu,
            fu.x + static_cast<int>(dx * (fu.speed / sqrt(dx * dx + dy * dy))),
            fu.y + static_cast<int>(dy * (fu.speed / sqrt(dx * dx + dy * dy))));
        } else if (fu.attackCooldownRemaining > 1 && closestEnemy->groundMaxRangeSquared < fu.groundMaxRangeSquared) {
          auto const dx = closestEnemy->x - fu.x;
          auto const dy = closestEnemy->y - fu.y;
          updatePosition(
            fu,
            fu.x - static_cast<int>(dx * (fu.speed / sqrt(dx * dx + dy * dy))),
            fu.y - static_cast<int>(dy * (fu.speed / sqrt(dx * dx + dy * dy))));
        }
      }

      didSomething = true;
      return;
    }

    if (closestEnemy != enemyUnits.end() && closestDistSquared <= fu.speedSquared &&
      !(fu.x == closestEnemy->x && fu.y == closestEnemy->y)) {
      updatePosition(fu, closestEnemy->x, closestEnemy->y);
      closestDistSquared = 0;

      didSomething = true;
    }

    if (closestEnemy != enemyUnits.end() && isInRange(fu, *closestEnemy, (closestEnemy->flying ? fu.airMaxRange : fu.groundMaxRange))) {
      if (closestEnemy->flying) {
        dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType);
        fu.attackCooldownRemaining = fu.airCooldown;
      }
      else {
        dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType);

        if constexpr (tankSplash) {
          if (fu.unitType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
            const int siegeTankBlastRadiusInner = fu.unitType.groundWeapon().innerSplashRadius();
            const int siegeTankBlastRadiusMedian = fu.unitType.groundWeapon().medianSplashRadius();
            const int siegeTankBlastRadiusOuter = fu.unitType.groundWeapon().outerSplashRadius();
            const int siegeTankBlastRadiusInnerSquared = siegeTankBlastRadiusInner * siegeTankBlastRadiusInner;
            const int siegeTankBlastRadiusMedianSquared = siegeTankBlastRadiusMedian * siegeTankBlastRadiusMedian;
            const int siegeTankBlastRadiusOuterSquared = siegeTankBlastRadiusOuter * siegeTankBlastRadiusOuter;

            for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end();) {
              if (!enemyIt->flying && enemyIt != closestEnemy) {
                bool killed = false;
                auto const effectiveDistToClosestEnemySquared = distSquared(*closestEnemy, *enemyIt) / 4; // shell hit point to unit edge
                auto underTaker = [&]() {
                  if (enemyIt->health < 1) {
                    killed = true;
                    auto temp = *enemyIt;
                    *enemyIt = enemyUnits.back();
                    enemyUnits.pop_back();
                    unitDeath(std::move(temp), enemyUnits, friendlyUnits);
                  }
                  return;
                };

                if (effectiveDistToClosestEnemySquared <= siegeTankBlastRadiusInnerSquared) { // inner
                  dealDamage(*enemyIt, fu.groundDamage, fu.groundDamageType);
                  underTaker();
                }
                else if (effectiveDistToClosestEnemySquared > siegeTankBlastRadiusInnerSquared && effectiveDistToClosestEnemySquared <= siegeTankBlastRadiusMedianSquared) { // median
                  dealDamage(*enemyIt, fu.groundDamage / 2, fu.groundDamageType);
                  underTaker();
                }
                else if (effectiveDistToClosestEnemySquared > siegeTankBlastRadiusMedianSquared && effectiveDistToClosestEnemySquared <= siegeTankBlastRadiusOuterSquared) { // outer
                  dealDamage(*enemyIt, fu.groundDamage / 4, fu.groundDamageType);
                  underTaker();
                }
                if (!killed) ++enemyIt;
              }
              else ++enemyIt;
            }
          }
        }

        fu.attackCooldownRemaining =
          fu.groundCooldown << static_cast<int>((fu.elevation != -1) & (closestEnemy->elevation != -1) & (closestEnemy->elevation > fu.elevation));
      }

      if (closestEnemy->health < 1) {
        auto temp = *closestEnemy;
        *closestEnemy = enemyUnits.back();
        enemyUnits.pop_back();
        unitDeath(std::move(temp), enemyUnits, friendlyUnits);
      }

      didSomething = true;
    }
    else if (closestEnemy != enemyUnits.end() && closestDistSquared > fu.speedSquared && fu.speed >= 1.0f) {
      auto const dx = closestEnemy->x - fu.x;
      auto const dy = closestEnemy->y - fu.y;
      updatePosition(
        fu,
        fu.x + static_cast<int>(dx * (fu.speed / sqrt(dx * dx + dy * dy))),
        fu.y + static_cast<int>(dy * (fu.speed / sqrt(dx * dx + dy * dy))));

      didSomething = true;
    }
  }

  template<typename UnitExtension>
  void FastAPproximation<UnitExtension>::medicsim(FAPUnit<UnitExtension> &fu, std::vector<FAPUnit<UnitExtension>> &friendlyUnits) {
    auto closestHealable = friendlyUnits.end();
    int closestDist;

    for (auto it = friendlyUnits.begin(); it != friendlyUnits.end(); ++it) {
      if (it->isOrganic && it->health < it->maxHealth && !it->didHealThisFrame) {
        auto const d = distSquared(fu, *it);
        if (closestHealable == friendlyUnits.end() || d < closestDist) {
          closestHealable = it;
          closestDist = d;
        }
      }
    }

    if (closestHealable != friendlyUnits.end()) {
      fu.x = closestHealable->x;
      fu.y = closestHealable->y;

      closestHealable->health += 150;

      if (closestHealable->health > closestHealable->maxHealth)
        closestHealable->health = closestHealable->maxHealth;

      closestHealable->didHealThisFrame = true;
    }
  }

  template<typename UnitExtension>
  bool FastAPproximation<UnitExtension>::suicideSim(
          FAPUnit<UnitExtension> &fu,
          std::vector<FAPUnit<UnitExtension>> &friendlyUnits,
          std::vector<FAPUnit<UnitExtension>> &enemyUnits) {
    auto closestEnemy = enemyUnits.end();
    int closestDistSquared;

    for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); ++enemyIt) {
      if (enemyIt->flying) {
        if (fu.airDamage) {
          auto const d = distSquared(fu, *enemyIt);
          if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
            d >= fu.airMinRangeSquared) {
            closestDistSquared = d;
            closestEnemy = enemyIt;
          }
        }
      }
      else {
        if (fu.groundDamage) {
          int d = distSquared(fu, *enemyIt);
          if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
            d >= fu.groundMinRangeSquared) {
            closestDistSquared = d;
            closestEnemy = enemyIt;
          }
        }
      }
    }

    if (closestEnemy != enemyUnits.end() && closestDistSquared <= fu.speedSquared) {
      if (closestEnemy->flying)
        dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType);
      else
        dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType);

      if (closestEnemy->health < 1) {
        auto temp = *closestEnemy;
        *closestEnemy = enemyUnits.back();
        enemyUnits.pop_back();
        unitDeath(std::move(temp), enemyUnits, friendlyUnits);
      }

      didSomething = true;
      return true;
    }
    else if (closestEnemy != enemyUnits.end() && closestDistSquared > fu.speedSquared) {
      auto const dx = closestEnemy->x - fu.x;
      auto const dy = closestEnemy->y - fu.y;

      fu.x += static_cast<int>(dx * (fu.speed / sqrt(dx * dx + dy * dy)));
      fu.y += static_cast<int>(dy * (fu.speed / sqrt(dx * dx + dy * dy)));

      didSomething = true;
    }

    return false;
  }

  template<typename UnitExtension>
  template<bool tankSplash>
  void FastAPproximation<UnitExtension>::isimulate() {
    const auto simUnit = [this](auto &unit, auto &friendly, auto &enemy) {
      if (isSuicideUnit(unit->unitType)) {
        auto const unitDied = suicideSim(*unit, friendly, enemy);
        if (unitDied) {
          auto temp = *unit;
          unit = friendly.erase(unit);
          unitDeath(std::move(temp), friendly, enemy);
        }
        else ++unit;
      }
      else {
        if (unit->unitType == BWAPI::UnitTypes::Terran_Medic)
          medicsim(*unit, friendly);
        else
          unitsim<tankSplash>(*unit, friendly, enemy);
        ++unit;
      }
    };

    for (auto fu = player1.begin(); fu != player1.end();) {
      simUnit(fu, player1, player2);
    }

    for (auto fu = player2.begin(); fu != player2.end();) {
      simUnit(fu, player2, player1);
    }

    const auto updateUnit = [](FAPUnit<UnitExtension> &fu) {
      if (fu.attackCooldownRemaining)
        --fu.attackCooldownRemaining;
      if (fu.didHealThisFrame)
        fu.didHealThisFrame = false;

      if (fu.unitType.getRace() == BWAPI::Races::Zerg) {
        if(fu.unitType != BWAPI::UnitTypes::Zerg_Egg &&
           fu.unitType != BWAPI::UnitTypes::Zerg_Lurker_Egg &&
           fu.unitType != BWAPI::UnitTypes::Zerg_Larva) {
            if (fu.health < fu.maxHealth)
              fu.health += 4;
            if (fu.health > fu.maxHealth)
              fu.health = fu.maxHealth;
          }
      }
      else if (fu.unitType.getRace() == BWAPI::Races::Protoss) {
        if (fu.shields < fu.maxShields)
          fu.shields += 7;
        if (fu.shields > fu.maxShields)
          fu.shields = fu.maxShields;
      }
      // Assume the enemy repairs their bunker
      // TODO: Actually simulate based on observations
      else if (fu.unitType == BWAPI::UnitTypes::Terran_Bunker) {
        if (fu.health < fu.maxHealth)
          fu.health += 112;
      }
    };

    for (auto &fu : player1)
      updateUnit(fu);

    for (auto &fu : player2)
      updateUnit(fu);
  }

  template<typename UnitExtension>
  void FastAPproximation<UnitExtension>::unitDeath(
          FAPUnit<UnitExtension> &&fu,
          std::vector<FAPUnit<UnitExtension>> &itsFriendlies,
          std::vector<FAPUnit<UnitExtension>> &itsEnemies) {
    for (auto & enemy : itsEnemies) {
      if (enemy.target == fu.id) enemy.target = 0;
    }
    if (fu.unitType == BWAPI::UnitTypes::Terran_Bunker && fu.numAttackers) {
      fu.unitType = BWAPI::UnitTypes::Terran_Marine;

      auto squaredRange = [](int tiles) constexpr {
        return (tiles * 32) * (tiles * 32);
      };

      switch(fu.groundMaxRangeSquared) {
      // No range upgrade
      case squaredRange(5):
        fu.groundMaxRangeSquared = squaredRange(4);
        break;
      // Range upgrade
      case squaredRange(6):
        fu.groundMaxRangeSquared = squaredRange(5);
        break;
      }

      fu.airMaxRangeSquared = fu.groundMaxRangeSquared;

      // @TODO: I guess I need to ask the bot here, I'll make some interface for that
      fu.armor = 0;

      fu.health = fu.maxHealth = BWAPI::UnitTypes::Terran_Marine.maxHitPoints();

      fu.groundCooldown *= 4;
      fu.airCooldown *= 4;

      for (int i = 0; i < fu.numAttackers - 1; ++i)
        itsFriendlies.push_back(reinterpret_cast<FAPUnit<UnitExtension>&>(fu));
      itsFriendlies.emplace_back(std::forward<FAPUnit<UnitExtension>>(fu));
    }
  }

} // namespace Neolib
