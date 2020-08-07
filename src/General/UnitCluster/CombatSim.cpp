#include "UnitCluster.h"

#include <fap.h>
#include "Players.h"
#include "PathFinding.h"
#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "General.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_COMBATSIM_CSV false  // Writes a CSV file for each cluster with detailed sim information
#endif

namespace
{
    // Cache of unit scores
    int baseScore[BWAPI::UnitTypes::Enum::MAX];
    int scaledScore[BWAPI::UnitTypes::Enum::MAX];

    auto inline makeUnit(const Unit &unit, bool mobileDetection, int target = 0)
    {
        BWAPI::UnitType weaponType;
        switch (unit->type)
        {
            case BWAPI::UnitTypes::Protoss_Carrier:
                weaponType = BWAPI::UnitTypes::Protoss_Interceptor;
                break;
            case BWAPI::UnitTypes::Terran_Bunker:
                weaponType = BWAPI::UnitTypes::Terran_Marine;
                break;
            case BWAPI::UnitTypes::Protoss_Reaver:
                weaponType = BWAPI::UnitTypes::Protoss_Scarab;
                break;
            default:
                weaponType = unit->type;
                break;
        }

        int groundDamage = Players::weaponDamage(unit->player, weaponType.groundWeapon());
        int airDamage = Players::weaponDamage(unit->player, weaponType.airWeapon());
        if ((unit->burrowed && unit->type != BWAPI::UnitTypes::Zerg_Lurker) ||
            (!unit->burrowed && unit->type == BWAPI::UnitTypes::Zerg_Lurker))
        {
            groundDamage = airDamage = 0;
        }

        // In open terrain, collision values scale based on the unit range
        // Rationale: melee units have a smaller area to maneuver in, so they interfere with each other more
        int collisionValue = unit->isFlying ? 0 : std::max(0, 6 - (unit->groundRange() / 32));

        // In a choke, collision values depend on unit size
        // We allow no collision between full-size units and a stack of two smaller-size units
        int collisionValueChoke = unit->isFlying ? 0 : 6;
        if (unit->type.width() >= 32 || unit->type.height() >= 32) collisionValue = 12;

        return FAP::makeUnit<>()
                .setUnitType(unit->type)
                .setPosition(unit->lastPosition)
                .setHealth(unit->lastHealth)
                .setShields(unit->lastShields)
                .setFlying(unit->isFlying)

                        // For this next section, we have modified FAP to allow taking the upgraded values instead of the upgrade levels
                .setSpeed(Players::unitTopSpeed(unit->player, unit->type))
                .setArmor(Players::unitArmor(unit->player, unit->type))
                .setGroundCooldown(Players::unitCooldown(unit->player, weaponType)
                                   / std::max(weaponType.maxGroundHits() * weaponType.groundWeapon().damageFactor(), 1))
                .setGroundDamage(groundDamage)
                .setGroundMaxRange(unit->groundRange())
                .setAirCooldown(Players::unitCooldown(unit->player, weaponType)
                                / std::max(weaponType.maxAirHits() * weaponType.airWeapon().damageFactor(), 1))
                .setAirDamage(airDamage)
                .setAirMaxRange(unit->airRange())

                .setElevation(BWAPI::Broodwar->getGroundHeight(unit->lastPosition.x << 3, unit->lastPosition.y << 3))
                .setAttackerCount(unit->type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 8)
                .setAttackCooldownRemaining(std::max(0, unit->cooldownUntil - BWAPI::Broodwar->getFrameCount()))

                .setSpeedUpgrade(false) // Squares the speed
                .setRangeUpgrade(false) // Squares the ranges
                .setShieldUpgrades(0)

                .setStimmed(unit->stimmedUntil > BWAPI::Broodwar->getFrameCount())
                .setUndetected(mobileDetection ? false : unit->undetected)

                .setID(unit->id)
                .setTarget(target)

                .setCollisionValues(collisionValue, collisionValueChoke)

                .setData({});
    }

    int score(std::vector<FAP::FAPUnit<>> *units)
    {
        int result = 0;
        for (auto &unit : *units)
        {
            result += CombatSim::unitValue(unit);
        }

        return result;
    }
}

