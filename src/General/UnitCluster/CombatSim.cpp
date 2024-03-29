#include "UnitCluster.h"

#include <fap.h>
#include "Players.h"
#include "PathFinding.h"
#include "UnitUtil.h"
#include "General.h"

#include <chrono>

#include "DebugFlag_CombatSim.h"

#define LIMIT_MICROSECONDS 5000

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_COMBATSIM_CSV false          // Writes a CSV file for each cluster with detailed sim information
#define DEBUG_COMBATSIM_CSV_FREQUENCY 1  // Frequency at which to write CSV output
#define DEBUG_COMBATSIM_CSV_ATTACKER true // Whether to output CSV for attacker or defender
#endif

#if INSTRUMENTATION_ENABLED
#define DEBUG_COMBATSIM_DRAW false           // Draws positions for all units
#define DEBUG_COMBATSIM_DRAW_FREQUENCY 24   // Frame frequency to draw combat sim info
#define DEBUG_COMBATSIM_DRAW_ATTACKER false  // Whether to draw attacker or defender
#endif

namespace
{
    // FAP collision vectors
    // Allocated here at initialization to avoid re-allocations
    std::vector<unsigned char> collisionPlayer1;
    std::vector<unsigned char> collisionPlayer2;

    // Cache of unit scores
    int baseScore[BWAPI::UnitTypes::Enum::MAX];
    int scaledScore[BWAPI::UnitTypes::Enum::MAX];

    // Parameters
    int maxIterations;

    // Whether a unit goes into the sim
    bool isSimUnit(const Unit &unit)
    {
        if (!unit->completed) return false;
        if (unit->immobile) return false;

        if (unit->type == BWAPI::UnitTypes::Protoss_Interceptor) return false;
        if (unit->type == BWAPI::UnitTypes::Terran_Medic) return true;
        if (unit->type == BWAPI::UnitTypes::Zerg_Overlord) return true;

        return unit->groundDamage() > 0 || unit->airDamage() > 0;
    }

    auto inline makeUnit(const Unit &unit,
                         const Unit &vanguard,
                         bool mobileDetection,
                         BWAPI::Position targetPosition = BWAPI::Positions::Invalid,
                         int target = 0)
    {
        int collisionValue;
        int collisionValueChoke;
        if (unit->isFlying)
        {
            collisionValue = 0;
            collisionValueChoke = 0;
        }
        else
        {
            // In open terrain, collision values scale based on the unit range
            // Rationale: melee units have a smaller area to maneuver in, so they interfere with each other more
            int range = unit->groundRange();
            if (range > 128)
            {
                collisionValue = 3;
            }
            else if (range > 32)
            {
                collisionValue = 4;
            }
            else
            {
                collisionValue = 6;
            }

            // In a choke, collision values depend on unit size
            // We allow no collision between full-size units and a stack of two smaller-size units
            if (unit->type.width() >= 32 || unit->type.height() >= 32)
            {
                collisionValueChoke = 12;
            }
            else
            {
                collisionValueChoke = 6;
            }
        }

        return FAP::makeUnit<>()
                .setUnitType(unit->type)
                .setPosition(unit->simPosition)
                .setTargetPosition(targetPosition == BWAPI::Positions::Invalid ? unit->simPosition : targetPosition)
                .setHealth(unit->lastHealth)
                .setShields(unit->lastShields)
                .setFlying(unit->isFlying)

                        // For this next section, we have modified FAP to allow taking the upgraded values instead of the upgrade levels
                .setSpeed((float)Players::unitTopSpeed(unit->player, unit->type))
                .setArmor(Players::unitArmor(unit->player, unit->type))
                .setGroundCooldown(Players::unitGroundCooldown(unit->player, unit->type))
                .setGroundDamage(unit->groundDamage())
                .setGroundMaxRange(unit->groundRange())
                .setAirCooldown(Players::unitAirCooldown(unit->player, unit->type))
                .setAirDamage(unit->airDamage())
                .setAirMaxRange(unit->airRange())

                .setElevation(BWAPI::Broodwar->getGroundHeight(unit->simPosition.x >> 5, unit->simPosition.y >> 5))

                // TODO: Figure out if we need to do something with initializing attack cooldown for bunkers
                .setAttackerCount(unit->type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 8)
                .setAttackCooldownRemaining(std::max(0, unit->cooldownUntil - currentFrame))

                .setSpeedUpgrade(false) // Squares the speed
                .setRangeUpgrade(false) // Squares the ranges
                .setShieldUpgrades(0)

                .setStimmed(unit->stimmedUntil > currentFrame)
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

        std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
        std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
        FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);
        if (narrowChoke)
        {
            sim.setChokeGeometry(narrowChoke->tileSide,
                                 narrowChoke->end1Center,
                                 narrowChoke->end2Center,
                                 narrowChoke->end1Exit,
                                 narrowChoke->end2Exit);
        }

