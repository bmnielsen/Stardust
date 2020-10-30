#include "StrategyEngines/PvZ.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"

std::map<PvZ::ZergStrategy, std::string> PvZ::ZergStrategyNames = {
        {ZergStrategy::Unknown,            "Unknown"},
        {ZergStrategy::WorkerRush,         "WorkerRush"},
        {ZergStrategy::ZerglingRush,       "ZerglingRush"},
        {ZergStrategy::PoolBeforeHatchery, "PoolBeforeHatchery"},
        {ZergStrategy::HatcheryBeforePool, "HatcheryBeforePool"},
        {ZergStrategy::ZerglingAllIn,      "ZerglingAllIn"},
        {ZergStrategy::Turtle,             "Turtle"},
        {ZergStrategy::Lair,               "Lair"}
};

namespace
{
    bool countAtLeast(BWAPI::UnitType type, int count)
    {
        auto &timings = Units::getEnemyUnitTimings(type);
        return timings.size() >= count;
    }

    bool createdBeforeFrame(BWAPI::UnitType type, int frame, int count = 1)
    {
        auto &timings = Units::getEnemyUnitTimings(type);
        if (timings.size() < count) return false;

        return timings[count - 1].first < frame;
    }

    bool createdBeforeUnit(BWAPI::UnitType first, int firstCount, BWAPI::UnitType second, int secondCount)
    {
        auto &firstTimings = Units::getEnemyUnitTimings(first);
        if (firstTimings.size() < firstCount) return false;

        auto &secondTimings = Units::getEnemyUnitTimings(second);
        return secondTimings.size() < secondCount || firstTimings[firstCount - 1].first <= secondTimings[secondCount - 1].first;
    }

    bool isWorkerRush()
    {
        if (BWAPI::Broodwar->getFrameCount() >= 6000) return false;

        int workers = 0;
        for (const Unit &unit : Units::allEnemy())
        {
            if (!unit->lastPositionValid) continue;
            if (unit->type.isBuilding()) continue;

            bool isInArea = false;
            for (const auto &area : Map::getMyMainAreas())
            {
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) == area)
                {
                    isInArea = true;
                    break;
                }
            }
            if (!isInArea) continue;

            // If there is a normal combat unit in our main, it isn't a worker rush
            if (UnitUtil::IsCombatUnit(unit->type) && unit->type.canAttack()) return false;

            if (unit->type.isWorker()) workers++;
        }

        return workers > 2;
    }

    bool isZerglingRush()
    {
        if (BWAPI::Broodwar->getFrameCount() >= 6000) return false;

        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Spawning_Pool, 1500) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 2500);
    }

    bool isZerglingAllIn()
    {
        if (BWAPI::Broodwar->getFrameCount() < 6000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 4000, 8) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 5000, 12);
        }

        if (BWAPI::Broodwar->getFrameCount() < 8000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 7000, 20) &&
                   Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 10;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 9000, 30) &&
               Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 10;
    }

    bool isTurtle()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, 5000, 2) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, 6000, 3) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, 7000, 4) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, 8000, 5);
    }

    bool hasLairTech()
    {
        return countAtLeast(BWAPI::UnitTypes::Zerg_Lair, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Lurker_Egg, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Lurker, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Spire, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Mutalisk, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Devourer, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Guardian, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Ultralisk, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Defiler_Mound, 1) ||
               countAtLeast(BWAPI::UnitTypes::Zerg_Defiler, 1);
    }
}

PvZ::ZergStrategy PvZ::recognizeEnemyStrategy()
{
    auto strategy = enemyStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case ZergStrategy::Unknown:
                if (isWorkerRush()) return ZergStrategy::WorkerRush;
                if (isZerglingRush()) return ZergStrategy::ZerglingRush;

                // Default to something reasonable if our scouting completely fails
                if (BWAPI::Broodwar->getFrameCount() > 4000)
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }

                // For pool or hatch first determination, wait until we have scouted the area around the base
                if (Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::EnemyBaseScouted) break;

                // Transition to pool-first or hatchery-first when we have the appropriate scouting information
                if (createdBeforeUnit(BWAPI::UnitTypes::Zerg_Spawning_Pool, 1, BWAPI::UnitTypes::Zerg_Hatchery, 2))
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }
                if (createdBeforeUnit(BWAPI::UnitTypes::Zerg_Hatchery, 2, BWAPI::UnitTypes::Zerg_Spawning_Pool, 1))
                {
                    strategy = ZergStrategy::HatcheryBeforePool;
                    continue;
                }

                break;
            case ZergStrategy::WorkerRush:
                if (!isWorkerRush())
                {
                    strategy = ZergStrategy::Unknown;
                    continue;
                }

                break;
            case ZergStrategy::ZerglingRush:
                if (isWorkerRush()) return ZergStrategy::WorkerRush;

                // Consider the rush to be over after 6000 frames
                // From there the PoolBeforeHatchery handler will potentially transition into ZerglingAllIn
                if (BWAPI::Broodwar->getFrameCount() >= 6000)
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }

                break;
            case ZergStrategy::PoolBeforeHatchery:
                if (isWorkerRush()) return ZergStrategy::WorkerRush;

                // We might detect a rush late on large maps or if scouting is denied
                if (isZerglingRush()) return ZergStrategy::ZerglingRush;

                if (isZerglingAllIn()) return ZergStrategy::ZerglingAllIn;

                if (isTurtle())
                {
                    strategy = ZergStrategy::Turtle;
                    continue;
                }

                if (hasLairTech())
                {
                    strategy = ZergStrategy::Lair;
                    continue;
                }

                break;
            case ZergStrategy::HatcheryBeforePool:
                if (isWorkerRush()) return ZergStrategy::WorkerRush;
                if (isZerglingAllIn()) return ZergStrategy::ZerglingAllIn;

                if (isTurtle())
                {
                    strategy = ZergStrategy::Turtle;
                    continue;
                }

                if (hasLairTech())
                {
                    strategy = ZergStrategy::Lair;
                    continue;
                }

                break;
            case ZergStrategy::ZerglingAllIn:
                if (!isZerglingAllIn())
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }

                if (hasLairTech())
                {
                    strategy = ZergStrategy::Lair;
                    continue;
                }

                break;
            case ZergStrategy::Turtle:
                if (hasLairTech())
                {
                    strategy = ZergStrategy::Lair;
                    continue;
                }

                break;
            case ZergStrategy::Lair:
                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << ZergStrategyNames[strategy];
    return strategy;
}