namespace CombatSim
{
    void initialize()
    {
        // TODO: This only considers economic value, should we consider effectiveness as well?
        for (auto type : BWAPI::UnitTypes::allUnitTypes())
        {
            int score = UnitUtil::MineralCost(type) + UnitUtil::GasCost(type) * 2;

            if (type == BWAPI::UnitTypes::Terran_Bunker)
            {
                score += 4 * UnitUtil::MineralCost(BWAPI::UnitTypes::Terran_Marine);
            }

            baseScore[type] = score >> 2U;
            scaledScore[type] = score - baseScore[type];
        }
    }

    int unitValue(const FAP::FAPUnit<> &unit)
    {
        return baseScore[unit.unitType] + scaledScore[unit.unitType] * (unit.health * 3 + unit.shields) / (unit.maxHealth * 3 + unit.maxShields);
    }

    int unitValue(const Unit &unit)
    {
        return baseScore[unit->type] +
               scaledScore[unit->type] * (unit->lastHealth * 3 + unit->lastShields) / (unit->type.maxHitPoints() * 3 + unit->type.maxShields());
    }

    int unitValue(BWAPI::UnitType type)
    {
        return baseScore[type] + scaledScore[type];
    }
}

CombatSimResult UnitCluster::runCombatSim(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                                          std::set<Unit> &targets,
                                          std::set<MyUnit> &detectors,
                                          bool attacking,
                                          Choke *choke)
{
    if (unitsAndTargets.empty() || targets.empty())
    {
        return CombatSimResult();
    }

#if DEBUG_COMBATSIM_CSV
    int minUnitId = INT_MAX;
#endif

    // Check if the armies are separated by a narrow choke
    Choke *narrowChoke = choke;
    if (!narrowChoke)
    {
        // Start by computing the enemy army center
        int enemyPositionXAccumulator = 0;
        int enemyPositionYAccumulator = 0;
        for (const auto &unit : targets)
        {
            enemyPositionXAccumulator += unit->lastPosition.x;
            enemyPositionYAccumulator += unit->lastPosition.y;
        }
        BWAPI::Position enemyCenter(enemyPositionXAccumulator / targets.size(), enemyPositionYAccumulator / targets.size());

        // Now find a narrow choke on the path between the armies
        for (auto &bwemChoke : PathFinding::GetChokePointPath(center,
                                                              enemyCenter,
                                                              BWAPI::UnitTypes::Protoss_Dragoon,
                                                              PathFinding::PathFindingOptions::UseNearestBWEMArea))
        {
            auto thisChoke = Map::choke(bwemChoke);
            if (thisChoke->isNarrowChoke)
            {
                narrowChoke = thisChoke;
                break;
            }
        }
    }

    FAP::FastAPproximation sim;
    if (narrowChoke)
    {
        sim.setChokeGeometry(narrowChoke->tileSide,
                             narrowChoke->end1Center,
                             narrowChoke->end2Center,
                             narrowChoke->end1Exit,
                             narrowChoke->end2Exit);
    }

    // Add our units with initial target
    int myCount = 0;
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto target = unitAndTarget.second ? unitAndTarget.second->id : 0;
        bool added = attacking
                     ? sim.addIfCombatUnitPlayer1(makeUnit(unitAndTarget.first, false, target))
                     : sim.addIfCombatUnitPlayer2(makeUnit(unitAndTarget.first, false, target));

        if (added)
        {
            myCount++;

#if DEBUG_COMBATSIM_CSV
            if (unitAndTarget.first->id < minUnitId) minUnitId = unitAndTarget.first->id;
#endif
        }
    }

    // Determine if we have mobile detection with this cluster
    bool haveMobileDetection = false;
    for (const auto &detector : detectors)
    {
        if (vanguard && vanguard->getDistance(detector) < 640)
        {
            haveMobileDetection = true;
            break;
        }
    }

    // Add enemy units
    int enemyCount = 0;
    for (auto &unit : targets)
    {
        if (!unit->completed) continue;

        // Only include workers if they have been seen attacking recently
        // TODO: Handle worker rushes
        if (!unit->type.isWorker() || (BWAPI::Broodwar->getFrameCount() - unit->lastSeenAttacking) < 120)
        {
            bool added = attacking
                         ? sim.addIfCombatUnitPlayer2(makeUnit(unit, haveMobileDetection))
                         : sim.addIfCombatUnitPlayer1(makeUnit(unit, haveMobileDetection));

            if (added)
            {
                enemyCount++;
            }
        }
    }

