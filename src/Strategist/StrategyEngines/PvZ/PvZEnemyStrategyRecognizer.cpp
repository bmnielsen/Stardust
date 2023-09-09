#include "StrategyEngines/PvZ.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"
#include "General.h"

std::map<PvZ::ZergStrategy, std::string> PvZ::ZergStrategyNames = {
        {ZergStrategy::Unknown,            "Unknown"},
        {ZergStrategy::WorkerRush,         "WorkerRush"},
        {ZergStrategy::SunkenContain,      "SunkenContain"},
        {ZergStrategy::ZerglingRush,       "ZerglingRush"},
        {ZergStrategy::PoolBeforeHatchery, "PoolBeforeHatchery"},
        {ZergStrategy::HatcheryBeforePool, "HatcheryBeforePool"},
        {ZergStrategy::ZerglingAllIn,      "ZerglingAllIn"},
        {ZergStrategy::HydraBust,          "HydraBust"},
        {ZergStrategy::Turtle,             "Turtle"},
        {ZergStrategy::Lair,               "Lair"},
        {ZergStrategy::MutaRush,           "MutaRush"},
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

//    int frameDiff(BWAPI::UnitType firstType, BWAPI::UnitType secondType, int firstCount = 1, int secondCount = 1)
//    {
//        auto &firstTimings = Units::getEnemyUnitTimings(firstType);
//        if (firstTimings.size() < firstCount) return 0;
//
//        auto &secondTimings = Units::getEnemyUnitTimings(firstType);
//        if (secondTimings.size() < secondCount) return currentFrame - firstTimings[firstCount - 1].first;
//
//        return secondTimings[secondCount - 1].first - firstTimings[firstCount - 1].first;
//    }

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
        // Suspect ling all-in if the enemy gets gas without a lair
        if (Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Extractor) && !Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Lair))
        {
            auto &gasTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Extractor);
            auto gasCompleted = gasTimings.begin()->first + UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Extractor);

            // Get the earliest frame we scouted one of the enemy's known hatcheries
            int earliestFrame = INT_MAX;
            for (auto &hatch : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Hatchery))
            {
                int lastSeen = Map::lastSeen(BWAPI::TilePosition(hatch->lastPosition));
                earliestFrame = std::min(earliestFrame, lastSeen);
            }

            // Expect lair to have been started less than 1000 frames after gas completion
            if ((earliestFrame - gasCompleted) > 1000) return true;
        }

        // Expect a ling all-in if the enemy builds two in-base hatches on a low worker count
        if (currentFrame < 5000 &&
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Hatchery) > 1 &&
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Drone) < 15 &&
            !Map::getEnemyStartingNatural()->owner)
        {
            return true;
        }

        if (currentFrame < 6000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 4000, 9) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 5000, 13);
        }

        if (currentFrame < 8000)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 7000, 20) &&
                   Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 10;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 9000, 30) &&
               Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 10;
    }

    bool isHydraBust()
    {
        // Not a Hydra bust if there are more than 6 lings or no early hydra den
        if (createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 7000, 7) ||
            !createdBeforeFrame(BWAPI::UnitTypes::Zerg_Hydralisk_Den, 7000, 1))
        {
            return false;
        }

        // Early logic: determined purely on existance of early Hydra den
        if (currentFrame < 8000) return true;

        // Middle logic: ling and hydra counts
        if (currentFrame < 10000)
        {
            return !createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, 10000, 10) &&
                   createdBeforeFrame(BWAPI::UnitTypes::Zerg_Hydralisk, 10000, 4);
        }

        // Late logic: current hydra counts
        return Units::countEnemy(BWAPI::UnitTypes::Zerg_Hydralisk) > 10;
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

        // Sunken contain if the enemy owns our natural and has us outnumbered
        auto natural = Map::getMyNatural();
        if (!natural) return false;
        if (natural->owner != BWAPI::Broodwar->enemy()) return false;

        // Gather enemy combat value at the base
        int enemyValue = 0;
        for (const auto &unit : Units::enemyAtBase(natural))
        {
            auto type = unit->type;
            bool complete = unit->completed;

            if (unit->type == BWAPI::UnitTypes::Zerg_Creep_Colony)
            {
                type = BWAPI::UnitTypes::Zerg_Sunken_Colony;
                complete = false;
            }

            if (!UnitUtil::IsCombatUnit(type)) continue;

            if (complete)
            {
                enemyValue += CombatSim::unitValue(type);
            }
            else
            {
                enemyValue += CombatSim::unitValue(type) / 2;
            }
        }

        // Gather our combat value
        int ourValue = 0;
        for (const auto &unit : Units::allMine())
        {
            if (!unit->completed) continue;
            if (!UnitUtil::IsCombatUnit(unit->type)) continue;
            if (unit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;

            ourValue += CombatSim::unitValue(unit);
        }

        return enemyValue > ourValue;
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

    bool isMutaRush()
    {
        // Assume early lair means mutas
        if (currentFrame < 8000 && createdBeforeFrame(BWAPI::UnitTypes::Zerg_Lair, 6000, 1)) return true;

        // Early spire or mutas
        return currentFrame < 12000 && (createdBeforeFrame(BWAPI::UnitTypes::Zerg_Spire, 8000, 1) ||
                                        createdBeforeFrame(BWAPI::UnitTypes::Zerg_Mutalisk, 10000, 4));
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

                if (isHydraBust()) return ZergStrategy::HydraBust;

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
                if (isHydraBust()) return ZergStrategy::HydraBust;

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
            case ZergStrategy::HydraBust:
                if (isZerglingAllIn()) return ZergStrategy::ZerglingAllIn;

                if (!isHydraBust())
                {
                    strategy = ZergStrategy::PoolBeforeHatchery;
                    continue;
                }

                break;
            case ZergStrategy::Turtle:
                if (isSunkenContain()) return ZergStrategy::SunkenContain;
                if (isHydraBust()) return ZergStrategy::HydraBust;

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
                if (isMutaRush()) return ZergStrategy::MutaRush;

                break;
            case ZergStrategy::MutaRush:
                if (!isMutaRush()) return ZergStrategy::Lair;

                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy recognizer, ended on " << ZergStrategyNames[strategy];
    return strategy;
}