        bool allTierOne = true;

        // Add our units with initial target
        int myCount = 0;
        for (auto &unitAndTarget : unitsAndTargets)
        {
            if (!isSimUnit(unitAndTarget.first)) continue;

            auto target = unitAndTarget.second ? unitAndTarget.second->id : 0;
            if (attacking)
            {
                sim.addPlayer1<choke>(makeUnit(unitAndTarget.first, cluster->vanguard, false, targetPosition, target));
            }
            else
            {
                sim.addPlayer2<choke>(makeUnit(unitAndTarget.first, cluster->vanguard, false, targetPosition, target));
            }

            myCount++;
            if (unitAndTarget.first->type != BWAPI::UnitTypes::Protoss_Zealot) allTierOne = false;

#if DEBUG_COMBATSIM_CSV
            if (unitAndTarget.first->id < minUnitId) minUnitId = unitAndTarget.first->id;
#endif
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
            if (!isSimUnit(unit)) continue;
            if (unit->undetected && !haveMobileDetection) enemyHasUndetectedUnits = true;

            // Only include workers if they have been seen attacking recently
            // TODO: Handle worker rushes
            if (!unit->type.isWorker() || (currentFrame - unit->lastSeenAttacking) < 120)
            {
                if (attacking)
                {
                    sim.addPlayer2<choke>(makeUnit(unit, cluster->vanguard, haveMobileDetection));
                }
                else
                {
                    sim.addPlayer1<choke>(makeUnit(unit, cluster->vanguard, haveMobileDetection));
                }

                enemyCount++;

                if (unit->type != BWAPI::UnitTypes::Protoss_Zealot &&
                    unit->type != BWAPI::UnitTypes::Zerg_Zergling &&
                    unit->type != BWAPI::UnitTypes::Terran_Marine)
                {
                    allTierOne = false;
                }
            }
        }

#if DEBUG_COMBATSIM_CSV
        std::string simCsvLabel = (std::ostringstream() << "clustersim-" << minUnitId).str();
        std::string actualCsvLabel = (std::ostringstream() << "clustersimactuals-" << minUnitId).str();
        auto writeSimCsvLine = [&simCsvLabel](const auto &unit, int simFrame)
        {
            auto csv = Log::Csv(simCsvLabel);
            csv << currentFrame;
            csv << simFrame;
            csv << currentFrame + simFrame;
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
            csv << currentFrame;
            csv << "-";
            csv << currentFrame;
            csv << unit->type;
            csv << unit->id;
            csv << unit->lastPosition.x;
            csv << unit->lastPosition.y;
            csv << unit->lastHealth;
            csv << unit->lastShields;
            csv << std::max(0, unit->cooldownUntil - currentFrame);
        };

