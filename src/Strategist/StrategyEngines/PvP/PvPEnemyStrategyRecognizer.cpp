#include "StrategyEngines/PvP.h"

#include "Config.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"
#include "OpponentEconomicModel.h"
#include "Players.h"
#include "RecognizerShared.h"
#include "FileTools.h"

#include <fstream>

/* P2 step A (dedup plan): shared observation predicates live in RecognizerShared.h. */
using namespace RecognizerShared;

std::map<PvP::ProtossStrategy, std::string> PvP::ProtossStrategyNames = {
        {ProtossStrategy::Unknown,         "Unknown"},
        {ProtossStrategy::WorkerRush,      "WorkerRush"},
        {ProtossStrategy::ProxyRush,       "ProxyRush"},
        {ProtossStrategy::ZealotRush,      "ZealotRush"},
        {ProtossStrategy::EarlyForge,      "EarlyForge"},
        {ProtossStrategy::TwoGate,         "TwoGate"},
        {ProtossStrategy::NoZealotCore,    "NoZealotCore"},
        {ProtossStrategy::OneZealotCore,   "OneZealotCore"},
        {ProtossStrategy::FastExpansion,   "FastExpansion"},
        {ProtossStrategy::BlockScouting,   "BlockScouting"},
        {ProtossStrategy::ZealotAllIn,     "ZealotAllIn"},
        {ProtossStrategy::DragoonAllIn,    "DragoonAllIn"},
        {ProtossStrategy::EarlyRobo,       "EarlyRobo"},
        {ProtossStrategy::Turtle,          "Turtle"},
        {ProtossStrategy::DarkTemplarRush, "DarkTemplarRush"},
        {ProtossStrategy::MidGame,         "MidGame"},
};

namespace
{
    /* Declarative thresholds (S6 v1): defaults are the audited literals, so a */
    /* missing/partial recognizers.json is always behavior-identical. Values can */
    /* be tuned per-matchup in data/recognizers.json without recompiling. */
    struct RecognizerThresholds
    {
        int fastExpansionNexusBy = 6000;
        int workerRushWindow = 6000;
        int workerRushMinWorkers = 2;
        int zealotFirstBy = 2700;
        int zealotSecondBy = 3300;
        int gatewaySecondBy = 2300;
        int proxyWindow = 5000;
        int proxyMainDist = 1500;
        int proxyNaturalDist = 640;
        int proxyFirstPylonBy = 1300;
        int proxyFirstProdBy = 2200;
        int proxySecondPylonBy = 4000;
        int zealotAllInEarlyWindow = 6000;
        int zealotAllInEarlyZealotsBy = 5000;
        int zealotAllInEarlyZealotCount = 4;
        int zealotAllInMidWindow = 8000;
        int zealotAllInMidZealotsBy = 7000;
        int zealotAllInMidZealotCount = 8;
        int zealotAllInMidLiveZealots = 4;
        int zealotAllInLateZealotsBy = 9000;
        int zealotAllInLateZealotCount = 12;
        int dragoonAllInWindow = 10000;
        int dragoonAllInGateCount = 4;
        int earlyRoboBy = 7500;
        int dtRushArchivesBy = 10000;
        int dtRushFirstDtBy = 12000;
        int turtleCannon2By = 5000;
        int turtleCannon3By = 6000;
        int turtleCannon4By = 7000;
        int turtleCannon5By = 8000;
        int dtSuspectCoreAgeBy = 2400;
        int dtSuspectWindow = 24000;
        int midGameFrom = 14000;
    };

