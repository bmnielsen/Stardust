#include "UnitCluster.h"

#include <fap.h>
#include "Config.h"
#include "FileTools.h"
#include "Players.h"
#include "PathFinding.h"
#include "UnitUtil.h"
#include "General.h"

#include <chrono>

#include <sstream>

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
    /* FAP collision vectors */
    /* Allocated here at initialization to avoid re-allocations */
    std::vector<unsigned char> collisionPlayer1;
    std::vector<unsigned char> collisionPlayer2;

    /* Cache of unit scores */
    int baseScore[BWAPI::UnitTypes::Enum::MAX];
    int scaledScore[BWAPI::UnitTypes::Enum::MAX];

    /* Parameters */
    int maxIterations;

    /* Whether a unit goes into the sim */
    bool isSimUnit(const Unit &gameUnit)
    {
        if (!gameUnit->completed) return false;
        if (gameUnit->immobile) return false;

        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Interceptor) return false;
        if (gameUnit->type == BWAPI::UnitTypes::Terran_Medic) return true;
        if (gameUnit->type == BWAPI::UnitTypes::Zerg_Overlord) return true;

        return gameUnit->groundDamage() > 0 || gameUnit->airDamage() > 0;
    }

    auto inline makeUnit(const Unit &gameUnit,
                         const Unit &vanguard,
                         bool mobileDetection,
                         BWAPI::Position targetPosition = BWAPI::Positions::Invalid,
                         int target = 0)
    {
        int collisionValue;
        int collisionValueChoke;
        if (gameUnit->isFlying)
        {
            collisionValue = 0;
            collisionValueChoke = 0;
        }
        else
        {
            /* In open terrain, collision values scale based on the unit range */
            /* Rationale: melee units have a smaller area to maneuver in, so they interfere with each other more */
            int range = gameUnit->groundRange();
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

            /* In a choke, collision values depend on unit size */
            /* We allow no collision between full-size units and a stack of two smaller-size units */
            if (gameUnit->type.width() >= 32 || gameUnit->type.height() >= 32)
            {
                collisionValueChoke = 12;
            }
            else
            {
                collisionValueChoke = 6;
            }
        }

        return FAP::makeUnit<>()
                .setUnitType(gameUnit->type)
                .setPosition(gameUnit->simPosition)
                .setTargetPosition(targetPosition == BWAPI::Positions::Invalid ? gameUnit->simPosition : targetPosition)
                .setHealth(gameUnit->lastHealth)
                .setShields(gameUnit->lastShields)
                .setFlying(gameUnit->isFlying)

                        /* For this next section, we have modified FAP to allow taking the upgraded values instead of the upgrade levels */
                .setSpeed((float)PlayerInfo::unitTopSpeed(gameUnit->player, gameUnit->type))
                .setArmor(PlayerInfo::unitArmor(gameUnit->player, gameUnit->type))
                .setGroundCooldown(PlayerInfo::unitGroundCooldown(gameUnit->player, gameUnit->type))
                .setGroundDamage(gameUnit->groundDamage())
                .setGroundMaxRange(gameUnit->groundRange())
                .setAirCooldown(PlayerInfo::unitAirCooldown(gameUnit->player, gameUnit->type))
                .setAirDamage(gameUnit->airDamage())
                .setAirMaxRange(gameUnit->airRange())

                .setElevation(BWAPI::Broodwar->getGroundHeight(gameUnit->simPosition.x >> 5, gameUnit->simPosition.y >> 5))

                /* TODO: Figure out if we need to do something with initializing attack cooldown for bunkers */
                .setAttackerCount(gameUnit->type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 8)
                .setAttackCooldownRemaining(std::max(0, gameUnit->cooldownUntil - frameNow))

                .setSpeedUpgrade(false) // Squares the speed
                .setRangeUpgrade(false) // Squares the ranges
                .setShieldUpgrades(0)

                .setStimmed(gameUnit->stimmedUntil > frameNow)
                .setUndetected(gameUnit->isCliffedTank(vanguard) || (gameUnit->undetected && !mobileDetection))

                .setID(gameUnit->ident)
                .setTarget(target)

                .setCollisionValues(collisionValue, collisionValueChoke)

                .setData({});
    }

    int score(std::vector<FAP::FAPUnit<>> *units)
    {
        int outcome = 0;
        for (auto &gameUnit : *units)
        {
            outcome += CombatSim::unitValue(gameUnit);
        }

        return outcome;
    }

    template<bool choke>
    CombatSimResult execute(
            BWAPI::Position targetPosition,
            UnitCluster *clump,
            std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
            std::set<Unit> &targets,
            std::set<OwnedUnit> &detectors,
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

        /* Add our units with initial target */
        int myCount = 0;
        int myHtValue = 0;
        for (auto &unitAndTarget : unitsAndTargets)
        {
            if (!isSimUnit(unitAndTarget.first)) continue;

            /* Direction "fap_ht_value" (2026-09-30): weaponless HTs are skipped */
            /* by the sim, so storm casters read as zero value. Count their cost */
            /* value into the initial totals instead of the combat loop. */
            if (Config::fapHtValueEnabled()
                && unitAndTarget.first->type == BWAPI::UnitTypes::Protoss_High_Templar
                && unitAndTarget.first->completed)
            {
                myHtValue += UnitHelper::MineralCost(unitAndTarget.first->type)
                             + UnitHelper::GasCost(unitAndTarget.first->type) * 2;
            }

            auto target = unitAndTarget.second ? unitAndTarget.second->ident : 0;
            if (attacking)
            {
                sim.addPlayer1<choke>(makeUnit(unitAndTarget.first, clump->vanguard, false, targetPosition, target));
            }
            else
            {
                sim.addPlayer2<choke>(makeUnit(unitAndTarget.first, clump->vanguard, false, targetPosition, target));
            }

            myCount++;
            if (unitAndTarget.first->type != BWAPI::UnitTypes::Protoss_Zealot) allTierOne = false;

#if DEBUG_COMBATSIM_CSV
            if (unitAndTarget.first->ident < minUnitId) minUnitId = unitAndTarget.first->ident;
#endif
        }

        /* Determine if we have mobile detection with this cluster */
        bool haveMobileDetection = false;
        for (const auto &detector : detectors)
        {
            if (clump->vanguard && clump->vanguard->getDistance(detector) < 480)
            {
                haveMobileDetection = true;
                break;
            }
        }

        /* Add enemy units */
        int enemyCount = 0;
        int enemyHtValue = 0;
        bool enemyHasUndetectedUnits = false;
        for (auto &gameUnit : targets)
        {
            if (!isSimUnit(gameUnit)) continue;
            if (gameUnit->undetected && !haveMobileDetection) enemyHasUndetectedUnits = true;

            /* Direction "fap_ht_value" (2026-09-30): mirror of our-side counter */
            if (Config::fapHtValueEnabled()
                && gameUnit->type == BWAPI::UnitTypes::Protoss_High_Templar
                && gameUnit->completed)
            {
                enemyHtValue += UnitHelper::MineralCost(gameUnit->type)
                                + UnitHelper::GasCost(gameUnit->type) * 2;
            }

            /* Only include workers if they have been seen attacking recently */
            /* TODO: Handle worker rushes */
            if (!gameUnit->type.isWorker() || (frameNow - gameUnit->lastSeenAttacking) < 120)
            {
                if (attacking)
                {
                    sim.addPlayer2<choke>(makeUnit(gameUnit, clump->vanguard, haveMobileDetection));
                }
                else
                {
                    sim.addPlayer1<choke>(makeUnit(gameUnit, clump->vanguard, haveMobileDetection));
                }

                enemyCount++;

                if (gameUnit->type != BWAPI::UnitTypes::Protoss_Zealot &&
                    gameUnit->type != BWAPI::UnitTypes::Zerg_Zergling &&
                    gameUnit->type != BWAPI::UnitTypes::Terran_Marine)
                {
                    allTierOne = false;
                }
            }
        }

#if DEBUG_COMBATSIM_CSV
        std::string simCsvLabel = (std::ostringstream() << "clustersim-" << minUnitId).str();
        std::string actualCsvLabel = (std::ostringstream() << "clustersimactuals-" << minUnitId).str();
        auto writeSimCsvLine = [&simCsvLabel](const auto &gameUnit, int simFrame)
        {
            auto csv = AppLog::Csv(simCsvLabel);
            csv << frameNow;
            csv << simFrame;
            csv << frameNow + simFrame;
            csv << gameUnit.unitType;
            csv << gameUnit.ident;
            csv << gameUnit.x;
            csv << gameUnit.y;
            csv << gameUnit.health;
            csv << gameUnit.shields;
            csv << gameUnit.attackCooldownRemaining;
        };

        auto writeActualCsvLine = [&actualCsvLabel](const Unit &gameUnit)
        {
            auto csv = AppLog::Csv(actualCsvLabel);
            csv << frameNow;
            csv << "-";
            csv << frameNow;
            csv << gameUnit->type;
            csv << gameUnit->ident;
            csv << gameUnit->prevPos.x;
            csv << gameUnit->prevPos.y;
            csv << gameUnit->lastHealth;
            csv << gameUnit->lastShields;
            csv << std::max(0, gameUnit->cooldownUntil - frameNow);
        };

        if (attacking == DEBUG_COMBATSIM_CSV_ATTACKER && frameNow % DEBUG_COMBATSIM_CSV_FREQUENCY == 0)
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
        /* Unit ID to: x, y, target, cooldown */
        std::unordered_map<std::string, std::vector<int>> unitLog;
        auto updateUnitLog = [&unitLog](auto &units)
        {
            for (auto &gameUnit : units)
            {
                auto ident = std::to_string(gameUnit.ident);
                unitLog[ident].push_back(gameUnit.x);
                unitLog[ident].push_back(gameUnit.y);
                unitLog[ident].push_back(gameUnit.target);
                unitLog[ident].push_back(gameUnit.attackCooldownRemaining);
            }
        };
        updateUnitLog(*sim.getState().first);
        updateUnitLog(*sim.getState().second);
#endif

        int initialMine = score(attacking ? sim.getState().first : sim.getState().second);
        int initialEnemy = score(attacking ? sim.getState().second : sim.getState().first);

        /* Direction "fap_ht_value" (2026-09-30): fold the counted HT value into */
        /* the initial totals so engagement gates see storm casters as value at */
        /* risk instead of free food; log the first adjustment of the game */
        if (Config::fapHtValueEnabled() && (myHtValue || enemyHtValue))
        {
            initialMine += myHtValue;
            initialEnemy += enemyHtValue;

            static int loggedFrame = -1;
            if (loggedFrame < 0)
            {
                loggedFrame = frameNow;
                FileTools::appendLine("shushanking_htpipe",
                                      (std::ostringstream() << "HTPIPE FAPHT frame=" << frameNow
                                       << " myAdj=" << myHtValue
                                       << " enemyAdj=" << enemyHtValue).str());
            }
        }

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
            /* Start by recording the current location and cooldown of each unit */
            std::map<int, std::tuple<int, int, int>> player1DrawData;
            std::map<int, std::tuple<int, int, int>> player2DrawData;

            if (attacking == DEBUG_COMBATSIM_DRAW_ATTACKER
                && ((unitsAndTargets.size() + targets.size()) < 10 || frameNow % DEBUG_COMBATSIM_DRAW_FREQUENCY == 0))
            {
                auto setDrawData = [](auto &simData, auto &localData)
                {
                    for (auto &gameUnit : simData)
                    {
                        localData[gameUnit.ident] = std::make_tuple(gameUnit.x, gameUnit.y, gameUnit.attackCooldownRemaining);
                    }
                };
                setDrawData(*sim.getState().first, player1DrawData);
                setDrawData(*sim.getState().second, player2DrawData);
            }
#endif

            sim.simulate<true, choke>(1);

#if DEBUG_COMBATSIM_DRAW
            if (attacking == DEBUG_COMBATSIM_DRAW_ATTACKER
                && ((unitsAndTargets.size() + targets.size()) < 10 || frameNow % DEBUG_COMBATSIM_DRAW_FREQUENCY == 0))
            {
                auto draw = [](auto &simData, auto &localData, auto color)
                {
                    for (auto &gameUnit : simData)
                    {
                        auto iterRef = localData.find(gameUnit.ident);
                        if (iterRef != localData.end() && gameUnit.attackCooldownRemaining > std::get<2>(iterRef->second))
                        {
                            /* Attacked */
                            color = Telemetry::DrawColor::Orange;
                        }
                        if (iterRef != localData.end())
                        {
                            localData.erase(iterRef);
                        }

                        Telemetry::drawCircle(gameUnit.x, gameUnit.y, 1, color, gameUnit.ident);
                    }

                    /* Everything left here has died */
                    for (auto &gameUnit : localData)
                    {
                        Telemetry::drawCircle(std::get<0>(gameUnit.second), std::get<1>(gameUnit.second), 1, Telemetry::DrawColor::Red, gameUnit.first);
                    }
                };
                draw(*sim.getState().first, player1DrawData, Telemetry::DrawColor::Yellow);
                draw(*sim.getState().second, player2DrawData, Telemetry::DrawColor::Cyan);
            }
#endif

#if DEBUG_COMBATSIM_CVIS
            updateUnitLog(*sim.getState().first);
            updateUnitLog(*sim.getState().second);
#endif

#if DEBUG_COMBATSIM_CSV
            if (attacking == DEBUG_COMBATSIM_CSV_ATTACKER && frameNow % DEBUG_COMBATSIM_CSV_FREQUENCY == 0)
            {
                for (auto gameUnit : *sim.getState().first)
                {
                    writeSimCsvLine(gameUnit, i);
                }
                for (auto gameUnit : *sim.getState().second)
                {
                    writeSimCsvLine(gameUnit, i);
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
                if (iterations < maxIterations) Telemetry::log() << "Sim aborted after " << i << "iterations";
                break;
            }
        }

        int finalMine = score(attacking ? sim.getState().first : sim.getState().second);
        int finalEnemy = score(attacking ? sim.getState().second : sim.getState().first);

#if DEBUG_COMBATSIM_LOG
        std::ostringstream debug;
        debug << BWAPI::WalkPosition(clump->midpoint);
        if (!attacking && narrowChoke)
        {
            debug << " (defend " << BWAPI::WalkPosition(narrowChoke->midpoint) << ")";
        }
        else if (narrowChoke)
        {
            debug << " (through " << BWAPI::WalkPosition(narrowChoke->midpoint) << ")";
        }
        debug << ": " << initialMine << "," << initialEnemy
              << "-" << finalMine << "," << finalEnemy;
        Telemetry::log() << debug.str();
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
            /* Base the score on the cost */
            int score = UnitHelper::MineralCost(type) + UnitHelper::GasCost(type) * 2;

            /* Add cost of built / loaded units */
            if (type == BWAPI::UnitTypes::Terran_Bunker)
            {
                score += 4 * UnitHelper::MineralCost(BWAPI::UnitTypes::Terran_Marine);
            }
            else if (type == BWAPI::UnitTypes::Protoss_Carrier)
            {
                score += 8 * UnitHelper::MineralCost(BWAPI::UnitTypes::Protoss_Interceptor);
            }

            /* Adjust units whose combat value differs from their cost */
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

    int unitValue(const FAP::FAPUnit<> &gameUnit)
    {
        return baseScore[gameUnit.unitType] + (scaledScore[gameUnit.unitType] * (gameUnit.health * 3 + gameUnit.shields)) / (gameUnit.maxHealth * 3 + gameUnit.maxShields);
    }

    int unitValue(const Unit &gameUnit)
    {
        return baseScore[gameUnit->type] +
               (scaledScore[gameUnit->type] * (gameUnit->lastHealth * 3 + gameUnit->lastShields)) / (gameUnit->type.maxHitPoints() * 3 + gameUnit->type.maxShields());
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
                                          std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets,
                                          std::set<Unit> &targets,
                                          std::set<OwnedUnit> &detectors,
                                          bool attacking,
                                          Choke *choke)
{
    if (unitsAndTargets.empty() || targets.empty())
    {
        return CombatSimResult{};
    }

    /* Check if the armies are separated by a narrow choke */
    /* We consider this to be the case if our cluster center is on one side of the choke */
    /* and most of their units are on the other side */
    Choke *narrowChoke = choke;
    if (!narrowChoke)
    {
        /* First check for the next narrow choke between our center and the target position */
        narrowChoke = Pathfinder::SeparatingNarrowChoke(midpoint,
                                                         targetPosition,
                                                         BWAPI::UnitTypes::Protoss_Dragoon,
                                                         Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);

        /* Next validate that most of the enemy units are behind that choke */
        if (narrowChoke)
        {
            int count = 0;
            int total = 0;
            for (const auto &gameUnit : targets)
            {
                if (!isSimUnit(gameUnit)) continue;
                if (!gameUnit->simPositionValid) continue;

                total++;

                if (narrowChoke == Pathfinder::SeparatingNarrowChoke(midpoint,
                                                                      gameUnit->simPosition,
                                                                      BWAPI::UnitTypes::Protoss_Dragoon,
                                                                      Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea))
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
    /* Reset recent sim results if it hasn't been run on the last frame */
    if (!recentSimResults.empty() && recentSimResults.rbegin()->first.tick != frameNow - 1)
    {
        recentSimResults.clear();
    }

    recentSimResults.emplace_back(simResult, attack);
}

void UnitCluster::addRegroupSimResult(CombatSimResult &simResult, bool contain)
{
    /* Reset recent sim results if it hasn't been run on the last frame */
    if (!recentRegroupSimResults.empty() && recentRegroupSimResults.rbegin()->first.tick != frameNow - 1)
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
    for (auto iterRef = simResults.rbegin(); iterRef != simResults.rend() && count < limit; iterRef++)
    {
        if (isConsecutive)
        {
            if (iterRef->second == firstResult)
            {
                consecutive++;
            }
            else
            {
                isConsecutive = false;
            }
        }

        if (iterRef->second)
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