        if (attacking == DEBUG_COMBATSIM_CSV_ATTACKER && currentFrame % DEBUG_COMBATSIM_CSV_FREQUENCY == 0)
        {
            for (auto &unitAndTarget : unitsAndTargets)
            {
                writeActualCsvLine(unitAndTarget.first);
            }
            for (auto &target : targets)
            {
                writeActualCsvLine(target);
            }
        }
#endif
#if DEBUG_COMBATSIM_CVIS
        // Unit ID to: x, y, target, cooldown
        std::unordered_map<std::string, std::vector<int>> unitLog;
        auto updateUnitLog = [&unitLog](auto &units)
        {
            for (auto &unit : units)
            {
                auto id = std::to_string(unit.id);
                unitLog[id].push_back(unit.x);
                unitLog[id].push_back(unit.y);
                unitLog[id].push_back(unit.target);
                unitLog[id].push_back(unit.attackCooldownRemaining);
            }
        };
        updateUnitLog(*sim.getState().first);
        updateUnitLog(*sim.getState().second);
#endif

        int initialMine = score(attacking ? sim.getState().first : sim.getState().second);
        int initialEnemy = score(attacking ? sim.getState().second : sim.getState().first);

        int iterations = maxIterations;
        if (allTierOne && !attacking) iterations /= 4;

#if DEBUG_COMBATSIM_EACHFRAME
        std::vector<int> eachFrameMine;
        std::vector<int> eachFrameEnemy;
        eachFrameMine.reserve(iterations + 1);
        eachFrameEnemy.reserve(iterations + 1);
        eachFrameMine.push_back(initialMine);
        eachFrameEnemy.push_back(initialEnemy);
#endif

        auto startTime = std::chrono::high_resolution_clock::now();
        int i = 0;
        while (true)
        {
#if DEBUG_COMBATSIM_DRAW
            // Start by recording the current location and cooldown of each unit
            std::map<int, std::tuple<int, int, int>> player1DrawData;
            std::map<int, std::tuple<int, int, int>> player2DrawData;

            if (attacking == DEBUG_COMBATSIM_DRAW_ATTACKER
                && ((unitsAndTargets.size() + targets.size()) < 10 || currentFrame % DEBUG_COMBATSIM_DRAW_FREQUENCY == 0))
            {
                auto setDrawData = [](auto &simData, auto &localData)
                {
                    for (auto &unit : simData)
                    {
                        localData[unit.id] = std::make_tuple(unit.x, unit.y, unit.attackCooldownRemaining);
                    }
                };
                setDrawData(*sim.getState().first, player1DrawData);
                setDrawData(*sim.getState().second, player2DrawData);
            }
#endif

            sim.simulate<true, choke>(1);

#if DEBUG_COMBATSIM_DRAW
            if (attacking == DEBUG_COMBATSIM_DRAW_ATTACKER
                && ((unitsAndTargets.size() + targets.size()) < 10 || currentFrame % DEBUG_COMBATSIM_DRAW_FREQUENCY == 0))
            {
                auto draw = [](auto &simData, auto &localData, auto color)
                {
                    for (auto &unit : simData)
                    {
                        auto it = localData.find(unit.id);
                        if (it != localData.end() && unit.attackCooldownRemaining > std::get<2>(it->second))
                        {
                            // Attacked
                            color = CherryVis::DrawColor::Orange;
                        }
                        if (it != localData.end())
                        {
                            localData.erase(it);
                        }

                        CherryVis::drawCircle(unit.x, unit.y, 1, color, unit.id);
                    }

                    // Everything left here has died
                    for (auto &unit : localData)
                    {
                        CherryVis::drawCircle(std::get<0>(unit.second), std::get<1>(unit.second), 1, CherryVis::DrawColor::Red, unit.first);
                    }
                };
                draw(*sim.getState().first, player1DrawData, CherryVis::DrawColor::Yellow);
                draw(*sim.getState().second, player2DrawData, CherryVis::DrawColor::Cyan);
            }
#endif

#if DEBUG_COMBATSIM_CVIS
            updateUnitLog(*sim.getState().first);
            updateUnitLog(*sim.getState().second);
#endif

#if DEBUG_COMBATSIM_CSV
            if (attacking == DEBUG_COMBATSIM_CSV_ATTACKER && currentFrame % DEBUG_COMBATSIM_CSV_FREQUENCY == 0)
            {
                for (auto unit : *sim.getState().first)
                {
                    writeSimCsvLine(unit, i);
                }
                for (auto unit : *sim.getState().second)
                {
                    writeSimCsvLine(unit, i);
                }
            }
#endif

#if DEBUG_COMBATSIM_EACHFRAME
            eachFrameMine.push_back(score(attacking ? sim.getState().first : sim.getState().second));
            eachFrameEnemy.push_back(score(attacking ? sim.getState().second : sim.getState().first));
#endif

            i++;

            if (i >= iterations) break;

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
            if (duration > LIMIT_MICROSECONDS)
            {
                if (iterations < maxIterations) CherryVis::log() << "Sim aborted after " << i << "iterations";
                break;
            }
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

        return {
            myCount,
            enemyCount,
            initialMine,
            initialEnemy,
            finalMine,
            finalEnemy,
#if DEBUG_COMBATSIM_EACHFRAME
            std::move(eachFrameMine),
            std::move(eachFrameEnemy),
#endif
            enemyHasUndetectedUnits,
            narrowChoke,
#if DEBUG_COMBATSIM_CVIS
            std::move(unitLog),
#endif
        };
    }
}