    const RecognizerThresholds &rt()
    {
        /* Populated once on first use; Config::initialize runs earlier in onStart. */
        static const RecognizerThresholds instance = []
        {
            RecognizerThresholds x;
            const auto &j = Config::recognizers();
            auto iterRef = j.find("PvP");
            if (iterRef != j.end() && iterRef->is_object())
            {
                auto assign = [&iterRef](const char *key, int &field)
                {
                    auto f = iterRef->find(key);
                    if (f != iterRef->end() && f->is_number_integer()) field = f->get<int>();
                };
                assign("fastExpansionNexusBy", x.fastExpansionNexusBy);
                assign("workerRushWindow", x.workerRushWindow);
                assign("workerRushMinWorkers", x.workerRushMinWorkers);
                assign("zealotFirstBy", x.zealotFirstBy);
                assign("zealotSecondBy", x.zealotSecondBy);
                assign("gatewaySecondBy", x.gatewaySecondBy);
                assign("proxyWindow", x.proxyWindow);
                assign("proxyMainDist", x.proxyMainDist);
                assign("proxyNaturalDist", x.proxyNaturalDist);
                assign("proxyFirstPylonBy", x.proxyFirstPylonBy);
                assign("proxyFirstProdBy", x.proxyFirstProdBy);
                assign("proxySecondPylonBy", x.proxySecondPylonBy);
                assign("zealotAllInEarlyWindow", x.zealotAllInEarlyWindow);
                assign("zealotAllInEarlyZealotsBy", x.zealotAllInEarlyZealotsBy);
                assign("zealotAllInEarlyZealotCount", x.zealotAllInEarlyZealotCount);
                assign("zealotAllInMidWindow", x.zealotAllInMidWindow);
                assign("zealotAllInMidZealotsBy", x.zealotAllInMidZealotsBy);
                assign("zealotAllInMidZealotCount", x.zealotAllInMidZealotCount);
                assign("zealotAllInMidLiveZealots", x.zealotAllInMidLiveZealots);
                assign("zealotAllInLateZealotsBy", x.zealotAllInLateZealotsBy);
                assign("zealotAllInLateZealotCount", x.zealotAllInLateZealotCount);
                assign("dragoonAllInWindow", x.dragoonAllInWindow);
                assign("dragoonAllInGateCount", x.dragoonAllInGateCount);
                assign("earlyRoboBy", x.earlyRoboBy);
                assign("dtRushArchivesBy", x.dtRushArchivesBy);
                assign("dtRushFirstDtBy", x.dtRushFirstDtBy);
                assign("turtleCannon2By", x.turtleCannon2By);
                assign("turtleCannon3By", x.turtleCannon3By);
                assign("turtleCannon4By", x.turtleCannon4By);
                assign("turtleCannon5By", x.turtleCannon5By);
                assign("dtSuspectCoreAgeBy", x.dtSuspectCoreAgeBy);
                assign("dtSuspectWindow", x.dtSuspectWindow);
                assign("midGameFrom", x.midGameFrom);
            }
            return x;
        }();
        return instance;
    }

