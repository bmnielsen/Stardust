#include "UnitCluster.h"

#include <fap.h>
#include "Players.h"
#include "PathFinding.h"
#include "UnitUtil.h"
#include "General.h"

#include "DebugFlag_CombatSim.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_COMBATSIM_CSV  false // Writes a CSV file for each cluster with detailed sim information
#define DEBUG_COMBATSIM_DRAW false // Draws positions for all units
#endif

namespace
{
    // Cache of unit scores
    int baseScore[BWAPI::UnitTypes::Enum::MAX];
    int scaledScore[BWAPI::UnitTypes::Enum::MAX];

    auto inline makeUnit(const Unit &unit,
                         const Unit &vanguard,
                         bool mobileDetection,
                         BWAPI::Position targetPosition = BWAPI::Positions::Invalid,
                         int target = 0)
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
                .setPosition(unit->simPosition)
                .setTargetPosition(targetPosition == BWAPI::Positions::Invalid ? unit->simPosition : targetPosition)
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

                        // Uses lastPosition since we don't know if simPosition is walkable
                .setElevation(BWAPI::Broodwar->getGroundHeight(unit->lastPosition.x >> 5, unit->lastPosition.y >> 5))

                .setAttackerCount(unit->type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 8)
                .setAttackCooldownRemaining(std::max(0, unit->cooldownUntil - BWAPI::Broodwar->getFrameCount()))

                .setSpeedUpgrade(false) // Squares the speed
                .setRangeUpgrade(false) // Squares the ranges
                .setShieldUpgrades(0)

                .setStimmed(unit->stimmedUntil > BWAPI::Broodwar->getFrameCount())
                .setUndetected(unit->isCliffedTank(vanguard) || (unit->undetected && !mobileDetection))

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

    template<bool choke>
    CombatSimResult execute(
            BWAPI::Position targetPosition,
            UnitCluster *cluster,
            std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
            std::set<Unit> &targets,
            std::set<MyUnit> &detectors,
            bool attacking,
            Choke *narrowChoke = nullptr)
    {
#if DEBUG_COMBATSIM_CSV
        int minUnitId = INT_MAX;
#endif

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
            if (unitAndTarget.first->immobile) continue;

            auto target = unitAndTarget.second ? unitAndTarget.second->id : 0;
            bool added = attacking
                         ? sim.addIfCombatUnitPlayer1<choke>(makeUnit(unitAndTarget.first, cluster->vanguard, false, targetPosition, target))
                         : sim.addIfCombatUnitPlayer2<choke>(makeUnit(unitAndTarget.first, cluster->vanguard, false, targetPosition, target));

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
            if (cluster->vanguard && cluster->vanguard->getDistance(detector) < 480)
            {
                haveMobileDetection = true;
                break;
            }
        }

