#pragma once

#include "FAP/Unit.hpp"

#include <vector>

#define TANK_SPLASH_INNER_RADIUS_SQAURED 100
#define TANK_SPLASH_MEDIAN_RADIUS_SQAURED 625
#define TANK_SPLASH_OUTER_RADIUS_SQAURED 1600

namespace FAP {
  template<typename T = std::tuple<>>
  auto makeUnit() {
    Unit<UnitValues{}, T> v;
    return v;
  }

  struct ChokeGeometry {
    std::vector<int> &tileSide;
    std::vector<BWAPI::Position> forward;
    std::vector<BWAPI::Position> backwardVector;
    ChokeGeometry(std::vector<int> &tileSide,
                  BWAPI::Position end1Center,
                  BWAPI::Position end2Center,
                  BWAPI::Position end1Exit,
                  BWAPI::Position end2Exit) : tileSide(tileSide) {
      forward.resize(7);
      forward[0] = forward[1] = end1Center;
      forward[4] = forward[6] = end2Center;
      forward[5] = end1Exit;
      forward[2] = end2Exit;
      backwardVector.resize(7);
      backwardVector[0] = backwardVector[1] = backwardVector[5] = (end2Center - end1Center);
      backwardVector[2] = backwardVector[4] = backwardVector[6] = (end1Center - end2Center);
    }
  };

  template<typename UnitExtension = std::tuple<>>
  struct FastAPproximation {
    FastAPproximation(std::vector<unsigned char> &collision) : collision(collision) {}

    /**
     * \brief Adds the unit to the simulator for player 1, only if it is a combat unit
     * \param fu The FAPUnit to add
     */
    template<bool choke = false, UnitValues uv>
    bool addIfCombatUnitPlayer1(Unit<uv, UnitExtension> &&fu);

    /**
     * \brief Adds the unit to the simulator for player 2, only if it is a combat unit
     * \param fu The FAPUnit to add
     */
    template<bool choke = false, UnitValues uv>
    bool addIfCombatUnitPlayer2(Unit<uv, UnitExtension> &&fu);

    /**
     * \brief Starts the simulation. You can run this function multiple times. Feel free to run once, get the state and keep running.
     * \param nFrames the number of frames to simulate. A negative number runs the sim until combat is over.
     */
    template<bool tankSplash = false, bool choke = false>
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

    /**
     * \brief Sets that this battle is happening through a choke and configures the choke geometry
     */
    void setChokeGeometry(std::vector<int> &tileSide,
                          BWAPI::Position end1Center,
                          BWAPI::Position end2Center,
                          BWAPI::Position end1Exit,
                          BWAPI::Position end2Exit);

  private:
    std::vector<FAPUnit<UnitExtension>> player1, player2;

    std::unique_ptr<ChokeGeometry> chokeGeometry;

    // Current approach to collisions: allow two units to share the same grid cell, using half-tile resolution
    // This seems to strike a reasonable balance between improving how large melee armies are simmed and avoiding
    // expensive collision-based pathing calculations
    std::vector<unsigned char> &collision;

    template<bool choke>
    void initializeCollision(FAPUnit<UnitExtension> &fu);
    template<bool choke = false>
    void updatePosition(FAPUnit<UnitExtension> &fu, int x, int y);

    bool didSomething = false;
    static void dealDamage(FAPUnit<UnitExtension> &fu, int damage, BWAPI::DamageType damageType, FAPUnit<UnitExtension> &attacker);

    template<bool choke = false>
    int distSquared(FAPUnit<UnitExtension> const &u1, const FAPUnit<UnitExtension> &u2);
    static int isInRange(FAPUnit<UnitExtension> const &attacker, const FAPUnit<UnitExtension> &target, int minRange, int maxRange);
    static int edgeToPointDistance(FAPUnit<UnitExtension> const &fu, int x, int y);
    static bool isSuicideUnit(BWAPI::UnitType ut);

    template<bool tankSplash, bool choke>
    void unitsim(
            FAPUnit<UnitExtension> &fu,
            std::vector<FAPUnit<UnitExtension>> &friendlyUnits,
            std::vector<FAPUnit<UnitExtension>> &enemyUnits);

