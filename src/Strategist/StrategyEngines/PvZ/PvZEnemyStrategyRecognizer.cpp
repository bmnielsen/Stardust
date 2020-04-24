#include "StrategyEngines/PvZ.h"

#include "Units.h"
#include "Map.h"

std::map<PvZ::ZergStrategy, std::string> PvZ::ZergStrategyNames = {
        {ZergStrategy::Unknown,            "Unknown"},
        {ZergStrategy::ZerglingRush,       "ZerglingRush"},
        {ZergStrategy::GasSteal,           "GasSteal"},
        {ZergStrategy::PoolBeforeHatchery, "PoolBeforeHatchery"},
        {ZergStrategy::HatcheryBeforePool, "HatcheryBeforePool"},
        {ZergStrategy::ZerglingAllIn,      "ZerglingAllIn"},
        {ZergStrategy::Turtle,             "Turtle"},
        {ZergStrategy::Lair,               "Lair"}
};

namespace
{
    int countAtLeast(BWAPI::UnitType type, int count)
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

    bool isZerglingRush()
    {
        if (BWAPI::Broodwar->getFrameCount() >= 6000) return false;

        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Spawning_Pool, 1500) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 2500);
    }

    bool isGasSteal()
    {
        for (const Unit &unit : Units::allEnemy())
        {
            if (!unit->lastPositionValid) continue;
            if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) != Map::getMyMain()->getArea()) continue;

            if (unit->type.isRefinery())
            {
                return true;
            }
        }

        return false;
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
                if (isZerglingRush()) return ZergStrategy::ZerglingRush;
                if (isGasSteal()) return ZergStrategy::GasSteal;

                // Default to something reasonable if our scouting completely fails
                if (BWAPI::Broodwar->getFrameCount() > 3500)
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }

                // Wait until we have seen at least one hatchery and two drones
                if (!countAtLeast(BWAPI::UnitTypes::Zerg_Hatchery, 1) ||
                    !countAtLeast(BWAPI::UnitTypes::Zerg_Drone, 2))
                {
                    break;
                }

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
            case ZergStrategy::ZerglingRush:
                // Consider the rush to be over after 6000 frames
                // From there the PoolBeforeHatchery handler will potentially transition into ZerglingAllIn
                if (BWAPI::Broodwar->getFrameCount() >= 6000)
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }

                break;
            case ZergStrategy::GasSteal:
                if (!isGasSteal())
                {
                    strategy = ZergStrategy::Unknown;
                    continue;
                }
                break;
            case ZergStrategy::PoolBeforeHatchery:
                // We might detect a rush late on large maps or if scouting is denied
                if (isZerglingRush()) return ZergStrategy::ZerglingRush;

                if (isGasSteal()) return ZergStrategy::GasSteal;
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
                if (isGasSteal()) return ZergStrategy::GasSteal;
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
                if (isGasSteal()) return ZergStrategy::GasSteal;

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
                if (isGasSteal()) return ZergStrategy::GasSteal;

                if (hasLairTech())
                {
                    strategy = ZergStrategy::Lair;
                    continue;
                }

                break;
            case ZergStrategy::Lair:
                if (isGasSteal()) return ZergStrategy::GasSteal;

                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << ZergStrategyNames[strategy];
    return strategy;
}
