#include "DarkTemplarHarass.h"

#include "Map.h"
#include "Players.h"
#include "PathFinding.h"
#include "Units.h"

#include "DebugFlag_UnitOrders.h"

namespace
{
    bool hasPathWithoutDetection(BWAPI::Position start, BWAPI::Position end)
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        auto chokePath = PathFinding::GetChokePointPath(start,
                                                        end,
                                                        BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
        for (auto bwemChoke : chokePath)
        {
            auto choke = Map::choke(bwemChoke);
            if (choke->width > 300) continue;
            if (grid.detection(choke->center) > 0) return false;
        }

        return true;
    }

    Base *getBaseToHarass(MyUnit &unit)
    {
        std::vector<std::pair<Base *, bool>> harassableBases;
        for (auto base : Map::getEnemyBases())
        {
            bool hasCannon = false;
            for (const auto &cannon : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
            {
                if (!cannon->completed) continue;
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(cannon->lastPosition)) == base->getArea())
                {
                    hasCannon = true;
                    break;
                }
            }
            if (hasCannon) continue;
            if (!hasPathWithoutDetection(unit->lastPosition, base->getPosition())) continue;

            int workerCount = 0;
            for (const auto &worker : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (!worker->lastPositionValid) continue;

                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(worker->lastPosition)) == base->getArea())
                {
                    workerCount++;
                }
            }

            harassableBases.push_back(std::make_pair(base, workerCount > 2));
        }

        int bestDist = INT_MAX;
        Base *bestBase = nullptr;
        bool bestBaseHasWorkers = false;
        for (auto base : harassableBases)
        {
            if (bestBaseHasWorkers && !base.second) continue;

            int dist = unit->getDistance(base.first->getPosition());
            if (dist < bestDist || (!bestBaseHasWorkers && base.second))
            {
                bestDist = dist;
                bestBase = base.first;
                bestBaseHasWorkers = base.second;
            }
        }

        return bestBase;
    }

    Unit getTarget(const MyUnit &myUnit,
                   const std::set<Unit> &enemyUnits,
                   bool allowRetreating = true,
                   int distThreshold = INT_MAX,
                   const std::function<bool(const Unit &)> &predicate = nullptr)
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());

        Unit bestTarget = nullptr;
        int bestTargetDist = INT_MAX;
        int bestTargetHealth = INT_MAX;
        for (auto &enemy : enemyUnits)
        {
            if (!enemy->lastPositionValid) continue;
            if (enemy->undetected) continue;
            if (grid.detection(enemy->lastPosition) > 0) continue;

            int dist = myUnit->getDistance(enemy);
            if (dist > distThreshold) continue;

            if (predicate && !predicate(enemy)) continue;

            if (dist < bestTargetDist || (dist == bestTargetDist && (enemy->lastHealth + enemy->lastShields) < bestTargetHealth))
            {
                if (!allowRetreating && (myUnit->getDistance(enemy, enemy->predictPosition(1)) > dist))
                {
                    continue;
                }

                bestTarget = enemy;
                bestTargetDist = dist;
                bestTargetHealth = enemy->lastHealth + enemy->lastShields;
            }
        }

        if (bestTargetDist > distThreshold) return nullptr;
        return bestTarget;
    }

    void moveToBase(MyUnit &unit, Base *base)
    {
        auto grid = PathFinding::getNavigationGrid(base->getTilePosition());
        if (grid)
        {
            auto node = (*grid)[unit->getTilePosition()];
            auto nextNode = node.nextNode;
            auto secondNode = nextNode ? nextNode->nextNode : nullptr;
            auto onPathPredicate = [&](const Unit &unit)
            {
                auto tilePosition = unit->getTilePosition();
                return (tilePosition.x == node.x && tilePosition.y == node.y) ||
                       (nextNode && (tilePosition.x == nextNode->x && tilePosition.y == nextNode->y)) ||
                       (secondNode && (tilePosition.x == secondNode->x && tilePosition.y == secondNode->y));
            };
            auto target = getTarget(unit, Units::allEnemy(), false, 100, onPathPredicate);
            if (target)
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(unit->id) << "Attacking blocking unit " << *target;
#endif
                std::vector<std::pair<MyUnit, Unit>> emptyUnitsAndTargets;
                unit->attackUnit(target, emptyUnitsAndTargets);
                return;
            }
        }

        unit->moveTo(base->getPosition());
    }

    void executeUnit(MyUnit &unit)
    {
        auto workerTarget = getTarget(unit, Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Probe), false, 300);
        if (workerTarget)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Attacking worker " << *workerTarget;
#endif
            std::vector<std::pair<MyUnit, Unit>> emptyUnitsAndTargets;
            unit->attackUnit(workerTarget, emptyUnitsAndTargets);
            return;
        }

        auto base = getBaseToHarass(unit);
        if (base)
        {
            if (unit->getDistance(base->mineralLineCenter) > 320 ||
                Map::isInNarrowChoke(base->getTilePosition()) ||
                BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != base->getArea())
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(unit->id) << "Moving to harass base @ " << BWAPI::WalkPosition(base->getPosition());
#endif
                moveToBase(unit, base);
                return;
            }
        }

        auto otherTarget = getTarget(unit, Units::allEnemy(), false);
        if (otherTarget)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Attacking target " << *otherTarget;
#endif
            std::vector<std::pair<MyUnit, Unit>> emptyUnitsAndTargets;
            unit->attackUnit(otherTarget, emptyUnitsAndTargets);
            return;
        }

        auto nextExpansions = Map::getUntakenExpansions(BWAPI::Broodwar->enemy());
        if (!nextExpansions.empty())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unit->id) << "Moving to next probably enemy expansion @ " << BWAPI::WalkPosition((*nextExpansions.begin())->getPosition());
#endif
            moveToBase(unit, *nextExpansions.begin());
            return;
        }
    }
}

void DarkTemplarHarass::update()
{
    // Always request DTs so all created DTs get assigned to this play
    status.unitRequirements.emplace_back(10, BWAPI::UnitTypes::Protoss_Dark_Templar, Map::getMyMain()->getPosition());

    // Micro each unit individually
    for (auto unit : units)
    {
        executeUnit(unit);
    }
}
