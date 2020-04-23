#include "DefendMyMain.h"

#include "Map.h"
#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Workers.h"

/*
 * Thoughts about main base defense.
 *
 * Make it a decision at the strategy engine level on whether the main army participates in defending or not
 * Make it a decision at the strategy engine level what to produce
 * Play defines when and how to pull workers
 * Squad performs actual micro
 */


#define DEBUG_PLAY_STATE false

/*
 * Main base defense:
 * - Initially keep some zealots for safety until we have scouting information
 * - If under pressure, produce infinite zealots -> triggers Producer to remove workers from gas
 * - Workers do worker defense if enemy melee units enter the mineral line
 */

namespace
{
    const int REGROUP_EMERGENCY_TIMEOUT = 30 * 24;
}

DefendMyMain::DefendMyMain()
        : emergencyProduction(BWAPI::UnitTypes::None)
        , squad(std::make_shared<DefendBaseSquad>(Map::getMyMain()))
        , lastRegroupFrame(0)
{
    General::addSquad(squad);
}

void DefendMyMain::update()
{
    // Get enemy combat units in our base
    std::set<Unit> enemyCombatUnits;
    std::set<Unit> enemyWorkers;
    bool enemyFlyingUnit = false;
    bool gasSteal = false;
    for (const Unit &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != Map::getMyMain()->getArea()) continue;

        if (unit->type.isWorker())
        {
            enemyWorkers.insert(unit);
            if (unit->lastSeenAttacking > (BWAPI::Broodwar->getFrameCount() - 48)) enemyCombatUnits.insert(unit);
        }
        else if (UnitUtil::IsCombatUnit(unit->type) && unit->type.canAttack())
        {
            enemyCombatUnits.insert(unit);
            if (unit->isFlying) enemyFlyingUnit = true;
        }
        else if (unit->type.isRefinery())
        {
            gasSteal = true;
        }
    }

    // If there are more than two workers, consider this to be a worker rush and add them to the set of combat units
    if (enemyWorkers.size() > 2)
    {
        enemyCombatUnits.insert(enemyWorkers.begin(), enemyWorkers.end());
    }

    mineralLineWorkerDefense(enemyCombatUnits);

    // Keep track of when the squad was last regrouping, considering an empty squad to be regrouping
    if (!enemyCombatUnits.empty() && (squad->getUnits().empty() || squad->hasClusterWithActivity(UnitCluster::Activity::Regrouping)))
    {
        lastRegroupFrame = BWAPI::Broodwar->getFrameCount();
    }

    // If the squad has been regrouping recently, consider this an emergency
    emergencyProduction = BWAPI::UnitTypes::None;
    if (lastRegroupFrame > 0 && lastRegroupFrame > (BWAPI::Broodwar->getFrameCount() - REGROUP_EMERGENCY_TIMEOUT))
    {
        emergencyProduction = enemyFlyingUnit ? BWAPI::UnitTypes::Protoss_Dragoon : BWAPI::UnitTypes::Protoss_Zealot;

#if DEBUG_PLAY_STATE
        CherryVis::log() << "Defend base: emergency, producing " << emergencyProduction;
#endif
        return;
    }

    // Get at least one zealot if the enemy is doing a gas steal
    if (gasSteal && squad->getUnits().empty())
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Zealot, squad->getTargetPosition());

#if DEBUG_PLAY_STATE
        CherryVis::log() << "Defend base: gas steal, ordering one zealot";
#endif
    }
}

void DefendMyMain::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // If we have an emergency production type, produce an infinite number of them off two gateways
    if (emergencyProduction != BWAPI::UnitTypes::None)
    {
        prioritizedProductionGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>, emergencyProduction, -1, 2);
        return;
    }

    // Otherwise produce to fulfill our unit requirements
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                    unitRequirement.type,
                                                    unitRequirement.count,
                                                    (unitRequirement.count + 1) / 2);
    }
}

void DefendMyMain::mineralLineWorkerDefense(std::set<Unit> &enemiesInBase)
{
    // Check if there are enemy melee units in our mineral line
    std::set<Unit> enemyUnits;
    Units::enemy(enemyUnits, [](const Unit &unit)
    {
        if (!unit->lastPositionValid) return false;
        if (!UnitUtil::IsCombatUnit(unit->type) || UnitUtil::IsRangedUnit(unit->type) || !UnitUtil::CanAttackGround(unit->type)) return false;

        auto comingPosition = unit->predictPosition(24);
        return comingPosition.isValid() && Map::isInOwnMineralLine(BWAPI::TilePosition(comingPosition));
    });

    if (enemyUnits.empty())
    {
        if (!reservedWorkers.empty())
        {
            for (const auto &worker : reservedWorkers)
            {
                Workers::releaseWorker(worker);
            }

            reservedWorkers.clear();
        }

        return;
    }

    // Prune dead workers from the list
    for (auto it = reservedWorkers.begin(); it != reservedWorkers.end();)
    {
        if ((*it)->exists())
        {
            it++;
        }
        else
        {
            it = reservedWorkers.erase(it);
        }
    }

    // Grab all of the base's workers
    Workers::reserveBaseWorkers(reservedWorkers, Map::getMyMain());

    // Choose a target for each of our workers
    // Each attacks the closest enemy unit with the least health
    bool isEnemyInRange = false;
    std::vector<std::pair<MyUnit, Unit>> workersAndTargets;
    for (const auto &worker : reservedWorkers)
    {
        Unit bestTarget = nullptr;
        int bestTargetDist = INT_MAX;
        int bestTargetHealth = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            int dist = worker->getDistance(enemy);
            if (dist < bestTargetDist || (dist == bestTargetDist && enemy->lastHealth < bestTargetHealth))
            {
                bestTarget = enemy;
                bestTargetDist = dist;
                bestTargetHealth = enemy->lastHealth;
            }
        }

        workersAndTargets.emplace_back(std::make_pair(worker, bestTarget));

        auto predictedPosition = bestTarget->predictPosition(BWAPI::Broodwar->getLatencyFrames());
        if (worker->isInOurWeaponRange(bestTarget, predictedPosition) ||
            worker->isInEnemyWeaponRange(bestTarget, predictedPosition))
        {
            isEnemyInRange = true;
        }
    }

    // Consider if any combat units are in range of an enemy
    if (!isEnemyInRange && squad)
    {
        for (const auto &unit : squad->getUnits())
        {
            for (auto &enemy : enemyUnits)
            {
                if (unit->isInOurWeaponRange(enemy) || unit->isInEnemyWeaponRange(enemy))
                {
                    isEnemyInRange = true;
                    goto breakOuterLoop;
                }
            }
        }
        breakOuterLoop:;
    }

    // If any of our units are in range of an enemy, we attack
    if (isEnemyInRange)
    {
        for (auto &workerAndTarget : workersAndTargets)
        {
            workerAndTarget.first->attack(workerAndTarget.second->bwapiUnit);
        }

        return;
    }

    // Otherwise we rally our workers on the mineral patch
    auto rallyPatch = Map::getMyMain()->workerDefenseRallyPatch;
    if (rallyPatch)
    {
        for (const auto &worker : reservedWorkers)
        {
            worker->gather(rallyPatch);
        }
    }
}