    void medicsim(FAPUnit<UnitExtension> &fu, std::vector<FAPUnit<UnitExtension>> &friendlyUnits);
    bool suicideSim(
            FAPUnit<UnitExtension> &fu,
            std::vector<FAPUnit<UnitExtension>> &friendlyUnits,
            std::vector<FAPUnit<UnitExtension>> &enemyUnits);

    template<bool tankSplash, bool choke>
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
        (static_cast<bool>(u.airDamage) | static_cast<bool>(u.groundDamage)
                | static_cast<bool>(u.unitType == BWAPI::UnitTypes::Terran_Medic)
                | static_cast<bool>(u.unitType == BWAPI::UnitTypes::Zerg_Overlord));
    }
  };

  template<UnitValues uv>
  constexpr bool AssertValidUnit() {
    static_assert(Unit<uv>::hasFlag(UnitValues::x));
    static_assert(Unit<uv>::hasFlag(UnitValues::y));
    static_assert(Unit<uv>::hasFlag(UnitValues::targetX));
    static_assert(Unit<uv>::hasFlag(UnitValues::targetY));
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
    static_assert(Unit<uv>::hasFlag(UnitValues::target));
    static_assert(Unit<uv>::hasFlag(UnitValues::collision));
    static_assert(Unit<uv>::hasFlag(UnitValues::data));
    return true;
  }

  template<typename UnitExtension>
  template<bool choke, UnitValues uv>
  bool FastAPproximation<UnitExtension>::addIfCombatUnitPlayer1(Unit<uv, UnitExtension> &&fu) {
    if (isCombatUnit(fu.unit)) {
      static_assert(AssertValidUnit<uv>());
      initializeCollision<choke>(fu.unit);
      fu.unit.player = 1;
      player1.emplace_back(fu.unit);
      return true;
    }
    return false;
  }

  template<typename UnitExtension>
  template<bool choke, UnitValues uv>
  bool FastAPproximation<UnitExtension>::addIfCombatUnitPlayer2(Unit<uv, UnitExtension> &&fu) {
    if (isCombatUnit(fu.unit)) {
      static_assert(AssertValidUnit<uv>());
      initializeCollision<choke>(fu.unit);
      fu.unit.player = 2;
      player2.emplace_back(fu.unit);
      return true;
    }
    return false;
  }

  template<typename UnitExtension>
  template<bool tankSplash, bool choke>
  void FastAPproximation<UnitExtension>::simulate(int nFrames) {
    while (nFrames--) {
      if (player1.empty() || player2.empty())
        break;

      didSomething = false;

      isimulate<tankSplash, choke>();

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
    std::fill(collision.begin(), collision.end(), 0);
  }

  template<typename UnitExtension>
  void FastAPproximation<UnitExtension>::setChokeGeometry(std::vector<int> &tileSide,
                                                          BWAPI::Position end1Center,
                                                          BWAPI::Position end2Center,
                                                          BWAPI::Position end1Exit,
                                                          BWAPI::Position end2Exit)
  {
    chokeGeometry = std::make_unique<ChokeGeometry>(tileSide, end1Center, end2Center, end1Exit, end2Exit);
  }

  template<typename UnitExtension>
  void FastAPproximation<UnitExtension>::dealDamage(FAPUnit<UnitExtension> &fu,
                                                    int damage,
                                                    BWAPI::DamageType const damageType,
                                                    FAPUnit<UnitExtension> &attacker) {
    damage <<= 8;

    if (!fu.flying && !attacker.flying &&
        attacker.groundMaxRange > 32 &&
        fu.elevation != -1 && attacker.elevation != -1 && fu.elevation > attacker.elevation) {
      damage >>= 1;
    }

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
  template<bool choke>
  int FastAPproximation<UnitExtension>::distSquared(const FAPUnit<UnitExtension> &u1, const FAPUnit<UnitExtension> &u2) {
      if (choke && !u1.flying) {
          auto sideDiff = chokeGeometry->tileSide[u1.cell] - chokeGeometry->tileSide[u2.cell];
          if (sideDiff == 0) return (u1.x - u2.x) * (u1.x - u2.x) + (u1.y - u2.y) * (u1.y - u2.y);
          return (u1.x - chokeGeometry->forward[3 + sideDiff].x) * (u1.x - chokeGeometry->forward[3 + sideDiff].x) +
                 (u1.y - chokeGeometry->forward[3 + sideDiff].y) * (u1.y - chokeGeometry->forward[3 + sideDiff].y) +
                 (chokeGeometry->forward[3 + sideDiff].x - chokeGeometry->forward[3 - sideDiff].x) * (chokeGeometry->forward[3 + sideDiff].x - chokeGeometry->forward[3 - sideDiff].x) +
                 (chokeGeometry->forward[3 + sideDiff].y - chokeGeometry->forward[3 - sideDiff].y) * (chokeGeometry->forward[3 + sideDiff].y - chokeGeometry->forward[3 - sideDiff].y) +
                 (u2.x - chokeGeometry->forward[3 - sideDiff].x) * (u2.x - chokeGeometry->forward[3 - sideDiff].x) +
                 (u2.y - chokeGeometry->forward[3 - sideDiff].y) * (u2.y - chokeGeometry->forward[3 - sideDiff].y);
      } else {
          return (u1.x - u2.x) * (u1.x - u2.x) + (u1.y - u2.y) * (u1.y - u2.y);
      }
  }

  template<typename UnitExtension>
  int FastAPproximation<UnitExtension>::isInRange(FAPUnit<UnitExtension> const &attacker,
                                                  const FAPUnit<UnitExtension> &target,
                                                  int minRange,
                                                  int maxRange) {
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
    int dist;

    if (xDist < yDist)
    {
      if (xDist < (yDist >> 2)) {
        dist = yDist;
      } else {
        unsigned int minCalc = (3 * xDist) >> 3;
        dist = ((minCalc >> 5) + minCalc + yDist - (yDist >> 4) - (yDist >> 6));
      }
    } else {
      if (yDist < (xDist >> 2)) {
        dist = xDist;
      } else {
        unsigned int minCalc = (3 * yDist) >> 3;
        dist = ((minCalc >> 5) + minCalc + xDist - (xDist >> 4) - (xDist >> 6));
      }
    }

    return dist >= minRange && dist <= maxRange;
  }

  template<typename UnitExtension>
  int FastAPproximation<UnitExtension>::edgeToPointDistance(FAPUnit<UnitExtension> const &fu, int x, int y) {
    // Compute edge-to-edge x and y offsets
    int xDist =
      fu.x > x
      ? ((fu.x - fu.unitType.dimensionLeft()) - x - 1)
      : (x - (fu.x + fu.unitType.dimensionRight()) - 1);
    int yDist =
      fu.y > y
      ? ((fu.y - fu.unitType.dimensionUp()) - y - 1)
      : (y - (fu.y + fu.unitType.dimensionDown()) - 1);
    if (xDist < 0) xDist = 0;
    if (yDist < 0) yDist = 0;

    // Do the BW approximate distance calculation
    if (xDist < yDist)
    {
      if (xDist < (yDist >> 2)) {
        return yDist;
      }

      unsigned int minCalc = (3 * xDist) >> 3;
      return ((minCalc >> 5) + minCalc + yDist - (yDist >> 4) - (yDist >> 6));
    }

    if (yDist < (xDist >> 2)) {
      return xDist;
    }

    unsigned int minCalc = (3 * yDist) >> 3;
    return ((minCalc >> 5) + minCalc + xDist - (xDist >> 4) - (xDist >> 6));
  }

  template<typename UnitExtension>
  bool FastAPproximation<UnitExtension>::isSuicideUnit(BWAPI::UnitType const ut) {
    return (ut == BWAPI::UnitTypes::Zerg_Scourge ||
      ut == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
      ut == BWAPI::UnitTypes::Zerg_Infested_Terran ||
      ut == BWAPI::UnitTypes::Protoss_Scarab);
  }

  template<typename UnitExtension>
  template<bool choke>
  void FastAPproximation<UnitExtension>::initializeCollision(FAPUnit<UnitExtension> &fu) {
    fu.cell = (fu.x >> 4) + ((fu.y >> 4) * BWAPI::Broodwar->mapWidth() * 2);
    if constexpr (choke) {
      collision[fu.cell] += (chokeGeometry->tileSide[fu.cell] == 0) ? fu.collisionValueChoke : fu.collisionValue;
    } else {
      collision[fu.cell] += fu.collisionValue;
    }
    fu.targetCell = (fu.targetX >> 4) + ((fu.targetY >> 4) * BWAPI::Broodwar->mapWidth() * 2);
  }

  template<typename UnitExtension>
  template<bool choke>
  void FastAPproximation<UnitExtension>::updatePosition(FAPUnit<UnitExtension> &fu, int x, int y) {
    if (fu.flying) {
      fu.x = x;
      fu.y = y;
      return;
    }

    x = std::clamp(x, 0, BWAPI::Broodwar->mapWidth() * 32 - 1);
    y = std::clamp(y, 0, BWAPI::Broodwar->mapHeight() * 32 - 1);

    int cell = (x >> 4) + (y >> 4) * BWAPI::Broodwar->mapWidth() * 2;
    if (cell == fu.cell) {
      fu.x = x;
      fu.y = y;
      return;
    }

    if constexpr (choke) {
      int collisionValue = (chokeGeometry->tileSide[cell] == 0) ? fu.collisionValueChoke : fu.collisionValue;
      if (collision[cell] + collisionValue > 12) return;

      collision[fu.cell] -= (chokeGeometry->tileSide[fu.cell] == 0) ? fu.collisionValueChoke : fu.collisionValue;
      collision[cell] += collisionValue;
      fu.x = x;
      fu.y = y;
      fu.cell = cell;

      return;
    }

    if (collision[cell] + fu.collisionValue > 12) return;

    collision[fu.cell] -= fu.collisionValue;
    collision[cell] += fu.collisionValue;
    fu.x = x;
    fu.y = y;
    fu.cell = cell;
  }

  template<typename UnitExtension>
  template<bool tankSplash, bool choke>
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

    // Target selection

    // Start by fetching our current target, if we have one
    auto currentTarget = enemyUnits.end();
    int currentTargetDist = INT_MAX;
    if (fu.target) {
      for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); ++enemyIt) {
        if (enemyIt->health > 0 && enemyIt->id == fu.target) {
          currentTarget = enemyIt;
          currentTargetDist = distSquared(fu, *enemyIt);
          break;
        }
      }

      // Clear the target if it wasn't found or if it is within the unit's minimum range
      if (currentTarget == enemyUnits.end() || currentTargetDist < fu.groundMinRangeSquared) {
        fu.target = 0;
      }
    }

    // Potentially replace our current target with another one if we are off cooldown and it isn't in range
    auto closestEnemy = currentTarget;
    int closestDistSquared = currentTargetDist;
    if (closestEnemy == enemyUnits.end() ||
                        (closestDistSquared > (closestEnemy->flying ? fu.airMaxRangeSquared : fu.groundMaxRangeSquared) && !kite)) {
      for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); ++enemyIt) {
        if (enemyIt->health < 1 || enemyIt->undetected || enemyIt == currentTarget) continue;
        if (enemyIt->flying) {
          if (fu.airDamage) {
            auto const d = distSquared<choke>(fu, *enemyIt);
            if (d < closestDistSquared && d >= fu.airMinRangeSquared) {
              closestDistSquared = d;
              closestEnemy = enemyIt;
            }
          }
        }
        else {
          if (fu.groundDamage) {
            auto const d = distSquared<choke>(fu, *enemyIt);
            if (d < closestDistSquared && d >= fu.groundMinRangeSquared) {
              closestDistSquared = d;
              closestEnemy = enemyIt;
            }
          }
        }
      }

      // Keep the current target if the closest alternative isn't in range either
      if (currentTarget != enemyUnits.end() && closestDistSquared > (closestEnemy->flying ? fu.airMaxRangeSquared : fu.groundMaxRangeSquared)) {
        closestEnemy = currentTarget;
        closestDistSquared = currentTargetDist;
      } else if (closestEnemy != enemyUnits.end()) {
        fu.target = closestEnemy->id;
      }
    }

    auto updatePositionTowards = [this](FAPUnit<UnitExtension> &fu, int dx, int dy) {
      auto ds = dx * dx + dy * dy;
      if (ds > 0) {
        auto r = fu.speed / sqrt(ds);
        updatePosition(fu,
                       fu.x + static_cast<int>(dx * r),
                       fu.y + static_cast<int>(dy * r));
      }
    };

    auto defendChoke = [this, &updatePositionTowards](FAPUnit<UnitExtension> &fu, FAPUnit<UnitExtension> &target) {
      auto sideDiff = chokeGeometry->tileSide[fu.cell] - chokeGeometry->tileSide[target.cell];

      int dx, dy;

      // If the unit is inside the choke, it should move out using the backward vector
      if (chokeGeometry->tileSide[fu.cell] == 0) {
        dx = chokeGeometry->backwardVector[3 + sideDiff].x;
        dy = chokeGeometry->backwardVector[3 + sideDiff].y;
      } else {
        // Otherwise the unit should move to keep its distance to the choke entrance
        int dist = edgeToPointDistance(fu, chokeGeometry->forward[3 + sideDiff].x, chokeGeometry->forward[3 + sideDiff].y);
        if (dist < (target.flying ? fu.airMaxRange : fu.groundMaxRange)) {
          dx = fu.x - chokeGeometry->forward[3 + sideDiff].x;
          dy = fu.y - chokeGeometry->forward[3 + sideDiff].y;
        } else {
          dx = chokeGeometry->forward[3 + sideDiff].x - fu.x;
          dy = chokeGeometry->forward[3 + sideDiff].y - fu.y;
        }
      }
      updatePositionTowards(fu, dx, dy);
    };

    auto moveTowards = [this, &updatePositionTowards](FAPUnit<UnitExtension> &fu, int x, int y, int cell) {
      int dx, dy;

      // Movement in chokes is handled differently, as we force the units to path through the choke ends
      if constexpr (choke) {
        if (fu.flying) {
          dx = x - fu.x;
          dy = y - fu.y;
        } else {
          auto sideDiff = chokeGeometry->tileSide[fu.cell] - chokeGeometry->tileSide[cell];
          if (sideDiff == 0) {
            dx = x - fu.x;
            dy = y - fu.y;
          } else {
            dx = chokeGeometry->forward[3 + sideDiff].x - fu.x;
            dy = chokeGeometry->forward[3 + sideDiff].y - fu.y;
          }
        }
      } else {
        dx = x - fu.x;
        dy = y - fu.y;
      }
      updatePositionTowards(fu, dx, dy);
    };

    // Move towards target position if there is no target
    if (closestEnemy == enemyUnits.end()) {
      if constexpr (choke) {
        // Attacking units always moves forward; defending units stay put
        // TODO: Defending units should really get into defend position, but this isn't a state we expect to see often
        if (fu.player == 1) {
          didSomething = true;
          moveTowards(fu, fu.targetX, fu.targetY, fu.targetCell);
        }
      } else {
        didSomething = true;
        moveTowards(fu, fu.targetX, fu.targetY, fu.targetCell);
      }
      return;
    }

    // Kite is true for dragoons and vultures that are on their attack cooldown.
    // - If the unit is in a narrow choke, possibly do some special handling depending on where the unit and its target are.
    // - If the enemy is not in our attack range, move towards it (usual behaviour for units not on their attack cooldown).
    // - If the enemy is in our attack range, move away from it unless its range is equivalent or larger than ours
    //   or our attack cooldown is about to expire.
    if (kite) {
      didSomething = true;
      if (closestEnemy != enemyUnits.end()) {
        if constexpr (choke) {
          // Defending unit moves to defend the choke if it is not in the same side as its target
          if (fu.player == 2 && chokeGeometry->tileSide[fu.cell] != chokeGeometry->tileSide[closestEnemy->cell]) {
            defendChoke(fu, *closestEnemy);
            return;
          }

          // Attacking unit moves forward if it is in the choke
          if (fu.player == 1 && chokeGeometry->tileSide[fu.cell] == 0) {
            moveTowards(fu, closestEnemy->x, closestEnemy->y, closestEnemy->cell);
            return;
          }

          // Otherwise fall through to normal kiting handling
        }

        // Move towards the enemy if it is out of range or is a sieged tank
        if (!isInRange(fu, *closestEnemy, (closestEnemy->flying ? 0 : fu.groundMinRange), (closestEnemy->flying ? fu.airMaxRange : fu.groundMaxRange)) ||
          closestEnemy->unitType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
          moveTowards(fu, closestEnemy->x, closestEnemy->y, closestEnemy->cell);
        } else if (fu.attackCooldownRemaining > 1 &&
          (closestEnemy->flying ? fu.airMaxRange : fu.groundMaxRange) > ((fu.flying ? closestEnemy->airMaxRange : closestEnemy->groundMaxRange))) {
          auto const dx = fu.x - closestEnemy->x;
          auto const dy = fu.y - closestEnemy->y;
          updatePositionTowards(fu, dx, dy);
        }
      }
      return;
    }

    if (closestEnemy != enemyUnits.end() && closestDistSquared <= fu.speedSquared &&
      !(fu.x == closestEnemy->x && fu.y == closestEnemy->y)) {
      updatePosition(fu, closestEnemy->x, closestEnemy->y);
      closestDistSquared = 0;

      didSomething = true;
    }

    if (closestEnemy != enemyUnits.end() && isInRange(fu, *closestEnemy, (closestEnemy->flying ? 0 : fu.groundMinRange), (closestEnemy->flying ? fu.airMaxRange : fu.groundMaxRange))) {
      if (closestEnemy->flying) {
        dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType, fu);
        fu.attackCooldownRemaining = fu.airCooldown;
      }
      else {
        dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType, fu);

        if constexpr (tankSplash) {
          if (fu.unitType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
            auto dealSplashDamage = [&](auto &unit) {
              auto const effectiveDistToClosestEnemySquared = distSquared(*closestEnemy, unit) / 4; // shell hit point to unit edge
              if (effectiveDistToClosestEnemySquared <= TANK_SPLASH_INNER_RADIUS_SQAURED) { // inner
                dealDamage(unit, fu.groundDamage, fu.groundDamageType, fu);
              }
              else if (effectiveDistToClosestEnemySquared > TANK_SPLASH_INNER_RADIUS_SQAURED && effectiveDistToClosestEnemySquared <= TANK_SPLASH_MEDIAN_RADIUS_SQAURED) { // median
                dealDamage(unit, fu.groundDamage >> 1, fu.groundDamageType, fu);
              }
              else if (effectiveDistToClosestEnemySquared > TANK_SPLASH_MEDIAN_RADIUS_SQAURED && effectiveDistToClosestEnemySquared <= TANK_SPLASH_OUTER_RADIUS_SQAURED) { // outer
                dealDamage(unit, fu.groundDamage >> 2, fu.groundDamageType, fu);
              }
            };
            for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); enemyIt++) {
              if (!enemyIt->flying && enemyIt != closestEnemy) {
                dealSplashDamage(*enemyIt);
              }
            }
            for (auto friendlyIt = friendlyUnits.begin(); friendlyIt != friendlyUnits.end(); friendlyIt++) {
              if (!friendlyIt->flying) {
                dealSplashDamage(*friendlyIt);
              }
            }
          }
        }

        fu.attackCooldownRemaining = fu.groundCooldown;
      }

      didSomething = true;
    }
    else if (closestEnemy != enemyUnits.end() && closestDistSquared > 0 && fu.speed >= 1.0f) {
      didSomething = true;

      if constexpr (choke) {
        // Attacking units always moves forward
        if (fu.player == 1) {
          moveTowards(fu, closestEnemy->x, closestEnemy->y, closestEnemy->cell);
        } else {
          // Defending units move to attack when ready to "spring the trap"
          // Criteria for this is one of the following:
          // - The target is in the same "side" of the choke as this unit
          // - This unit is in the target's attack range
          // - The target is close to the near end of the choke
          auto sideDiff = chokeGeometry->tileSide[fu.cell] - chokeGeometry->tileSide[closestEnemy->cell];
          bool attack = sideDiff == 0 || isInRange(*closestEnemy, fu, (fu.flying ? 0 : closestEnemy->groundMinRange), (fu.flying ? closestEnemy->airMaxRange : closestEnemy->groundMaxRange));
          if (!attack) {
            int distChokeEntranceSquared =
                  (closestEnemy->x - chokeGeometry->forward[3 + sideDiff].x) * (closestEnemy->x - chokeGeometry->forward[3 + sideDiff].x) +
                  (closestEnemy->y - chokeGeometry->forward[3 + sideDiff].y) * (closestEnemy->y - chokeGeometry->forward[3 + sideDiff].y);
            attack = distChokeEntranceSquared < 1024;
          }

          if (attack) {
            moveTowards(fu, closestEnemy->x, closestEnemy->y, closestEnemy->cell);
          } else {
            defendChoke(fu, *closestEnemy);
          }
        }
      } else {
        moveTowards(fu, closestEnemy->x, closestEnemy->y, closestEnemy->cell);
      }
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

    if (closestEnemy == enemyUnits.end()) return false;

    // Compute edge-to-edge x and y offsets
    int xDist =
      fu.x > closestEnemy->x
      ? ((fu.x - fu.unitType.dimensionLeft()) - (closestEnemy->x + closestEnemy->unitType.dimensionRight()) - 1)
      : ((closestEnemy->x - closestEnemy->unitType.dimensionLeft()) - (fu.x + fu.unitType.dimensionRight()) - 1);
    int yDist =
      fu.y > closestEnemy->y
      ? ((fu.y - fu.unitType.dimensionUp()) - (closestEnemy->y + closestEnemy->unitType.dimensionDown()) - 1)
      : ((closestEnemy->y - closestEnemy->unitType.dimensionUp()) - (fu.y + fu.unitType.dimensionDown()) - 1);
    if (xDist < 0) xDist = 0;
    if (yDist < 0) yDist = 0;

    // Do the BW approximate distance calculation
    int dist;

    if (xDist < yDist)
    {
      if (xDist < (yDist >> 2)) {
        dist = yDist;
      } else {
        unsigned int minCalc = (3 * xDist) >> 3;
        dist = ((minCalc >> 5) + minCalc + yDist - (yDist >> 4) - (yDist >> 6));
      }
    } else {
      if (yDist < (xDist >> 2)) {
        dist = xDist;
      } else {
        unsigned int minCalc = (3 * yDist) >> 3;
        dist = ((minCalc >> 5) + minCalc + xDist - (xDist >> 4) - (xDist >> 6));
      }
    }

    if (dist <= fu.groundMaxRange) {
      if (closestEnemy->flying)
        dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType, fu);
      else
        dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType, fu);

      didSomething = true;
      return true;
    }
    else if (closestDistSquared > fu.speedSquared &&
             (fu.unitType != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine || dist <= 96)) {
      auto const dx = closestEnemy->x - fu.x;
      auto const dy = closestEnemy->y - fu.y;

      fu.x += static_cast<int>(dx * (fu.speed / sqrt(dx * dx + dy * dy)));
      fu.y += static_cast<int>(dy * (fu.speed / sqrt(dx * dx + dy * dy)));

      didSomething = true;
    }

    return false;
  }

  template<typename UnitExtension>
  template<bool tankSplash, bool choke>
  void FastAPproximation<UnitExtension>::isimulate() {
    const auto simUnit = [this](auto &unit, auto &friendly, auto &enemy) {
      if (unit->health < 1) {
        ++unit;
        return;
      }
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
          unitsim<tankSplash, choke>(*unit, friendly, enemy);
        ++unit;
      }
    };

    for (auto fu = player1.begin(); fu != player1.end();) {
      simUnit(fu, player1, player2);
    }

    for (auto fu = player2.begin(); fu != player2.end();) {
      simUnit(fu, player2, player1);
    }

    const auto updateUnit = [](auto &it, auto &friendly, auto &killed) {
      auto &fu = *it;
      if (fu.health < 1) {
        killed.push_back(fu);
        it = friendly.erase(it);
        return;
      }

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
      // Assume the enemy repairs their bunker with 4 SCVs
      // TODO: Actually simulate based on observations
      else if (fu.unitType == BWAPI::UnitTypes::Terran_Bunker) {
        if (fu.health < fu.maxHealth)
          fu.health += 680;
      }

      it++;
    };

    auto updateUnits = [&](auto &friendly, auto &enemy) {
      std::vector<FAPUnit<UnitExtension>> killed;
      for (auto it = friendly.begin(); it != friendly.end();) {
        updateUnit(it, friendly, killed);
      }
      for (auto &unit : killed) {
        auto temp = unit;
        unitDeath(std::move(temp), friendly, enemy);
      }
    };

    updateUnits(player1, player2);
    updateUnits(player2, player1);
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

      fu.health = fu.maxHealth = (BWAPI::UnitTypes::Terran_Marine.maxHitPoints() << 8);

      fu.groundCooldown *= 4;
      fu.airCooldown *= 4;

      for (int i = 0; i < fu.numAttackers - 1; ++i)
        itsFriendlies.push_back(reinterpret_cast<FAPUnit<UnitExtension>&>(fu));
      itsFriendlies.emplace_back(std::forward<FAPUnit<UnitExtension>>(fu));
    }
  }

} // namespace Neolib