namespace CombatSim
{
    void initialize()
    {
        collisionPlayer1.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);
        collisionPlayer2.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);

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

        maxIterations = 288;
    }

    int unitValue(const FAP::FAPUnit<> &unit)
    {
        return baseScore[unit.unitType] + (scaledScore[unit.unitType] * (unit.health * 3 + unit.shields)) / (unit.maxHealth * 3 + unit.maxShields);
    }

    int unitValue(const Unit &unit)
    {
        return baseScore[unit->type] +
               (scaledScore[unit->type] * (unit->lastHealth * 3 + unit->lastShields)) / (unit->type.maxHitPoints() * 3 + unit->type.maxShields());
    }

    int unitValue(BWAPI::UnitType type)
    {
        return baseScore[type] + scaledScore[type];
    }

    void setMaxIterations(int iterations)
    {
        maxIterations = iterations;
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
        return CombatSimResult{};
    }

    // Check if the armies are separated by a narrow choke
    // We consider this to be the case if our cluster center is on one side of the choke
    // and most of their units are on the other side
    Choke *narrowChoke = choke;
    if (!narrowChoke)
    {
        // First check for the next narrow choke between our center and the target position
        narrowChoke = PathFinding::SeparatingNarrowChoke(center,
                                                         targetPosition,
                                                         BWAPI::UnitTypes::Protoss_Dragoon,
                                                         PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);

        // Next validate that most of the enemy units are behind that choke
        if (narrowChoke)
        {
            int count = 0;
            int total = 0;
            for (const auto &unit : targets)
            {
                if (!isSimUnit(unit)) continue;
                if (!unit->simPositionValid) continue;

                total++;

                if (narrowChoke == PathFinding::SeparatingNarrowChoke(center,
                                                                      unit->simPosition,
                                                                      BWAPI::UnitTypes::Protoss_Dragoon,
                                                                      PathFinding::PathFindingOptions::UseNeighbouringBWEMArea))
                {
                    count++;
                }
            }
            if (count < (total - count)) narrowChoke = nullptr;
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
    if (!recentSimResults.empty() && recentSimResults.rbegin()->first.frame != currentFrame - 1)
    {
        recentSimResults.clear();
    }

    recentSimResults.emplace_back(simResult, attack);
}

void UnitCluster::addRegroupSimResult(CombatSimResult &simResult, bool contain)
{
    // Reset recent sim results if it hasn't been run on the last frame
    if (!recentRegroupSimResults.empty() && recentRegroupSimResults.rbegin()->first.frame != currentFrame - 1)
    {
        recentRegroupSimResults.clear();
    }

    recentRegroupSimResults.emplace_back(simResult, contain);
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
