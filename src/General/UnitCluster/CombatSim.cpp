#include "UnitCluster.h"

#include <fap.h>
#include "Players.h"
#include "PathFinding.h"
#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"

#if INSTRUMENTATION_ENABLED
    #define DEBUG_COMBATSIM_CSV false  // Writes a CSV file for each cluster with detailed sim information
#endif

namespace
{
    const double pi = 3.14159265358979323846;

    // Cache of unit scores
    int baseScore[BWAPI::UnitTypes::Enum::MAX];
    int scaledScore[BWAPI::UnitTypes::Enum::MAX];

    auto inline makeUnit(const Unit &unit, int target = 0)
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

        return FAP::makeUnit<>()
                .setUnitType(unit->type)
                .setPosition(unit->lastPosition)
                .setHealth(unit->lastHealth)
                .setShields(unit->lastShields)
                .setFlying(unit->isFlying)

                        // For this next section, we have modified FAP to allow taking the upgraded values instead of the upgrade levels
                .setSpeed(Players::unitTopSpeed(unit->player, unit->type))
                .setArmor(Players::unitArmor(unit->player, unit->type))
                .setGroundCooldown(Players::unitCooldown(unit->player, unit->type)
                                   / std::max(unit->type.maxGroundHits() * unit->type.groundWeapon().damageFactor(), 1))
                .setGroundDamage(groundDamage)
                .setGroundMaxRange(Players::weaponRange(unit->player, weaponType.groundWeapon()))
                .setAirDamage(airDamage)

                .setElevation(BWAPI::Broodwar->getGroundHeight(unit->lastPosition.x << 3, unit->lastPosition.y << 3))
                .setAttackerCount(unit->type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 8)
                .setAttackCooldownRemaining(std::max(0, unit->cooldownUntil - BWAPI::Broodwar->getFrameCount()))

                .setSpeedUpgrade(false) // Squares the speed
                .setRangeUpgrade(false) // Squares the ranges
                .setShieldUpgrades(0)

                .setStimmed(unit->stimmedUntil > BWAPI::Broodwar->getFrameCount())
                .setUndetected(unit->undetected)

                .setID(unit->id)
                .setTarget(target)