        // Add enemy units
        int enemyCount = 0;
        bool enemyHasUndetectedUnits = false;
        for (auto &unit : targets)
        {
            if (!unit->completed) continue;
            if (unit->immobile) continue;
            if (unit->undetected && !haveMobileDetection) enemyHasUndetectedUnits = true;

            // Only include workers if they have been seen attacking recently
            // TODO: Handle worker rushes
            if (!unit->type.isWorker() || (BWAPI::Broodwar->getFrameCount() - unit->lastSeenAttacking) < 120)
            {
                bool added = attacking
                             ? sim.addIfCombatUnitPlayer2<choke>(makeUnit(unit, cluster->vanguard, haveMobileDetection))
                             : sim.addIfCombatUnitPlayer1<choke>(makeUnit(unit, cluster->vanguard, haveMobileDetection));

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
#if DEBUG_COMBATSIM_DRAW
            std::map<int, std::tuple<int, int, int>> player1DrawData;
            std::map<int, std::tuple<int, int, int>> player2DrawData;
            auto setDrawData = [](auto &simData, auto &localData)
            {
                for (auto &unit : simData)
                {
                    localData[unit.id] = std::make_tuple(unit.x, unit.y, unit.attackCooldownRemaining);
                }
            };
            setDrawData(*sim.getState().first, player1DrawData);
            setDrawData(*sim.getState().second, player2DrawData);
#endif

            sim.simulate<true, choke>(1);

#if DEBUG_COMBATSIM_DRAW
            auto draw = [](auto &simData, auto &localData)
            {
                for (auto &unit : simData)
                {
                    auto color = CherryVis::DrawColor::Yellow;

                    auto it = localData.find(unit.id);
                    if (it != localData.end() && unit.attackCooldownRemaining > std::get<2>(it->second))
                    {
                        color = CherryVis::DrawColor::Cyan;
                    }
                    if (it != localData.end())
                    {
                        localData.erase(it);
                    }

                    CherryVis::drawCircle(unit.x, unit.y, 1, color);
                }

                for (auto &unit : localData)
                {
                    CherryVis::drawCircle(std::get<0>(unit.second), std::get<1>(unit.second), 1, CherryVis::DrawColor::Red);
                }
            };
            draw(*sim.getState().first, player1DrawData);
            draw(*sim.getState().second, player2DrawData);
#endif

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

#if DEBUG_COMBATSIM_LOG
        std::ostringstream debug;
        debug << BWAPI::WalkPosition(cluster->center);
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

        return CombatSimResult(myCount, enemyCount, initialMine, initialEnemy, finalMine, finalEnemy, enemyHasUndetectedUnits, narrowChoke);
    }
}

namespace CombatSim
{
    void initialize()
    {
        for (auto type : BWAPI::UnitTypes::allUnitTypes())
        {
            // Base the score on the cost
            int score = UnitUtil::MineralCost(type) + UnitUtil::GasCost(type) * 2;

            // Add cost of built / loaded units
            if (type == BWAPI::UnitTypes::Terran_Bunker)
            {
                score += 4 * UnitUtil::MineralCost(BWAPI::UnitTypes::Terran_Marine);
            }
            else if (type == BWAPI::UnitTypes::Protoss_Carrier)
            {
                score += 8 * UnitUtil::MineralCost(BWAPI::UnitTypes::Protoss_Interceptor);
            }

            // Adjust units whose combat value differs from their cost
            if (type == BWAPI::UnitTypes::Zerg_Sunken_Colony)
            {
                score += 125;
            }
            else if (type == BWAPI::UnitTypes::Zerg_Spore_Colony)
            {
                score += 100;
            }
            else if (type == BWAPI::UnitTypes::Zerg_Creep_Colony)
            {
                score += 50;
            }
            else if (type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
            {
                score += 100;
            }
            else if (type == BWAPI::UnitTypes::Terran_Missile_Turret)
            {
                score += 150;
            }
            else if (type == BWAPI::UnitTypes::Protoss_Shuttle)
            {
                score += 200;
            }
            else if (type == BWAPI::UnitTypes::Terran_Dropship)
            {
                score += 200;
            }
            else if (type == BWAPI::UnitTypes::Zerg_Zergling)
            {
                score += 15;
            }
            else if (type == BWAPI::UnitTypes::Terran_Vulture)
            {
                score += 50;
            }
            else if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
            {
                score += 100;
            }
            else if (type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
            {
                score += 100;
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

CombatSimResult UnitCluster::runCombatSim(BWAPI::Position targetPosition,
                                          std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets,
                                          std::set<Unit> &targets,
                                          std::set<MyUnit> &detectors,
                                          bool attacking,
                                          Choke *choke)
{
    if (unitsAndTargets.empty() || targets.empty())
    {
        return CombatSimResult();
    }

    // Check if the armies are separated by a narrow choke
    // We consider this to be the case if our cluster center is on one side of the choke and their furthest unit is on the other side
    Choke *narrowChoke = choke;
    if (!narrowChoke)
    {
        auto furthest = BWAPI::Positions::Invalid;
        auto furthestDist = 0;
        for (const auto &unit : targets)
        {
            if (!unit->completed) continue;
            if (!unit->simPositionValid) continue;

            int dist = unit->getDistance(center);
            if (dist > furthestDist)
            {
                furthestDist = dist;
                furthest = unit->simPosition;
            }
        }

        if (furthest.isValid())
        {
            narrowChoke = PathFinding::SeparatingNarrowChoke(center,
                                                             furthest,
                                                             BWAPI::UnitTypes::Protoss_Dragoon,
                                                             PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
        }
    }

    if (narrowChoke)
    {
        return execute<true>(targetPosition, this, unitsAndTargets, targets, detectors, attacking, narrowChoke);
    }

    return execute<false>(targetPosition, this, unitsAndTargets, targets, detectors, attacking);
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

int UnitCluster::consecutiveSimResults(std::deque<std::pair<CombatSimResult, bool>> &simResults,
                                       int *attack,
                                       int *regroup,
                                       int limit)
{
    *attack = 0;
    *regroup = 0;

    if (simResults.empty()) return 0;

    bool firstResult = simResults.rbegin()->second;
    bool isConsecutive = true;
    int consecutive = 0;
    int count = 0;
    for (auto it = simResults.rbegin(); it != simResults.rend() && count < limit; it++)
    {
        if (isConsecutive)
        {
            if (it->second == firstResult)
            {
                consecutive++;
            }
            else
            {
                isConsecutive = false;
            }
        }

        if (it->second)
        {
            (*attack)++;
        }
        else
        {
            (*regroup)++;
        }

        count++;
    }

    return consecutive;
}
