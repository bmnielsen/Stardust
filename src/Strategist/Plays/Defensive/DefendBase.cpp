#include "DefendBase.h"

#include "Map.h"
#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Workers.h"
#include "Players.h"

/*
 * General approach:
 *
 * - If the mineral line is under attack, do worker defense
 * - Otherwise, determine the level of risk to the base.
 * - If high risk (enemy units are here or expected to come soon), reserve some units for protection
 * - If at medium risk, get some static defense
 * - If at low risk, do nothing
 */

namespace
{
    auto &bwemMap = BWEM::Map::Instance();
}

DefendBase::DefendBase(Base *base) : base(base), squad(std::make_shared<DefendBaseSquad>(base))
{
    General::addSquad(squad);
}

void DefendBase::update()
{
    mineralLineWorkerDefense();

    // Temporary early game logic: keeps two zealots in main
    if (base == Map::getMyMain() && BWAPI::Broodwar->getFrameCount() < 7000)
    {
        bool protectionNeeded = true;
        if ((Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon)) > 2)
        {
            // Get enemy combat units in our base
            std::set<std::shared_ptr<Unit>> enemyCombatUnits;
            Units::getInArea(enemyCombatUnits, BWAPI::Broodwar->enemy(), Map::getMyMain()->getArea(), [](const std::shared_ptr<Unit> &unit)
            {
                return UnitUtil::IsCombatUnit(unit->type);
            });
            if (enemyCombatUnits.empty()) protectionNeeded = false;
        }

        if (protectionNeeded)
        {
            int zealotsNeeded = 2 - squad->getUnits().size();
            if (zealotsNeeded > 0)
            {
                status.unitRequirements.emplace_back(zealotsNeeded, BWAPI::UnitTypes::Protoss_Zealot, squad->getTargetPosition());
            }
        }
    }
}

void DefendBase::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // TODO: Use higher priority in emergencies
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[20].emplace_back(std::in_place_type<UnitProductionGoal>, unitRequirement.type, unitRequirement.count, 1);
    }
}

void DefendBase::mineralLineWorkerDefense()
{
    // Check if there are enemy melee units in our mineral line
    std::set<std::shared_ptr<Unit>> enemyUnits;
    Units::get(enemyUnits, BWAPI::Broodwar->enemy(), [this](const std::shared_ptr<Unit> &unit)
    {
        if (!unit->lastPositionValid) return false;
        if (!UnitUtil::IsCombatUnit(unit->type) || UnitUtil::IsRangedUnit(unit->type) || !UnitUtil::CanAttackGround(unit->type)) return false;

        auto comingPosition = UnitUtil::PredictPosition(unit->unit, 24);
        return base->isInMineralLine(BWAPI::TilePosition(comingPosition));
    });

    if (enemyUnits.empty())
    {
        if (!reservedWorkers.empty())
        {
            for (auto worker : reservedWorkers)
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
    Workers::reserveBaseWorkers(reservedWorkers, base);

    // Choose a target for each of our workers
    // Each attacks the closest enemy unit with the least health
    bool isEnemyInRange = false;
    std::vector<std::pair<BWAPI::Unit, BWAPI::Unit>> workersAndTargets;
    for (auto worker : reservedWorkers)
    {
        BWAPI::Unit bestTarget = nullptr;
        int bestTargetDist = INT_MAX;
        int bestTargetHealth = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            int dist = Geo::EdgeToEdgeDistance(worker->getType(), worker->getPosition(), enemy->type, enemy->lastPosition);
            if (dist < bestTargetDist || (dist == bestTargetDist && enemy->lastHealth < bestTargetHealth))
            {
                bestTarget = enemy->unit;
                bestTargetDist = dist;
                bestTargetHealth = enemy->lastHealth;
            }
        }

        workersAndTargets.emplace_back(std::make_pair(worker, bestTarget));

        if (bestTargetDist <= Players::weaponRange(bestTarget->getPlayer(), bestTarget->getType().groundWeapon()) ||
            bestTargetDist <= Players::weaponRange(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Probe.groundWeapon()))
        {
            isEnemyInRange = true;
        }
    }

    // If any of our workers is in range of an enemy, we attack
    if (isEnemyInRange)
    {
        for (auto &workerAndTarget : workersAndTargets)
        {
            Units::getMine(workerAndTarget.first).attack(workerAndTarget.second);
        }

        return;
    }

    // Otherwise we rally our workers on the mineral patch
    if (base->workerDefenseRallyPatch)
    {
        for (auto worker : reservedWorkers)
        {
            Units::getMine(worker).gather(base->workerDefenseRallyPatch);
        }
    }
}