    bool isFastExpansion()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Nexus, rt().fastExpansionNexusBy, 2);
    }

    bool isWorkerRush()
    {
        return RecognizerShared::isWorkerRush(rt().workerRushWindow, rt().workerRushMinWorkers);
    }

    bool isZealotRush()
    {
        if (frameNow >= rt().workerRushWindow) return false;

        /* We expect a zealot rush if we see an early zealot, early second zealot or early second gateway */
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, rt().zealotFirstBy) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, rt().zealotSecondBy, 2) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Gateway, rt().gatewaySecondBy, 2);
    }

    bool isProxy()
    {
        if (isFastExpansion()) return false;
        if (frameNow >= rt().proxyWindow) return false;
        if (UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Assimilator) > 0) return false;

        /* Check if we have directly scouted an enemy building in a proxy location */
        auto enemyMain = Map::getEnemyStartingMain();
        auto enemyNatural = Map::getEnemyStartingNatural();
        for (const auto &enemyUnit : UnitMgr::allEnemy())
        {
            if (!enemyUnit->type.isBuilding()) continue;
            if (enemyUnit->type.isRefinery()) continue; // Don't count gas steals
            if (enemyUnit->type.isResourceDepot()) continue; // Don't count expansions

            /* If the enemy main is unknown, this means the Map didn't consider this building to be part of that base and it indicates a proxy */
            if (!enemyMain) return true;

            /* Otherwise consider this a proxy if the building is not close to the enemy main or natural */
            int span = Pathfinder::GetGroundDistance(
                    enemyUnit->prevPos,
                    enemyMain->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);
            if (span != -1 && span < rt().proxyMainDist)
            {
                continue;
            }

            if (enemyNatural)
            {
                int naturalDist = Pathfinder::GetGroundDistance(
                        enemyUnit->prevPos,
                        enemyNatural->getPosition(),
                        BWAPI::UnitTypes::Protoss_Probe,
                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                if (naturalDist != -1 && naturalDist < rt().proxyNaturalDist)
                {
                    continue;
                }
            }

            return true;
        }

        /* If the enemy main has been scouted, determine if there is a proxy by looking at what they have built */
        if (Director::hasWorkerScoutCompletedInitialBaseScan())
        {
            /* Expect first pylon by frame 1300 */
            if (frameNow > rt().proxyFirstPylonBy && !countAtLeast(BWAPI::UnitTypes::Protoss_Pylon, 1)) return true;

            /* Expect first gateway or forge by frame 2200 */
            /* This will sometimes fail if the enemy does a fast expansion we don't see */
            if (frameNow > rt().proxyFirstProdBy
                && !countAtLeast(BWAPI::UnitTypes::Protoss_Gateway, 1)
                && !countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1))
            {
                return true;
            }

            /* If the enemy hasn't built a forge, expect a second pylon by frame 4000 if we still have a live scout */
            if (frameNow > rt().proxySecondPylonBy &&
                Director::hasWorkerScoutCompletedInitialBaseScan() &&
                !Director::isWorkerScoutComplete() &&
                !countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1) &&
                !countAtLeast(BWAPI::UnitTypes::Protoss_Pylon, 2))
            {
                return true;
            }

            return false;
        }

        return false;
    }

    bool isZealotAllIn()
    {
        /* In the early game, we consider the enemy to be doing a zealot all-in if we either see a lot of zealots or see two gateways with */
        /* no core or gas */
        if (frameNow < rt().zealotAllInEarlyWindow)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, rt().zealotAllInEarlyZealotsBy, rt().zealotAllInEarlyZealotCount) ||
                   (noneCreated(BWAPI::UnitTypes::Protoss_Assimilator) &&
                    noneCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core) &&
                    countAtLeast(BWAPI::UnitTypes::Protoss_Gateway, 2));
        }

        /* Later on, we consider it to be a zealot all-in purely based on the counts */
        if (frameNow < rt().zealotAllInMidWindow)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, rt().zealotAllInMidZealotsBy, rt().zealotAllInMidZealotCount) &&
                   UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) > rt().zealotAllInMidLiveZealots;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Zealot, rt().zealotAllInLateZealotsBy, rt().zealotAllInLateZealotCount) &&
               UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) > rt().zealotAllInMidLiveZealots;
    }

    bool isDragoonAllIn()
    {
        /* It's unlikely our worker scout survives long enough, but detect this if the enemy gets four gates before robo / citadel */
        if (frameNow < rt().dragoonAllInWindow &&
            createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, rt().dragoonAllInGateCount, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1) &&
            createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, rt().dragoonAllInGateCount, BWAPI::UnitTypes::Protoss_Templar_Archives, 1))
        {
            return true;
        }

        /* Check for four gates and no expansion from economic model */
        if (OpponentEconomicModel::enabled() &&
            !createdBeforeFrame(BWAPI::UnitTypes::Protoss_Nexus, frameNow, 2) &&
            OpponentEconomicModel::minimumProducerCount(BWAPI::UnitTypes::Protoss_Gateway) >= rt().dragoonAllInGateCount &&
            !OpponentEconomicModel::hasBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility) &&
            !OpponentEconomicModel::hasBuilt(BWAPI::UnitTypes::Protoss_Templar_Archives))
        {
            return true;
        }

        return false;
    }

    bool isEarlyRobo()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Robotics_Facility, rt().earlyRoboBy);
    }

    bool isDarkTemplarRush()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Templar_Archives, rt().dtRushArchivesBy) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Dark_Templar, rt().dtRushFirstDtBy);
    }

    /* AIIDE2025 absorb (report P1, 2026-09-17): reverse fingerprint - enemy
     * built a core long ago yet never finished goon range and has no robotics:
     * PW FingerprintDTRush.scala:19-21 / BB Strategy.cpp:65-72 analog. Warns
     * one beat earlier than waiting for Archives to be scouted. */
    bool isSuspectedDT()
    {
        auto &timings = UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
        if (timings.empty()) return false;
        int coreBuilt = timings.begin()->first;
        if (coreBuilt <= 0) return false;
        if (frameNow < coreBuilt + rt().dtSuspectCoreAgeBy) return false;
        if (frameNow > rt().dtSuspectWindow) return false;
        if (PlayerInfo::upgradeLevel(BWAPI::Broodwar->enemy(), BWAPI::UpgradeTypes::Singularity_Charge) > 0) return false;
        return !UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility);
    }

    /* dt_suspect guard: off = inactive (behavior identical to pre-absorb);
     * record = log the first activation per game to the write-dir diagnostics
     * channel and hold behavior; on = the transition fires. */
    bool dtSuspectGuard()
    {
        int mode = Config::dtSuspectMode();
        if (mode == 0) return false;

        static bool conditionActive = false;
        if (frameNow < 100) conditionActive = false;

        if (!isSuspectedDT())
        {
            conditionActive = false;
            return false;
        }

        if (mode == 1)
        {
            if (!conditionActive)
            {
                conditionActive = true;
                std::ofstream out(FileTools::getFilePath("shushanking_dt_suspect", "txt", true),
                                  std::ofstream::app);
                if (out.is_open()) out << frameNow << ",suspected\n";
            }
            return false;
        }

        return true;
    }

    bool isTurtle()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, rt().turtleCannon2By, 2) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, rt().turtleCannon3By, 3) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, rt().turtleCannon4By, 4) ||
               createdBeforeFrame(BWAPI::UnitTypes::Protoss_Photon_Cannon, rt().turtleCannon5By, 5);
    }

    bool isMidGame()
    {
        /* We consider ourselves to be in the mid-game after frame 14000 if the enemy has taken their natural or teched to something beyond goons */
        if (frameNow < rt().midGameFrom) return false;

        return countAtLeast(BWAPI::UnitTypes::Protoss_Nexus, 2) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Templar_Archives, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Dark_Templar, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_High_Templar, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Dark_Archon, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Archon, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Observatory, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Shuttle, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Reaver, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Observer, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Stargate, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Fleet_Beacon, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Scout, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Corsair, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Carrier, 1) ||
               countAtLeast(BWAPI::UnitTypes::Protoss_Arbiter, 1);
    }

    /* ===== P2-E: declaration-table recognition (dedup plan) ===== */
    /* Same predicates, same thresholds, same per-state guard order as the switch */
    /* path below; the state machine itself becomes data (RecognizerShared */
    /* transition table). Shadow-verified against the switch path before it */
    /* becomes authoritative. */

    using PS = PvP::ProtossStrategy;
    using PTransition = RecognizerShared::STransition<PS>;

    /* The OneZealotCore reconsider guard in the switch path reads the COMMITTED */
    /* member state (enemyStrategy), not the loop-local chase position; mirror */
    /* that exactly by latching "the call started from OneZealotCore" per call. */
    bool g_startedFromOneZealotCore = false;

    const std::vector<PTransition> &protossTransitionTable()
    {
        static const std::vector<PTransition> table = [] {
            std::vector<PTransition> t;
            auto add = [&t](PS from, bool (*guard)(), PS to, bool chase) { t.push_back(PTransition{from, chase, to, guard}); };

            /* Unknown: rush classification first, then the scouting-driven */
            /* opening determination (with the economic-model NoZealot split) */
            add(PS::Unknown, []{ return isWorkerRush(); }, PS::WorkerRush, false);
            add(PS::Unknown, []{ return isZealotRush(); }, PS::ZealotRush, false);
            add(PS::Unknown, []{ return isProxy(); }, PS::ProxyRush, false);
            add(PS::Unknown, []{ return Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::ScoutingBlocked; }, PS::BlockScouting, true);
            add(PS::Unknown, []{ return isFastExpansion(); }, PS::FastExpansion, false);
            add(PS::Unknown, []{ return isEarlyRobo(); }, PS::EarlyRobo, false);
            /* Early forge if we have seen either a forge or a cannon */
            add(PS::Unknown, []{ return countAtLeast(BWAPI::UnitTypes::Protoss_Forge, 1) || countAtLeast(BWAPI::UnitTypes::Protoss_Photon_Cannon, 1); }, PS::EarlyForge, true);
            /* Transition to one- or two-gate on the appropriate scouting information */
            add(PS::Unknown, []{ return createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 2, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1) &&
                                        createdBeforeUnit(BWAPI::UnitTypes::Protoss_Gateway, 2, BWAPI::UnitTypes::Protoss_Assimilator, 1); }, PS::TwoGate, true);
            add(PS::Unknown, []{ return createdBeforeUnit(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, BWAPI::UnitTypes::Protoss_Gateway, 2) &&
                                        OpponentEconomicModel::enabled() &&
                                        OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot).second == 0; }, PS::NoZealotCore, true);
            add(PS::Unknown, []{ return createdBeforeUnit(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, BWAPI::UnitTypes::Protoss_Gateway, 2); }, PS::OneZealotCore, true);
            /* Assume one-zealot if we scout the assimilator first */
            add(PS::Unknown, []{ return createdBeforeUnit(BWAPI::UnitTypes::Protoss_Assimilator, 1, BWAPI::UnitTypes::Protoss_Gateway, 2); }, PS::OneZealotCore, true);
            /* Default to something reasonable if our scouting completely fails */
            add(PS::Unknown, []{ return frameNow > 4000; }, PS::OneZealotCore, true);

            add(PS::WorkerRush, []{ return !isWorkerRush(); }, PS::Unknown, true);

            add(PS::ProxyRush, []{ return isWorkerRush(); }, PS::WorkerRush, false);
            /* Handle a misdetected proxy, can happen if the enemy does a fast expand or builds further away from their nexus */
            add(PS::ProxyRush, []{ return frameNow < 5000 && !isProxy(); }, PS::Unknown, true);
            /* Transition out of the proxy when they take gas, or the scout is dead past frame 5000 */
            add(PS::ProxyRush, []{ return UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Assimilator) > 0 ||
                                        (frameNow >= 5000 && Director::isWorkerScoutComplete()); }, PS::TwoGate, true);

            add(PS::ZealotRush, []{ return isWorkerRush(); }, PS::WorkerRush, false);
            /* Consider the rush to be over after 6000 frames */
            add(PS::ZealotRush, []{ return frameNow >= 6000; }, PS::TwoGate, true);

            add(PS::EarlyForge, []{ return isWorkerRush(); }, PS::WorkerRush, false);
            add(PS::EarlyForge, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::EarlyForge, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::EarlyForge, []{ return isProxy(); }, PS::ProxyRush, false);
            add(PS::EarlyForge, []{ return isFastExpansion(); }, PS::FastExpansion, true);
            add(PS::EarlyForge, []{ return isEarlyRobo(); }, PS::EarlyRobo, true);
            add(PS::EarlyForge, []{ return isTurtle(); }, PS::Turtle, true);
            add(PS::EarlyForge, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::TwoGate, []{ return isWorkerRush(); }, PS::WorkerRush, false);
            /* We might detect a zealot rush late on large maps or if scouting is denied */
            add(PS::TwoGate, []{ return isZealotRush(); }, PS::ZealotRush, false);
            add(PS::TwoGate, dtSuspectGuard, PS::DarkTemplarRush, false);
            add(PS::TwoGate, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::TwoGate, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::TwoGate, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::TwoGate, []{ return isEarlyRobo(); }, PS::EarlyRobo, true);
            add(PS::TwoGate, []{ return isTurtle(); }, PS::Turtle, true);
            add(PS::TwoGate, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::NoZealotCore, dtSuspectGuard, PS::DarkTemplarRush, false);
            add(PS::NoZealotCore, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::NoZealotCore, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::NoZealotCore, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::NoZealotCore, []{ return isEarlyRobo(); }, PS::EarlyRobo, true);
            add(PS::NoZealotCore, []{ return isTurtle(); }, PS::Turtle, true);
            add(PS::NoZealotCore, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::OneZealotCore, dtSuspectGuard, PS::DarkTemplarRush, false);
            add(PS::OneZealotCore, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::OneZealotCore, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::OneZealotCore, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::OneZealotCore, []{ return isEarlyRobo(); }, PS::EarlyRobo, true);
            add(PS::OneZealotCore, []{ return isTurtle(); }, PS::Turtle, true);
            add(PS::OneZealotCore, []{ return isMidGame(); }, PS::MidGame, true);
            /* Reconsider one-zealot if we have just discovered a core or assimilator */
            /* (only when the committed state is OneZealotCore, as in the switch path) */
            add(PS::OneZealotCore, []{ return g_startedFromOneZealotCore &&
                                        (justDiscovered(BWAPI::UnitTypes::Protoss_Cybernetics_Core) || justDiscovered(BWAPI::UnitTypes::Protoss_Assimilator)) &&
                                        OpponentEconomicModel::enabled() &&
                                        OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot).second == 0; }, PS::NoZealotCore, true);

            add(PS::FastExpansion, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::FastExpansion, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::FastExpansion, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::FastExpansion, []{ return isEarlyRobo(); }, PS::EarlyRobo, true);
            add(PS::FastExpansion, []{ return isTurtle(); }, PS::Turtle, true);
            add(PS::FastExpansion, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::BlockScouting, []{ return isWorkerRush(); }, PS::WorkerRush, false);
            add(PS::BlockScouting, []{ return isProxy(); }, PS::ProxyRush, false);
            add(PS::BlockScouting, []{ return isZealotRush(); }, PS::ZealotRush, false);
            add(PS::BlockScouting, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::BlockScouting, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::BlockScouting, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::BlockScouting, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::ZealotAllIn, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::ZealotAllIn, []{ return !isZealotAllIn(); }, PS::TwoGate, true);
            add(PS::ZealotAllIn, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::DragoonAllIn, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::DragoonAllIn, []{ return !isDragoonAllIn() && isEarlyRobo(); }, PS::EarlyRobo, true);
            add(PS::DragoonAllIn, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::Turtle, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::Turtle, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::Turtle, []{ return isProxy(); }, PS::ProxyRush, false);
            add(PS::Turtle, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::Turtle, []{ return isEarlyRobo(); }, PS::EarlyRobo, true);
            add(PS::Turtle, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::EarlyRobo, []{ return isZealotAllIn(); }, PS::ZealotAllIn, false);
            add(PS::EarlyRobo, []{ return isDragoonAllIn(); }, PS::DragoonAllIn, false);
            add(PS::EarlyRobo, []{ return isDarkTemplarRush(); }, PS::DarkTemplarRush, false);
            add(PS::EarlyRobo, []{ return isMidGame(); }, PS::MidGame, true);

            add(PS::DarkTemplarRush, []{ return !isDarkTemplarRush(); }, PS::OneZealotCore, true);
            add(PS::DarkTemplarRush, []{ return isMidGame(); }, PS::MidGame, true);

            /* MidGame is terminal (switch case falls through to return) */

            return t;
        }();
        return table;
    }
}

PvP::ProtossStrategy PvP::recognizeEnemyStrategyTable(PvP::ProtossStrategy current)
{
    /* Latch the committed state for the OneZealotCore reconsider guard (see the */
    /* table comment): the switch path read the member, not the chase position. */
    g_startedFromOneZealotCore = (current == ProtossStrategy::OneZealotCore);

    bool exhausted = false;
    auto outcome = RecognizerShared::runTransitions(current, protossTransitionTable(), exhausted);
    if (exhausted) AppLog::Get() << "ERROR: Loop in strategy recognizer (table path)";
    return outcome;
}

PvP::ProtossStrategy PvP::recognizeEnemyStrategy()
{
    /* P2-E authority flip (2026-09-16): the declaration-table path above is now */
    /* the single implementation. The switch-based predecessor was shadow-verified */
    /* with zero divergence across 7 games (1 trigger proof + 6 identity games: */
    /* 3 BananaBrain + 3 PurpleWave, the sensitive lines) before this flip. */
    return recognizeEnemyStrategyTable(enemyStrategy);
}