                .setData({});
    }

    int score(std::vector<FAP::FAPUnit<>> *units)
    {
        int result = 0;
        for (auto &unit : *units)
        {
            result +=
                    baseScore[unit.unitType] + scaledScore[unit.unitType] * (unit.health * 3 + unit.shields) / (unit.maxHealth * 3 + unit.maxShields);
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

    int unitValue(const Unit &unit)
    {
        return baseScore[unit->type]
               + scaledScore[unit->type] * (unit->lastHealth * 3 + unit->lastShields) / (unit->type.maxHitPoints() * 3 + unit->type.maxShields());
    }
}

CombatSimResult
UnitCluster::runCombatSim(std::vector<std::pair<MyUnit, Unit>> &unitsAndTargets, std::set<Unit> &targets)
{
    if (unitsAndTargets.empty() || targets.empty())
    {
        return CombatSimResult();
    }

#if DEBUG_COMBATSIM_CSV
    int minUnitId = INT_MAX;
#endif

    FAP::FastAPproximation sim;

    // Add our units with initial target
    int myGroundHeightAccumulator = 0;
    int myCount = 0;
    int myRangedCount = 0;
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto target = unitAndTarget.second ? unitAndTarget.second->id : 0;
        if (sim.addIfCombatUnitPlayer1(makeUnit(unitAndTarget.first, target)) && !unitAndTarget.first->isFlying)
        {
            myGroundHeightAccumulator += BWAPI::Broodwar->getGroundHeight(unitAndTarget.first->tilePositionX, unitAndTarget.first->tilePositionY);
            myCount++;
            if (unitAndTarget.first->type == BWAPI::UnitTypes::Protoss_Dragoon) myRangedCount++;

#if DEBUG_COMBATSIM_CSV
            if (unitAndTarget.first->id < minUnitId) minUnitId = unitAndTarget.first->id;
#endif
        }
    }

    // Add enemy units
    int enemyGroundHeightAccumulator = 0;
    int enemyPositionXAccumulator = 0;
    int enemyPositionYAccumulator = 0;
    int enemyCount = 0;
    for (auto &unit : targets)
    {
        // Only include workers if they have been seen attacking recently
        // TODO: Handle worker rushes
        if (!unit->type.isWorker() || (BWAPI::Broodwar->getFrameCount() - unit->lastSeenAttacking) < 120)
        {
            if (sim.addIfCombatUnitPlayer2(makeUnit(unit)) && !unit->isFlying)
            {
                enemyGroundHeightAccumulator += BWAPI::Broodwar->getGroundHeight(unit->tilePositionX, unit->tilePositionY);
                enemyPositionXAccumulator += unit->lastPosition.x;
                enemyPositionYAccumulator += unit->lastPosition.y;
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

    int initialMine = score(sim.getState().first);
    int initialEnemy = score(sim.getState().second);

    for (int i = 0; i < 144; i++)
    {
        sim.simulate(1);

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

    int finalMine = score(sim.getState().first);
    int finalEnemy = score(sim.getState().second);

    // Check if the armies are separated by a narrow choke
    Choke *narrowChoke = nullptr;
    double narrowChokeGain = 1.0;
    if (enemyCount > 0)
    {
        auto clusterDiameterSquared = (double)area * 4.0 / pi;
        BWAPI::Position enemyCenter(enemyPositionXAccumulator / enemyCount, enemyPositionYAccumulator / enemyCount);

        for (auto &bwemChoke : PathFinding::GetChokePointPath(center,
                                                          enemyCenter,
                                                          BWAPI::UnitTypes::Protoss_Dragoon,
                                                          PathFinding::PathFindingOptions::UseNearestBWEMArea))
        {
            auto choke = Map::choke(bwemChoke);
            auto chokeWidthSquared = choke->width * choke->width;
            if (chokeWidthSquared < clusterDiameterSquared)
            {
                // The gain is scaled from 1.0 (approximately same size) to 0.5 (cluster is 4+ times wider)
                narrowChokeGain = std::min(narrowChokeGain, 0.25 + 0.75 * std::max(0.25, (double)choke->width / sqrt(clusterDiameterSquared)));
                narrowChoke = choke;
            }
        }
    }

    // If the armies are separated by a narrow choke, compute the elevation gain
    double elevationGain = 0.0;
    if (narrowChoke && myCount > 0)
    {
        double myAvgElevation = (double)myGroundHeightAccumulator / (double)myCount;
        double enemyAvgElevation = (double)enemyGroundHeightAccumulator / (double)enemyCount;

        // If both armies are at completely different elevations, the difference will be 2
        // We scale this to give a gain between 0.5 (my army is on high ground) to -0.5 (my army is on low ground)
        elevationGain = (myAvgElevation - enemyAvgElevation) / 4.0;

        // Now we adjust for what percentage of our army is ranged
        elevationGain *= ((double)myRangedCount / (double)myCount);
    }

#if DEBUG_COMBATSIM
    std::ostringstream debug;
    debug << BWAPI::WalkPosition(center)
          << ": " << initialMine << "," << initialEnemy
          << "-" << finalMine << "," << finalEnemy;
    if (narrowChoke)
    {
        debug << "; narrowGain=" << narrowChokeGain << "; elevationGain=" << elevationGain;
    }
    CherryVis::log() << debug.str();
#endif

    return CombatSimResult(myCount, enemyCount, initialMine, initialEnemy, finalMine, finalEnemy, narrowChoke, narrowChokeGain, elevationGain);
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

CombatSimResult UnitCluster::averageRecentSimResults(int maxDepth)
{
    CombatSimResult result;
    int count = 0;
    for (auto it = recentSimResults.rbegin(); it != recentSimResults.rend() && count < maxDepth; it++)
    {
        result += it->first;
        count++;
    }

    if (count > 1) result /= count;

    return result;
}