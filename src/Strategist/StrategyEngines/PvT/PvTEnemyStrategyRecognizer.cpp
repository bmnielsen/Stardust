#include "StrategyEngines/PvT.h"

#include "Config.h"
#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "UnitUtil.h"
#include "RecognizerShared.h"

/* P2 step A (dedup plan): shared observation predicates live in RecognizerShared.h. */
using namespace RecognizerShared;

std::map<PvT::TerranStrategy, std::string> PvT::TerranStrategyNames = {
        {TerranStrategy::Unknown,       "Unknown"},
        {TerranStrategy::WorkerRush,    "WorkerRush"},
        {TerranStrategy::ProxyRush,     "ProxyRush"},
        {TerranStrategy::MarineRush,    "MarineRush"},
        {TerranStrategy::WallIn,        "WallIn"},
        {TerranStrategy::BlockScouting, "BlockScouting"},
        {TerranStrategy::FastExpansion, "FastExpansion"},
        {TerranStrategy::TwoFactory,    "TwoFactory"},
        {TerranStrategy::NormalOpening, "Normal"},
        {TerranStrategy::MidGameMech,   "MidGameMech"},
        {TerranStrategy::MidGameBio,    "MidGameBio"},
};

namespace
{
    /* Declarative thresholds (S9-merge): defaults are the audited literals so a */
    /* missing/partial recognizers.json is behavior-identical. */
    struct PvTRecognizerThresholds
    {
        int fastExpansionCCBy = 7000;
        int workerRushWindow = 6000;
        int workerRushMinWorkers = 2;
        int marineFirstBy = 3500;
        int barracksSecondBy = 2300;
        int marineAllInEarlyBy = 5000;
        int marineAllInEarlyCount = 4;
        int marineAllInMidWindow = 8000;
        int marineAllInMidBy = 7000;
        int marineAllInMidCount = 8;
        int marineAllInMidLiveMarines = 4;
        int marineAllInLateBy = 9000;
        int marineAllInLateCount = 12;
        int proxyWindow = 6000;
        int proxyMainDist = 1500;
        int proxyNaturalDist = 640;
        int proxyFirstProdBy = 2400;
        int wallInWindow = 6000;
        int midGameFrom = 8000;
        int midGameTankLateFrom = 10000;
        int midGameTankEarlyCount = 4;
        int midGameTankLateCount = 2;
        int bioLowThreshold = 10;
        int blockScoutReleaseBy = 5000;
        int normalOpeningBy = 4000;
    };

    const PvTRecognizerThresholds &rt()
    {
        static const PvTRecognizerThresholds instance = []
        {
            PvTRecognizerThresholds x;
            const auto &j = Config::recognizers();
            auto iterRef = j.find("PvT");
            if (iterRef != j.end() && iterRef->is_object())
            {
                auto assign = [&iterRef](const char *key, int &field)
                {
                    auto f = iterRef->find(key);
                    if (f != iterRef->end() && f->is_number_integer()) field = f->get<int>();
                };
                assign("fastExpansionCCBy", x.fastExpansionCCBy);
                assign("workerRushWindow", x.workerRushWindow);
                assign("workerRushMinWorkers", x.workerRushMinWorkers);
                assign("marineFirstBy", x.marineFirstBy);
                assign("barracksSecondBy", x.barracksSecondBy);
                assign("marineAllInEarlyBy", x.marineAllInEarlyBy);
                assign("marineAllInEarlyCount", x.marineAllInEarlyCount);
                assign("marineAllInMidWindow", x.marineAllInMidWindow);
                assign("marineAllInMidBy", x.marineAllInMidBy);
                assign("marineAllInMidCount", x.marineAllInMidCount);
                assign("marineAllInMidLiveMarines", x.marineAllInMidLiveMarines);
                assign("marineAllInLateBy", x.marineAllInLateBy);
                assign("marineAllInLateCount", x.marineAllInLateCount);
                assign("proxyWindow", x.proxyWindow);
                assign("proxyMainDist", x.proxyMainDist);
                assign("proxyNaturalDist", x.proxyNaturalDist);
                assign("proxyFirstProdBy", x.proxyFirstProdBy);
                assign("wallInWindow", x.wallInWindow);
                assign("midGameFrom", x.midGameFrom);
                assign("midGameTankLateFrom", x.midGameTankLateFrom);
                assign("midGameTankEarlyCount", x.midGameTankEarlyCount);
                assign("midGameTankLateCount", x.midGameTankLateCount);
                assign("bioLowThreshold", x.bioLowThreshold);
                assign("blockScoutReleaseBy", x.blockScoutReleaseBy);
                assign("normalOpeningBy", x.normalOpeningBy);
            }
            return x;
        }();
        return instance;
    }

    bool isFastExpansion()
    {
        return createdBeforeFrame(BWAPI::UnitTypes::Terran_Command_Center, rt().fastExpansionCCBy, 2);
    }