#if DEBUG_COMBATSIM_CSV
    std::string simCsvLabel = (std::ostringstream() << "clustersim-" << minUnitId).str();
    std::string actualCsvLabel = (std::ostringstream() << "clustersimactuals-" << minUnitId).str();
    auto writeSimCsvLine = [&simCsvLabel](const auto &unit, int simFrame)
    {
        auto csv = Log::Csv(simCsvLabel);
        csv << BWAPI::Broodwar->getFrameCount();
        csv << simFrame;
        csv << BWAPI::Broodwar->getFrameCount() + simFrame;
        csv << unit.unitType;
        csv << unit.id;
        csv << unit.x;
        csv << unit.y;
        csv << unit.health;
        csv << unit.shields;
        csv << unit.attackCooldownRemaining;
    };

    auto writeActualCsvLine = [&actualCsvLabel](const Unit &unit)
    {
        auto csv = Log::Csv(actualCsvLabel);
        csv << BWAPI::Broodwar->getFrameCount();
        csv << "-";
        csv << BWAPI::Broodwar->getFrameCount();
        csv << unit->type;
        csv << unit->id;
        csv << unit->lastPosition.x;
        csv << unit->lastPosition.y;
        csv << unit->lastHealth;
        csv << unit->lastShields;
        csv << std::max(0, unit->cooldownUntil - BWAPI::Broodwar->getFrameCount());
    };

    for (auto &unitAndTarget : unitsAndTargets)
    {
        writeActualCsvLine(unitAndTarget.first);
    }
    for (auto &target : targets)
    {
        writeActualCsvLine(target);
    }
#endif

    int initialMine = score(attacking ? sim.getState().first : sim.getState().second);
    int initialEnemy = score(attacking ? sim.getState().second : sim.getState().first);

    for (int i = 0; i < 144; i++)
    {
        sim.simulate(1);

        // If nothing has happened after simming for three seconds, break now
        if (i == 72)
        {
            int halfwayMine = score(attacking ? sim.getState().first : sim.getState().second);
            int halfwayEnemy = score(attacking ? sim.getState().second : sim.getState().first);
            if (halfwayMine >= initialMine && halfwayEnemy >= initialEnemy)
            {
                return CombatSimResult(myCount, enemyCount, initialMine, initialEnemy, halfwayMine, halfwayEnemy, narrowChoke);
            }
        }

#if DEBUG_COMBATSIM_CSV
        for (auto unit : *sim.getState().first)
        {
            writeSimCsvLine(unit, i);
        }
        for (auto unit : *sim.getState().second)
        {
            writeSimCsvLine(unit, i);
        }
#endif
    }

    int finalMine = score(attacking ? sim.getState().first : sim.getState().second);
    int finalEnemy = score(attacking ? sim.getState().second : sim.getState().first);

#if DEBUG_COMBATSIM
    std::ostringstream debug;
    debug << BWAPI::WalkPosition(center);
    if (!attacking && narrowChoke)
    {
        debug << " (defend " << BWAPI::WalkPosition(narrowChoke->center) << ")";
    }
    else if (narrowChoke)
    {
        debug << " (through " << BWAPI::WalkPosition(narrowChoke->center) << ")";
    }
    debug << ": " << initialMine << "," << initialEnemy
          << "-" << finalMine << "," << finalEnemy;
    CherryVis::log() << debug.str();
#endif

    return CombatSimResult(myCount, enemyCount, initialMine, initialEnemy, finalMine, finalEnemy, narrowChoke);
}

void UnitCluster::addSimResult(CombatSimResult &simResult, bool attack)
{
    // Reset recent sim results if it hasn't been run on the last frame
    if (!recentSimResults.empty() && recentSimResults.rbegin()->first.frame != BWAPI::Broodwar->getFrameCount() - 1)
    {
        recentSimResults.clear();
    }

    recentSimResults.emplace_back(std::make_pair(simResult, attack));
}

void UnitCluster::addRegroupSimResult(CombatSimResult &simResult, bool contain)
{
    // Reset recent sim results if it hasn't been run on the last frame
    if (!recentRegroupSimResults.empty() && recentRegroupSimResults.rbegin()->first.frame != BWAPI::Broodwar->getFrameCount() - 1)
    {
        recentRegroupSimResults.clear();
    }

    recentRegroupSimResults.emplace_back(std::make_pair(simResult, contain));
}
