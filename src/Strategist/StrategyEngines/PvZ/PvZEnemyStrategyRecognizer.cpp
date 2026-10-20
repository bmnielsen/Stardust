#include "StrategyEngines/PvZ.h"

#include "Config.h"
#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"
#include "General.h"
#include "RecognizerShared.h"

/* P2 step A (dedup plan): shared observation predicates live in RecognizerShared.h. */
using namespace RecognizerShared;

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
    /* Declarative thresholds (S9-merge): defaults are the audited literals so a */
    /* missing/partial recognizers.json is behavior-identical. */
    struct PvZRecognizerThresholds
    {
        int workerRushWindow = 6000;
        int workerRushMinWorkers = 2;
        int zerglingRushPoolBy = 1500;
        int zerglingRushLingBy = 2500;
        int sunkenContainWindow = 5000;
        int zerglingAllInEarlyWindow = 6000;
        int zerglingAllInEarlyBy = 4000;
        int zerglingAllInEarlyCount = 9;
        int zerglingAllInMidBy = 5000;
        int zerglingAllInMidCount = 13;
        int zerglingAllInMidWindow = 8000;
        int zerglingAllInLateBy = 7000;
        int zerglingAllInLateCount = 20;
        int zerglingAllInFinalBy = 9000;
        int zerglingAllInFinalCount = 30;
        int hydraBustLingsBy = 7000;
        int hydraBustLingCount = 7;
        int hydraBustDenWindow = 7000;
        int lairWindow1 = 8000;
        int lairWindow2 = 10000;
        int lairLingsBy = 10000;
        int lairLingCount = 10;
        int lairHydraliskBy = 10000;
        int lairHydraliskCount = 4;
        int turtleColony2By = 5000;
        int turtleColony3By = 6000;
        int turtleColony4By = 7000;
        int turtleColony5By = 8000;
        int sunkenContainLateFrom = 8000;
        int lairFrom = 8000;
        int lairEarlyBy = 6000;
        int mutaRushWindow = 12000;
        int mutaRushSpireBy = 8000;
        int mutaRushMutaBy = 10000;
        int mutaRushMutaCount = 4;
    };

    const PvZRecognizerThresholds &rt()
    {
        static const PvZRecognizerThresholds instance = []
        {
            PvZRecognizerThresholds x;
            const auto &j = Config::recognizers();
            auto iterRef = j.find("PvZ");
            if (iterRef != j.end() && iterRef->is_object())
            {
                auto assign = [&iterRef](const char *key, int &field)
                {
                    auto f = iterRef->find(key);
                    if (f != iterRef->end() && f->is_number_integer()) field = f->get<int>();
                };
                assign("workerRushWindow", x.workerRushWindow);
                assign("workerRushMinWorkers", x.workerRushMinWorkers);
                assign("zerglingRushPoolBy", x.zerglingRushPoolBy);
                assign("zerglingRushLingBy", x.zerglingRushLingBy);
                assign("sunkenContainWindow", x.sunkenContainWindow);
                assign("zerglingAllInEarlyWindow", x.zerglingAllInEarlyWindow);
                assign("zerglingAllInEarlyBy", x.zerglingAllInEarlyBy);
                assign("zerglingAllInEarlyCount", x.zerglingAllInEarlyCount);
                assign("zerglingAllInMidBy", x.zerglingAllInMidBy);
                assign("zerglingAllInMidCount", x.zerglingAllInMidCount);
                assign("zerglingAllInMidWindow", x.zerglingAllInMidWindow);
                assign("zerglingAllInLateBy", x.zerglingAllInLateBy);
                assign("zerglingAllInLateCount", x.zerglingAllInLateCount);
                assign("zerglingAllInFinalBy", x.zerglingAllInFinalBy);
                assign("zerglingAllInFinalCount", x.zerglingAllInFinalCount);
                assign("hydraBustLingsBy", x.hydraBustLingsBy);
                assign("hydraBustLingCount", x.hydraBustLingCount);
                assign("hydraBustDenWindow", x.hydraBustDenWindow);
                assign("lairWindow1", x.lairWindow1);
                assign("lairWindow2", x.lairWindow2);
                assign("lairLingsBy", x.lairLingsBy);
                assign("lairLingCount", x.lairLingCount);
                assign("lairHydraliskBy", x.lairHydraliskBy);
                assign("lairHydraliskCount", x.lairHydraliskCount);
                assign("turtleColony2By", x.turtleColony2By);
                assign("turtleColony3By", x.turtleColony3By);
                assign("turtleColony4By", x.turtleColony4By);
                assign("turtleColony5By", x.turtleColony5By);
                assign("sunkenContainLateFrom", x.sunkenContainLateFrom);
                assign("lairFrom", x.lairFrom);
                assign("lairEarlyBy", x.lairEarlyBy);
                assign("mutaRushWindow", x.mutaRushWindow);
                assign("mutaRushSpireBy", x.mutaRushSpireBy);
                assign("mutaRushMutaBy", x.mutaRushMutaBy);
                assign("mutaRushMutaCount", x.mutaRushMutaCount);
            }
            return x;
        }();
        return instance;
    }

    bool isWorkerRush()
    {
        return RecognizerShared::isWorkerRush(rt().workerRushWindow, rt().workerRushMinWorkers);
    }

    bool isZerglingRush()
    {
        if (frameNow >= rt().workerRushWindow) return false;

        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Spawning_Pool, rt().zerglingRushPoolBy) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, rt().zerglingRushLingBy);
    }

    bool isZerglingAllIn()
    {
        /* Suspect ling all-in if the enemy gets gas without a lair */
        if (UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Extractor) && !UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Lair))
        {
            auto &gasTimings = UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Extractor);
            auto gasCompleted = gasTimings.begin()->first + UnitHelper::BuildTime(BWAPI::UnitTypes::Zerg_Extractor);

            /* Get the earliest frame we scouted one of the enemy's known hatcheries */
            int earliestFrame = INT_MAX;
            for (auto &hatch : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Zerg_Hatchery))
            {
                int lastSeen = Map::lastSeen(BWAPI::TilePosition(hatch->prevPos));
                earliestFrame = std::min(earliestFrame, lastSeen);
            }

            /* Expect lair to have been started less than 1000 frames after gas completion */
            if ((earliestFrame - gasCompleted) > 1000) return true;
        }

        /* Expect a ling all-in if the enemy builds two in-base hatches on a low worker count */
        if (frameNow < 5000 &&
            UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Spawning_Pool) > 0 &&
            UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Hatchery) > 1 &&
            UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Drone) < 15)
        {
            auto enemyNatural = Map::getEnemyStartingNatural();
            if (enemyNatural && !enemyNatural->owner) return true;
        }

        if (frameNow < rt().zerglingAllInEarlyWindow)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, rt().zerglingAllInEarlyBy, rt().zerglingAllInEarlyCount) ||
                   createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, rt().zerglingAllInMidBy, rt().zerglingAllInMidCount);
        }

        if (frameNow < rt().zerglingAllInMidWindow)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, rt().zerglingAllInLateBy, rt().zerglingAllInLateCount) &&
                   UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 10;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, rt().zerglingAllInFinalBy, rt().zerglingAllInFinalCount) &&
               UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 10;
    }

    bool isHydraBust()
    {
        /* Not a Hydra bust if there are more than 6 lings or no early hydra den */
        if (createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, rt().hydraBustLingsBy, rt().hydraBustLingCount) ||
            !createdBeforeFrame(BWAPI::UnitTypes::Zerg_Hydralisk_Den, rt().hydraBustDenWindow, 1))
        {
            return false;
        }

        /* Early logic: determined purely on existance of early Hydra den */
        if (frameNow < rt().lairWindow1) return true;

        /* Middle logic: ling and hydra counts */
        if (frameNow < rt().lairWindow2)
        {
            return !createdBeforeFrame(BWAPI::UnitTypes::Zerg_Zergling, rt().lairLingsBy, rt().lairLingCount) &&
                   createdBeforeFrame(BWAPI::UnitTypes::Zerg_Hydralisk, rt().lairHydraliskBy, rt().lairHydraliskCount);
        }

        /* Late logic: current hydra counts */
        return UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Hydralisk) > 10;
    }

    bool isTurtle()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, rt().turtleColony2By, 2) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, rt().turtleColony3By, 3) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, rt().turtleColony4By, 4) ||
               createdBeforeFrame(BWAPI::UnitTypes::Zerg_Creep_Colony, rt().turtleColony5By, 5);
    }

    bool isSunkenContain(bool currentlySunkenContain = false)
    {
        if (!currentlySunkenContain && frameNow > 8000) return false;
        if (Map::mapSpecificOverride()->hasBackdoorNatural()) return false;

        /* Sunken contain if the enemy owns our natural and has us outnumbered */
        auto natBase = Map::getMyNatural();
        if (!natBase) return false;
        if (natBase->owner != BWAPI::Broodwar->enemy()) return false;

        /* Gather enemy combat value at the base */
        int enemyValue = 0;
        for (const auto &gameUnit : UnitMgr::enemyAtBase(natBase))
        {
            auto type = gameUnit->type;
            bool complete = gameUnit->completed;

            if (gameUnit->type == BWAPI::UnitTypes::Zerg_Creep_Colony)
            {
                type = BWAPI::UnitTypes::Zerg_Sunken_Colony;
                complete = false;
            }

            if (!UnitHelper::IsCombatUnit(type)) continue;

            if (complete)
            {
                enemyValue += CombatSim::unitValue(type);
            }
            else
            {
                enemyValue += CombatSim::unitValue(type) / 2;
            }
        }

        /* Gather our combat value */
        int ourValue = 0;
        for (const auto &gameUnit : UnitMgr::allMine())
        {
            if (!gameUnit->completed) continue;
            if (!UnitHelper::IsCombatUnit(gameUnit->type)) continue;
            if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
            {
                int span = gameUnit->getDistance(natBase->getPosition());
                if (span > 200) continue;
            }

            ourValue += CombatSim::unitValue(gameUnit);
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
        /* Assume early lair means mutas */
        if (frameNow < 8000 && createdBeforeFrame(BWAPI::UnitTypes::Zerg_Lair, 6000, 1)) return true;

        /* Early spire or mutas */
        return frameNow < 12000 && (createdBeforeFrame(BWAPI::UnitTypes::Zerg_Spire, 8000, 1) ||
                                        createdBeforeFrame(BWAPI::UnitTypes::Zerg_Mutalisk, 10000, 4));
    }

    /* ===== P2-C: declaration-table recognition (dedup plan) ===== */
    /* Same predicates, same thresholds, same per-state guard order as the switch */
    /* path above; the state machine itself becomes data (RecognizerShared */
    /* transition table). Shadow-verified against the switch path before it */
    /* becomes authoritative. */

    using ZS = PvZ::ZergStrategy;
    using ZTransition = RecognizerShared::STransition<ZS>;

    const std::vector<ZTransition> &zergTransitionTable()
    {
        static const std::vector<ZTransition> table = [] {
            std::vector<ZTransition> t;
            auto add = [&t](ZS from, bool (*guard)(), ZS to, bool chase) { t.push_back(ZTransition{from, chase, to, guard}); };

            /* Unknown: rush/contain classification first, then the frame-4000 */
            /* default chase, the scout gate (stay = final to self), and the */
            /* pool/hatch order determination */
            add(ZS::Unknown, []{ return isWorkerRush(); }, ZS::WorkerRush, false);
            add(ZS::Unknown, []{ return isSunkenContain(); }, ZS::SunkenContain, false);
            add(ZS::Unknown, []{ return isZerglingRush(); }, ZS::ZerglingRush, false);
            add(ZS::Unknown, []{ return frameNow > 4000; }, ZS::PoolBeforeHatchery, true);
            add(ZS::Unknown, []{ return Director::getWorkerScoutStatus() != Director::WorkerScoutStatus::EnemyBaseScouted; }, ZS::Unknown, false);
            add(ZS::Unknown, []{ return createdBeforeUnit(BWAPI::UnitTypes::Zerg_Spawning_Pool, 1, BWAPI::UnitTypes::Zerg_Hatchery, 2); }, ZS::PoolBeforeHatchery, true);
            add(ZS::Unknown, []{ return createdBeforeUnit(BWAPI::UnitTypes::Zerg_Hatchery, 2, BWAPI::UnitTypes::Zerg_Spawning_Pool, 1); }, ZS::HatcheryBeforePool, true);

            add(ZS::WorkerRush, []{ return !isWorkerRush(); }, ZS::Unknown, true);

            add(ZS::ZerglingRush, []{ return isWorkerRush(); }, ZS::WorkerRush, false);
            add(ZS::ZerglingRush, []{ return isSunkenContain(); }, ZS::SunkenContain, false);
            add(ZS::ZerglingRush, []{ return frameNow >= 6000; }, ZS::PoolBeforeHatchery, true);

            add(ZS::PoolBeforeHatchery, []{ return isWorkerRush(); }, ZS::WorkerRush, false);
            add(ZS::PoolBeforeHatchery, []{ return isSunkenContain(); }, ZS::SunkenContain, false);
            add(ZS::PoolBeforeHatchery, []{ return isZerglingRush(); }, ZS::ZerglingRush, false);
            add(ZS::PoolBeforeHatchery, []{ return isZerglingAllIn(); }, ZS::ZerglingAllIn, false);
            add(ZS::PoolBeforeHatchery, []{ return isHydraBust(); }, ZS::HydraBust, false);
            add(ZS::PoolBeforeHatchery, []{ return isTurtle(); }, ZS::Turtle, true);
            add(ZS::PoolBeforeHatchery, []{ return hasLairTech(); }, ZS::Lair, true);

            add(ZS::HatcheryBeforePool, []{ return isWorkerRush(); }, ZS::WorkerRush, false);
            add(ZS::HatcheryBeforePool, []{ return isSunkenContain(); }, ZS::SunkenContain, false);
            add(ZS::HatcheryBeforePool, []{ return isZerglingAllIn(); }, ZS::ZerglingAllIn, false);
            add(ZS::HatcheryBeforePool, []{ return isHydraBust(); }, ZS::HydraBust, false);
            add(ZS::HatcheryBeforePool, []{ return isTurtle(); }, ZS::Turtle, true);
            add(ZS::HatcheryBeforePool, []{ return hasLairTech(); }, ZS::Lair, true);

            add(ZS::ZerglingAllIn, []{ return isSunkenContain(); }, ZS::SunkenContain, false);
            add(ZS::ZerglingAllIn, []{ return !isZerglingAllIn(); }, ZS::PoolBeforeHatchery, true);
            add(ZS::ZerglingAllIn, []{ return hasLairTech(); }, ZS::Lair, true);

            add(ZS::HydraBust, []{ return isZerglingAllIn(); }, ZS::ZerglingAllIn, false);
            add(ZS::HydraBust, []{ return !isHydraBust(); }, ZS::PoolBeforeHatchery, true);

            add(ZS::Turtle, []{ return isSunkenContain(); }, ZS::SunkenContain, false);
            add(ZS::Turtle, []{ return isHydraBust(); }, ZS::HydraBust, false);
            add(ZS::Turtle, []{ return hasLairTech(); }, ZS::Lair, true);

            add(ZS::SunkenContain, []{ return !isSunkenContain(true); }, ZS::PoolBeforeHatchery, true);

            add(ZS::Lair, []{ return isMutaRush(); }, ZS::MutaRush, false);

            add(ZS::MutaRush, []{ return !isMutaRush(); }, ZS::Lair, false);

            return t;
        }();
        return table;
    }
}

PvZ::ZergStrategy PvZ::recognizeEnemyStrategyTable(PvZ::ZergStrategy current)
{
    bool exhausted = false;
    auto outcome = RecognizerShared::runTransitions(current, zergTransitionTable(), exhausted);
    if (exhausted) AppLog::Get() << "ERROR: Loop in strategy recognizer (table path)";
    return outcome;
}

PvZ::ZergStrategy PvZ::recognizeEnemyStrategy()
{
    /* P2-C authority flip (2026-09-16): the declaration-table path above is now */
    /* the single implementation. The switch-based predecessor was shadow-verified */
    /* with zero divergence across 59 games (5 small-batch + the 54-game routine */
    /* batch) before this flip. */
    return recognizeEnemyStrategyTable(enemyStrategy);
}
