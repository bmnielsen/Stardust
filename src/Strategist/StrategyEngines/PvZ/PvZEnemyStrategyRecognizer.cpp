#include "StrategyEngines/PvZ.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"

std::map<PvZ::ZergStrategy, std::string> PvZ::ZergStrategyNames = {
        {ZergStrategy::Unknown,            "Unknown"},
        {ZergStrategy::WorkerRush,         "WorkerRush"},
        {ZergStrategy::SunkenContain,      "SunkenContain"},
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
        if (currentFrame >= 6000) return false;

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
        if (currentFrame >= 6000) return false;

        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Spawning_Pool, 1500) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 2500);
    }

    bool isZerglingAllIn()
    {
        // Expect a ling all-in if the enemy builds two in-base hatches on a low worker count
        if (currentFrame < 5000 &&
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Hatchery) > 2 &&
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Drone) < 15 &&
            !Map::getEnemyStartingNatural()->owner)
        {
            return true;
        }

        if (currentFrame < 6000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 4000, 8) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 5000, 12);
        }

        if (currentFrame < 8000)
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

    bool isSunkenContain(bool currentlySunkenContain = false)
    {
        if (!currentlySunkenContain && currentFrame > 8000) return false;

        // Sunken contain if the enemy owns our natural or if they have a sunken in it
        auto natural = Map::getMyNatural();
        if (!natural) return false;
        if (natural->owner == BWAPI::Broodwar->enemy()) return true;

        auto inOurNatural = [&natural](const Unit &unit)
        {
            auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition));
            return area && area == natural->getArea();
        };
        for (auto &creepColony : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Creep_Colony))
        {
            if (inOurNatural(creepColony)) return true;
        }
        for (auto &sunkenColony : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Sunken_Colony))
        {
            if (inOurNatural(sunkenColony)) return true;
        }

        return false;
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
                if (isSunkenContain()) return ZergStrategy::SunkenContain;
                if (isZerglingRush()) return ZergStrategy::ZerglingRush;

                // Default to something reasonable if our scouting completely fails
                if (currentFrame > 4000)
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
                if (isSunkenContain()) return ZergStrategy::SunkenContain;

                // Consider the rush to be over after 6000 frames
                // From there the PoolBeforeHatchery handler will potentially transition into ZerglingAllIn
                if (currentFrame >= 6000)
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }

                break;
            case ZergStrategy::PoolBeforeHatchery:
                if (isWorkerRush()) return ZergStrategy::WorkerRush;
                if (isSunkenContain()) return ZergStrategy::SunkenContain;

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
                if (isSunkenContain()) return ZergStrategy::SunkenContain;
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
                if (isSunkenContain()) return ZergStrategy::SunkenContain;

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
                if (isSunkenContain()) return ZergStrategy::SunkenContain;

                if (hasLairTech())
                {
                    strategy = ZergStrategy::Lair;
                    continue;
                }

                break;
            case ZergStrategy::SunkenContain:
                if (!isSunkenContain(true))
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
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