    bool isWorkerRush()
    {
        return RecognizerShared::isWorkerRush(rt().workerRushWindow, rt().workerRushMinWorkers);
    }

    bool isMarineRush()
    {
        /* In the early game, we consider the enemy to be doing a marine rush or all-in if we either see a lot of marines */
        /* or see two barracks with no factory or gas */
        if (frameNow < rt().workerRushWindow)
        {
            /* Early rushes */
            if (createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, rt().marineFirstBy, 2) ||
                createdBeforeFrame(BWAPI::UnitTypes::Terran_Barracks, rt().barracksSecondBy, 2))
            {
                return true;
            }

            /* Later all-ins */
            return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, rt().marineAllInEarlyBy, rt().marineAllInEarlyCount) ||
                   (noneCreated(BWAPI::UnitTypes::Terran_Refinery) &&
                    noneCreated(BWAPI::UnitTypes::Terran_Factory) &&
                    countAtLeast(BWAPI::UnitTypes::Terran_Barracks, 2));
        }

        /* Later on, we consider it to be a marine all-in purely based on the counts */
        if (frameNow < rt().marineAllInMidWindow)
        {
            return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, rt().marineAllInMidBy, rt().marineAllInMidCount) &&
                   UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Marine) > rt().marineAllInMidLiveMarines;
        }

        return createdBeforeFrame(BWAPI::UnitTypes::Terran_Marine, rt().marineAllInLateBy, rt().marineAllInLateCount) &&
               UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Marine) > rt().marineAllInMidLiveMarines;
    }

    bool isProxy()
    {
        if (frameNow >= rt().proxyWindow) return false;
        if (UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Refinery) > 0) return false;

        /* Otherwise check if we have directly scouted an enemy building in a proxy location */
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
            /* Expect first barracks, refinery or command center by frame 2400 */
            if (frameNow > rt().proxyFirstProdBy
                && !countAtLeast(BWAPI::UnitTypes::Terran_Barracks, 1)
                && !countAtLeast(BWAPI::UnitTypes::Terran_Refinery, 1)
                && !countAtLeast(BWAPI::UnitTypes::Terran_Command_Center, 2))
            {
                return true;
            }

            return false;
        }

        return false;
    }

    bool isWallIn()
    {
        /* The enemy is considered walled-in if our scout was unable to get into the base and */
        /* there is no path from our main to their base */

        /* TODO: This is too strict - the scout might get into the base before the wall-in is finished */

        if (Director::getWorkerScoutStatus() != Director::WorkerScoutStatus::ScoutingBlocked) return false;
        if (frameNow > rt().wallInWindow) return false;

        auto enemyMain = Map::getEnemyStartingMain();
        auto mainChoke = Map::getMyMainChoke();
        if (!enemyMain || !mainChoke) return false;

        auto grid = Pathfinder::getNavigationGrid(enemyMain->getPosition());
        if (!grid) return false;

        auto &node = (*grid)[mainChoke->midpoint];
        return node.nextNode == nullptr;
    }

    bool isMidGame()
    {
        /* Never in the mid-game before frame 8000 */
        if (frameNow < rt().midGameFrom) return false;

        /* We consider ourselves to be in the mid-game if the enemy has expanded */

        /* Scouted expansion */
        if (countAtLeast(BWAPI::UnitTypes::Terran_Command_Center, 2)) return true;

        /* Inferred taken natural, usually by seeing a bunker */
        auto enemyNatural = Map::getEnemyStartingNatural();
        if (enemyNatural && enemyNatural->owner == BWAPI::Broodwar->enemy()) return true;

        /* We consider ourselves to be in the mid-game if the enemy has siege tech and a number of siege tanks */
        return UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) &&
               (UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)) >
                   (frameNow > rt().midGameTankLateFrom ? rt().midGameTankLateCount
                                                            : rt().midGameTankEarlyCount);
    }

    bool isMidGameMech()
    {
        int mech = UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                   UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) +
                   UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Vulture) +
                   UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Goliath);
        int bio = UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Marine) +
                  UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Medic) +
                  UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Firebat);

        /* Mech if they don't have many bio units */
        if (bio < rt().bioLowThreshold) return true;

        /* Mech if they have less than twice as many bio units compared to mech */
        return mech >= (bio / 2);
    }

    /* ===== P2-D: declaration-table recognition (dedup plan) ===== */
    /* Same predicates, same thresholds, same per-state guard order as the switch */
    /* path below; the state machine itself becomes data (RecognizerShared */
    /* transition table). Shadow-verified against the switch path before it */
    /* becomes authoritative. */

    using TS = PvT::TerranStrategy;
    using TTransition = RecognizerShared::STransition<TS>;

    const std::vector<TTransition> &terranTransitionTable()
    {
        static const std::vector<TTransition> table = [] {
            std::vector<TTransition> t;
            auto add = [&t](TS from, bool (*guard)(), TS to, bool chase) { t.push_back(TTransition{from, chase, to, guard}); };

            /* Unknown: rush/proxy/wall-in/FE classification, then the */
            /* block-scouting and two-factory chases, then the normal default */
            add(TS::Unknown, []{ return isWorkerRush(); }, TS::WorkerRush, false);
            add(TS::Unknown, []{ return isMarineRush(); }, TS::MarineRush, false);
            add(TS::Unknown, []{ return isProxy(); }, TS::ProxyRush, false);
            add(TS::Unknown, []{ return isWallIn(); }, TS::WallIn, false);
            add(TS::Unknown, []{ return isFastExpansion(); }, TS::FastExpansion, false);
            add(TS::Unknown, []{ return Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::ScoutingBlocked; }, TS::BlockScouting, true);
            add(TS::Unknown, []{ return countAtLeast(BWAPI::UnitTypes::Terran_Factory, 2); }, TS::TwoFactory, true);
            add(TS::Unknown, []{ return frameNow > rt().normalOpeningBy ||
                                        Director::hasWorkerScoutCompletedInitialBaseScan(); }, TS::NormalOpening, true);

            add(TS::WorkerRush, []{ return !isWorkerRush(); }, TS::Unknown, true);

            add(TS::ProxyRush, []{ return isWorkerRush(); }, TS::WorkerRush, false);
            /* Handle a misdetected proxy, can happen if the enemy does a fast expand or builds further away from their depot */
            add(TS::ProxyRush, []{ return frameNow < 6000 && !isProxy(); }, TS::Unknown, true);
            /* Transition out when they take gas, or the scout is dead past frame 5000 */
            add(TS::ProxyRush, []{ return UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Refinery) > 0 ||
                                        (frameNow >= 6000 && Director::isWorkerScoutComplete()); }, TS::Unknown, true);
            /* Bail out if we have more dragoons than the enemy has marines */
            add(TS::ProxyRush, []{ return frameNow >= 6000 &&
                                        UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Marine) < UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon); }, TS::Unknown, true);

            add(TS::MarineRush, []{ return isWorkerRush(); }, TS::WorkerRush, false);
            /* Consider the rush to be over after 6000 frames */
            add(TS::MarineRush, []{ return frameNow >= 6000; }, TS::NormalOpening, true);

            add(TS::WallIn, []{ return isWorkerRush(); }, TS::WorkerRush, false);
            add(TS::WallIn, []{ return isMarineRush(); }, TS::MarineRush, false);
            add(TS::WallIn, []{ return isFastExpansion(); }, TS::FastExpansion, false);
            add(TS::WallIn, []{ return isMidGame(); }, TS::MidGameMech, true);

            add(TS::BlockScouting, []{ return isWorkerRush(); }, TS::WorkerRush, false);
            add(TS::BlockScouting, []{ return isProxy(); }, TS::ProxyRush, false);
            add(TS::BlockScouting, []{ return isMarineRush(); }, TS::MarineRush, false);
            add(TS::BlockScouting, []{ return isFastExpansion(); }, TS::FastExpansion, false);
            /* If we haven't had any evidence of a rush for about 3 1/2 minutes, assume the enemy is opening normally */
            add(TS::BlockScouting, []{ return frameNow > rt().blockScoutReleaseBy; }, TS::NormalOpening, true);
            add(TS::BlockScouting, []{ return isMidGame(); }, TS::MidGameMech, true);

            /* TwoFactory, FastExpansion and NormalOpening share the same guards */
            for (TS s : {TS::TwoFactory, TS::FastExpansion, TS::NormalOpening})
            {
                add(s, []{ return isWorkerRush(); }, TS::WorkerRush, false);
                add(s, []{ return isProxy(); }, TS::ProxyRush, false);
                add(s, []{ return isMarineRush(); }, TS::MarineRush, false);
                add(s, []{ return isMidGame(); }, TS::MidGameMech, true);
            }

            add(TS::MidGameMech, []{ return !isMidGameMech(); }, TS::MidGameBio, true);
            add(TS::MidGameBio, []{ return isMidGameMech(); }, TS::MidGameMech, true);

            return t;
        }();
        return table;
    }
}

PvT::TerranStrategy PvT::recognizeEnemyStrategyTable(PvT::TerranStrategy current)
{
    bool exhausted = false;
    auto outcome = RecognizerShared::runTransitions(current, terranTransitionTable(), exhausted);
    if (exhausted) AppLog::Get() << "ERROR: Loop in strategy recognizer (table path)";
    return outcome;
}

PvT::TerranStrategy PvT::recognizeEnemyStrategy()
{
    /* P2-D authority flip (2026-09-16): the declaration-table path above is now */
    /* the single implementation. The switch-based predecessor was shadow-verified */
    /* with zero divergence across 5 shadow games (incl. a 28.5k-frame long game) */
    /* plus the harness trigger proof, on top of the pattern proven for PvZ. */
    return recognizeEnemyStrategyTable(enemyStrategy);
}
