#include "StrategyEngines/PvP.h"

#include "Units.h"
#include "Map.h"
#include "Builder.h"
#include "UnitUtil.h"
#include "Strategist.h"
#include "Workers.h"
#include "Opponent.h"
#include "OpponentEconomicModel.h"
#include "StrategySelector.h"
#include "Config.h"
#include "FileTools.h"

#include <fstream>

#include "Plays/Macro/HiddenBase.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/MainArmy/ForgeFastExpand.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Plays/Scouting/EjectEnemyScout.h"
#include "Plays/Defensive/AntiCannonRush.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define OUTPUT_DETECTION_DEBUG false
#endif

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;

    /* P1-A/P1-B (2026-09-20 report v2): rough army value (mineral+gas price of */
    /* completed combat units) - forward declared for the window_push trigger */
    int armyValue(bool mine);
}

void PvP::initialize(std::vector<std::shared_ptr<PlayItem>> &playList, bool transitioningFromRandom, const std::string &openingOverride)
{
    if (!openingOverride.empty()) playList.clear();

    if (transitioningFromRandom)
    {
        playList.emplace(beforePlayIt<MainArmyPlay>(playList), std::make_shared<EjectEnemyScout>());

        if (getPlay<ForgeFastExpand>(playList))
        {
            ourStrategy = OurStrategy::ForgeExpandDT;
        }
    }
    else
    {
        playList.emplace_back(std::make_shared<SaturateBases>());
        playList.emplace_back(std::make_shared<EarlyGameWorkerScout>());
        playList.emplace_back(std::make_shared<EjectEnemyScout>());

        std::string opening = openingOverride;
        if (opening.empty())
        {
            /* L2 seam (D6-9): JSON strategy library picks first (legality + Tier-0). */
            /* Returns "" when strategy_engine=legacy or nothing legal, in which case */
            /* the legacy selection path below runs unchanged. */
            opening = StrategySelector::selectOpening("PvP");
        }
        if (opening.empty()) opening = StrategySelector::selectOpeningScored("PvP");
        if (opening == OurStrategyNames[OurStrategy::ForgeExpandDT] && BuildingPlacement::hasForgeGatewayWall())
        {
            playList.emplace_back(std::make_shared<ForgeFastExpand>());
            ourStrategy = OurStrategy::ForgeExpandDT;
        }
        else
        {
            playList.emplace_back(std::make_shared<DefendMyMain>());

            if (opening == OurStrategyNames[OurStrategy::TwoGateDT])
            {
                ourStrategy = OurStrategy::TwoGateDT;
            }
            else if (opening == OurStrategyNames[OurStrategy::OneGateCoreNoZealot])
            {
                ourStrategy = OurStrategy::OneGateCoreNoZealot;
            }
        }
        AppLog::Get() << "Selected opening " << OurStrategyNames[ourStrategy];

/* plays.emplace_back(std::make_shared<HiddenBase>()); */
    }

    Opponent::addMyStrategyChange(OurStrategyNames[ourStrategy]);
    Opponent::addEnemyStrategyChange(ProtossStrategyNames[enemyStrategy]);
}

void PvP::updatePlays(std::vector<std::shared_ptr<PlayItem>> &playList)
{
    if (OpponentEconomicModel::enabled())
    {
        Telemetry::setBoardValue(
                "modelled-earliest-dt",
                (std::ostringstream() << OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Dark_Templar)).str());
        Telemetry::setBoardValue(
                "modelled-earliest-nexus",
                (std::ostringstream() << OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Nexus)).str());
        auto zealots = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
        auto dragoons = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);

        Telemetry::setBoardValue(
                "modelled-zealots-now",
                (std::ostringstream() << "[" << zealots.first << "," << zealots.second << "]").str());
        Telemetry::setBoardValue(
                "modelled-dragoons-now",
                (std::ostringstream() << "[" << dragoons.first << "," << dragoons.second << "]").str());
    }

    auto newEnemyStrategy = recognizeEnemyStrategy();

    /* window_push (2026-10-08 funnel plan, B lever): one-shot all-in trigger.
     * The funnel shows only 9.1% of games where we hold an army lead get
     * converted before 15 minutes, while 81% of losses are continuous losing
     * fights after the enemy storm forms. When our army is meaningfully ahead
     * inside the pre-storm window, commit everything: the attack play locks
     * onto the enemy main and ignores combat sim, worker production freezes
     * and gateways spam (consumers read WindowPush::fired). One shot per game.
     * The enemy army value only counts what we can see - hence the absolute
     * floor and two-base requirements. */
    if (Config::windowPushEnabled() && !Config::WindowPush::fired
        && frameNow >= Config::windowPushMinFrame()
        && frameNow <= Config::windowPushMinFrame() + 2400
        && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) >= 2
        && armyValue(true) >= 2500
        && (double) armyValue(true) >= Config::windowPushArmyRatio() * (double) std::max(armyValue(false), 1))
    {
        Config::WindowPush::fired = true;

        /* HT-pipe diagnostics: the push decision point */
        FileTools::appendLine("shushanking_htpipe",
                              (std::ostringstream() << "HTPIPE WPUSH frame=" << frameNow
                               << " myArmy=" << armyValue(true)
                               << " enemyArmy=" << armyValue(false)).str());
    }

    auto newStrategy = chooseOurStrategy(newEnemyStrategy, playList);

    if (enemyStrategy != newEnemyStrategy)
    {
#if LOGGING_ENABLED
        AppLog::Get() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#endif
#if CHERRYVIS_ENABLED
        Telemetry::log() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#endif

        enemyStrategy = newEnemyStrategy;
        enemyStrategyChanged = frameNow;
        Opponent::addEnemyStrategyChange(ProtossStrategyNames[enemyStrategy]);
    }

    if (ourStrategy != newStrategy)
    {
#if LOGGING_ENABLED
        AppLog::Get() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#endif
#if CHERRYVIS_ENABLED
        Telemetry::log() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#endif

        ourStrategy = newStrategy;
        Opponent::addMyStrategyChange(OurStrategyNames[ourStrategy]);
    }

    bool defendOurMain;
    if (hasEnemyStolenOurGas())
    {
        defendOurMain = true;
    }
    else
    {
        switch (ourStrategy)
        {
            case OurStrategy::ForgeExpandDT:
            {
                /* Attack when we have goon range and a completed DT */
                defendOurMain = (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0) ||
                                (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0);
                break;
            }

            case OurStrategy::TwoGateDT:
            {
                /* TODO */
                /* Attack with initial zealot groups via a different play that ignores sim */
                /* Transition to normal aggressive play after the first two DTs are out */
                defendOurMain = false;
                break;
            }

            case OurStrategy::EarlyGameDefense:
            case OurStrategy::AntiZealotRush:
            case OurStrategy::AntiDarkTemplarRush:
            case OurStrategy::Defensive:
                defendOurMain = true;
                break;
            case OurStrategy::DTExpand:
            {
                /* Wait until we have at least completed two dark templar that aren't in our main */
                auto &mainAreas = Map::getMyMainAreas();
                int dtCount = 0;
                for (const auto &gameUnit : UnitMgr::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Dark_Templar))
                {
                    auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos));
                    if (!mainAreas.contains(area)) dtCount++;
                }

                defendOurMain = dtCount < 2;
                break;
            }
            case OurStrategy::FastExpansion:
            case OurStrategy::Normal:
            case OurStrategy::ThreeGateRobo:
            case OurStrategy::MidGame:
            {
                auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
                if (!mainArmyPlay)
                {
                    defendOurMain = true;
                    break;
                }

                /* Always use a defend play if the squad has no units */
                auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                if (!vanguard)
                {
                    defendOurMain = true;
                    break;
                }

                /* Require range off of an FFE */
                if (getPlay<ForgeFastExpand>(playList) && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0)
                {
                    defendOurMain = true;
                    break;
                }

                /* Use a defend play if we are doing a 3 gate robo or the main army needs detection and doesn't have any observers */
                if ((ourStrategy == OurStrategy::ThreeGateRobo && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Observer) == 0)
                    || (mainArmyPlay->getSquad()->needsDetection() && mainArmyPlay->getSquad()->getDetectors().empty()))
                {
                    defendOurMain = true;
                    break;
                }

                defendOurMain = false;

                /* Transition from a defend squad when the vanguard cluster has 3 units and can do so */
                /* Exception: always attack if the enemy strategy is fast expansion */
                if (typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    defendOurMain = enemyStrategy != ProtossStrategy::FastExpansion &&
                                    (vanguard->units.size() < 3 ||
                                     !((DefendMyMain *) mainArmyPlay)->canTransitionToAttack());
                }

                /* Transition to a defend squad if our attack squad has been pushed back into our main and we haven't yet taken our natural */
                auto natBase = Map::getMyNatural();
                if (typeid(*mainArmyPlay) == typeid(AttackEnemyBase)
                    && (!natBase || natBase->owner != BWAPI::Broodwar->self() || !natBase->resourceDepot || !natBase->resourceDepot->completed
                        || Map::mapSpecificOverride()->hasBackdoorNatural()))
                {
                    if (vanguard->currentActivity == UnitCluster::Activity::Regrouping &&
                        vanguard->currentSubActivity == UnitCluster::SubActivity::Flee)
                    {
                        auto inMain = [](BWAPI::Position pt)
                        {
                            auto choke = Map::getMyMainChoke();
                            if (choke && choke->midpoint.getApproxDistance(pt) < 320) return true;

                            auto mainAreas = Map::getMyMainAreas();
                            return mainAreas.contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(pt)));
                        };

                        if (inMain(vanguard->midpoint) || (vanguard->vanguard && inMain(vanguard->vanguard->prevPos)))
                        {
                            defendOurMain = true;
                        }
                    }
                }

                break;
            }
        }
    }

    updateAttackPlays(playList, defendOurMain);

    /* Ensure we have an anti cannon rush play if the enemy strategy warrants it */
    if (frameNow < 4000)
    {
        /* If we've detected a proxy, keep track of whether we have identified it as a zealot rush */
        /* Once we've either seen the proxied gateway or a zealot, we assume the enemy isn't doing a "proxy" cannon rush */
        bool zealotProxy = enemyStrategy == ProtossStrategy::ProxyRush &&
                           (UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Gateway) || UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Zealot));

        auto antiCannonRushPlay = getPlay<AntiCannonRush>(playList);
        if (enemyStrategy == ProtossStrategy::EarlyForge ||
            (enemyStrategy == ProtossStrategy::ProxyRush && !zealotProxy) ||
            enemyStrategy == ProtossStrategy::BlockScouting ||
            (enemyStrategy == ProtossStrategy::Unknown && frameNow > 2000))
        {
            if (!antiCannonRushPlay)
            {
                playList.emplace(playList.begin(), std::make_shared<AntiCannonRush>());
            }
        }
        else if (antiCannonRushPlay && (
                enemyStrategy == ProtossStrategy::FastExpansion
                || enemyStrategy == ProtossStrategy::NoZealotCore
                || enemyStrategy == ProtossStrategy::OneZealotCore
                || enemyStrategy == ProtossStrategy::ZealotRush
                || enemyStrategy == ProtossStrategy::TwoGate
                || zealotProxy))
        {
            antiCannonRushPlay->safeEnemyStrategyDetermined = true;
        }
    }

    /* Direction 2 (2026-09-24 plan): DT-scout window. After the enemy core */
    /* completes and while no templar tech (Citadel/Archives) and no robo has */
    /* been sighted, the enemy is in the pre-DT decision window - keep a worker */
    /* scout alive on their main despite ranged threats. Sighting the Archives */
    /* feeds handleDetection an exact expected DT frame, and PW's source */
    /* abandons a scouted DT line with 0.65-0.85 probability. Off unless */
    /* params dt_scout_proactive is set; the flag is refreshed every frame so */
    /* the window closing restores the stock completion behavior. */
    if (Config::dtScoutProactive())
    {
        bool dtWindow = false;
        auto &coreTimings = UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
        if (!coreTimings.empty()
            && UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Citadel_of_Adun).empty()
            && UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Templar_Archives).empty()
            && UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Robotics_Facility).empty())
        {
            int coreDone = coreTimings.begin()->first
                           + UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
            dtWindow = frameNow >= coreDone && frameNow < coreDone + 4200;
        }
        auto dtPlay = getPlay<EarlyGameWorkerScout>(playList);
        if (dtWindow)
        {
            if (dtPlay)
            {
                dtPlay->ignoreRangedThreatCompletion = true;
            }
            else
            {
                playList.emplace(playList.begin(), std::make_shared<EarlyGameWorkerScout>());
            }
        }
        else if (dtPlay)
        {
            dtPlay->ignoreRangedThreatCompletion = false;
        }
    }

    /* Set the worker scout mode */
    /* This is just completing the play when we don't expect the scout to be able to gather any additional useful information */
    if (Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::EnemyBaseScouted ||
        Director::getWorkerScoutStatus() == Director::WorkerScoutStatus::MonitoringEnemyChoke)
    {
        auto play = getPlay<EarlyGameWorkerScout>(playList);
        if (play)
        {
            switch (enemyStrategy)
            {
                case ProtossStrategy::Unknown:
                case ProtossStrategy::WorkerRush:
                case ProtossStrategy::ProxyRush:
                case ProtossStrategy::BlockScouting:
                case ProtossStrategy::ZealotRush:
                case ProtossStrategy::TwoGate:
                case ProtossStrategy::ZealotAllIn:
                case ProtossStrategy::EarlyForge:
                case ProtossStrategy::NoZealotCore:
                case ProtossStrategy::OneZealotCore:
                case ProtossStrategy::FastExpansion:
                    break;
                case ProtossStrategy::DragoonAllIn:
                case ProtossStrategy::DarkTemplarRush:
                case ProtossStrategy::EarlyRobo:
                case ProtossStrategy::Turtle:
                case ProtossStrategy::MidGame:
                    play->status.complete = true;
                    break;
            }
        }
    }

    updateDefendBasePlays(playList);
    updateSpecialTeamsPlays(playList);
    defaultExpansions(playList);
    scoutExpos(playList, 12000);
}

void PvP::updateProduction(std::vector<std::shared_ptr<PlayItem>> &playList,
                           std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
    handleNaturalExpansion(playList, prioProdGoals);
    handleDetection(prioProdGoals);
    handleLateTransition(prioProdGoals);

    if (handleIslandExpansionProduction(playList, prioProdGoals)) return;

    auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
    auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
    auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

    int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
    int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];
    int dtCount = UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar);

    int inProgressCount = UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot)
                          + UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Dragoon)
                          + UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Dark_Templar);

    if (!getPlay<ForgeFastExpand>(playList))
    {
        handleGasStealProduction(prioProdGoals, zealotCount);
    }

    /* Main army production */
    switch (ourStrategy)
    {
        case OurStrategy::ForgeExpandDT:
        {
            /* Keep a single zealot once we have a gateway until we are ready for dragoons */
            if (zealotCount == 0
                && UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Gateway) > 0
                && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
            {
                auto incompleteCyberCore = UnitMgr::allMineIncompleteOfType(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
                if (incompleteCyberCore.empty() || (*incompleteCyberCore.begin())->estimatedCompletionFrame > (frameNow + 250))
                {
                    prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                             "SE",
                                                                             BWAPI::UnitTypes::Protoss_Zealot,
                                                                             1,
                                                                             1);
                }
            }

            /* First dragoon is important */
            if (dragoonCount == 0)
            {
                prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE",
                                                                         BWAPI::UnitTypes::Protoss_Dragoon,
                                                                         1,
                                                                         1);
            }

            /* Get a DT at a timing that can be used to discourage a dragoon all-in */
            if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0)
            {
                prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                        "SE",
                        BWAPI::UnitTypes::Protoss_Dark_Templar,
                        1,
                        1,
                        9000 - UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar));
            }

            prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);
            break;
        }
        case OurStrategy::TwoGateDT:
        {
            auto ejectScoutPlay = getPlay<EjectEnemyScout>(playList);
            bool hasEjectedScout = !ejectScoutPlay || ejectScoutPlay->hasEjectedScout();

            /* Build three zealots initially */
            if (zealotCount < 3 && UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Assimilator) == 0
                && UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
            {
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-2gateDT",
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           -1,
                                                                           2);
            }

            /* Build one dragoon to eject the scout */
            if (!hasEjectedScout && dragoonCount < 1)
            {
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-2gateDT",
                                                                           BWAPI::UnitTypes::Protoss_Dragoon,
                                                                           1,
                                                                           2);
            }

            /* Build two DTs once the scout is ejected */
            if (hasEjectedScout && dtCount < 2)
            {
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-2gateDT",
                                                                           BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                           2 - dtCount,
                                                                           -1);
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-2gateDT",
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           -1,
                                                                           -1);
            }
            else
            {
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-2gateDT",
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           -1,
                                                                           2);
            }
            break;
        }
        case OurStrategy::EarlyGameDefense:
        {
            /* Start with one-gate core with two zealots until we have more scouting information */
            oneGateCoreOpening(prioProdGoals, dragoonCount, zealotCount, 2);

            upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);
            break;
        }
        case OurStrategy::AntiZealotRush:
        {
            /* We get at least four zealots, but ensure we match enemy zealot production to avoid getting overrun */
            /* When they are doing a proxy, add in a couple of extra zealots to handle the ones in production */
            int enemyZealots = UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) + 1;
            if (enemyStrategy == ProtossStrategy::ProxyRush) enemyZealots += 2;
            int desiredZealots = std::max(4, enemyZealots);
            int zealotsRequired = desiredZealots - zealotCount;

            handleAntiRushProduction(prioProdGoals, dragoonCount, zealotCount, zealotsRequired);

            break;
        }

        case OurStrategy::AntiDarkTemplarRush:
        {
            /* handleDetection takes care of ordering cannons and obs */

            /* Expand to our hidden base */
            auto hiddenBasePlay = getPlay<HiddenBase>(playList);
            if (hiddenBasePlay && hiddenBasePlay->baseLoc->ownedSince == -1)
            {
                auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(hiddenBasePlay->baseLoc->getTilePosition()),
                                                                      0, 0, 0);
                prioProdGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE-antidt",
                                                                         BWAPI::UnitTypes::Protoss_Nexus,
                                                                         buildLocation);
            }

            prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);

            /* Produce probes at our main so we can quickly ramp up our natural after moving out */
            prioProdGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Probe,
                                                                       -1,
                                                                       1,
                                                                       BuildingPlacement::Neighbourhood::MainBase);

            break;
        }

        case OurStrategy::FastExpansion:
        {
            oneGateCoreOpening(prioProdGoals, dragoonCount, zealotCount, 0);

            /* Default upgrades */
            handleUpgrades(prioProdGoals);

            break;
        }

        case OurStrategy::OneGateCoreNoZealot:
        {
            /* S7: core-first nzcore line - no zealots, dragoons with early range. */
            /* Buildings emerge from Producer's prerequisite closure off the dragoon */
            /* requests (gateway -> assimilator -> core), matching the JSON spec */
            /* (8py/10g/12as/13core/17goon/range). */
            oneGateCoreOpening(prioProdGoals, dragoonCount, zealotCount, 0);

            upgradeAtCount(prioProdGoals,
                           BWAPI::UpgradeTypes::Singularity_Charge,
                           BWAPI::UnitTypes::Protoss_Dragoon,
                           1);

            handleUpgrades(prioProdGoals);

            break;
        }

        case OurStrategy::Defensive:
        case OurStrategy::Normal:
        {
            int desiredZealots = 1;

            /* Modify desired zealots based on detected enemy strategy */
            if (enemyStrategy == ProtossStrategy::NoZealotCore)
            {
                desiredZealots = 0;
            }
            else if (enemyStrategy == ProtossStrategy::BlockScouting)
            {
                desiredZealots = 3;
            }
            
            oneGateCoreOpening(prioProdGoals, dragoonCount, zealotCount, desiredZealots);

            /* Default upgrades */
            handleUpgrades(prioProdGoals);

            break;
        }

        case OurStrategy::ThreeGateRobo:
        {
            if (zealotCount == 0 && dragoonCount == 0)
            {
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-3grobo",
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           1,
                                                                           1);
            }

            if (dragoonCount < 2)
            {
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-3grobo",
                                                                           BWAPI::UnitTypes::Protoss_Dragoon,
                                                                           2 - dragoonCount,
                                                                           1);
            }

            int observerCount = completedUnits[BWAPI::UnitTypes::Protoss_Observer] + incompleteUnits[BWAPI::UnitTypes::Protoss_Observer];
            if (observerCount == 0)
            {
                prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-3grobo",
                                                                           BWAPI::UnitTypes::Protoss_Observer,
                                                                           1,
                                                                           1);
            }

            prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE-3grobo",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);

            upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);
            break;
        }

        case OurStrategy::MidGame:
        {
            /* Have one DT out on the map if we have a templar archives */
            /* Two if the enemy doesn't have mobile detection */
            int desiredDarkTemplar = 0;
            if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0)
            {
                if (!UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer) &&
                    !UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory))
                {
                    desiredDarkTemplar = 2;
                }
                else
                {
                    desiredDarkTemplar = 1;
                }
            }

            /* storm_guard (2026-10-08 funnel plan): once the enemy storm line is */
            /* online we stop trading armies into it - deepen the DT harass line */
            /* instead (DT>0 games win 44.4% vs 9.6% in the official data) */
            if (Config::stormGuardEnabled()
                && (!UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Templar_Archives).empty()
                    || !UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_High_Templar).empty()))
            {
                desiredDarkTemplar += 1;
            }

            if (desiredDarkTemplar > dtCount)
            {
                prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE-nodetect",
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         desiredDarkTemplar - dtCount,
                                                                         -1);
            }

            /* Baseline production is one combat unit for every 6 workers (approximately 3 units per mining base) */
            int higherPriorityCount = (WorkerMgr::mineralWorkers() / 6) - inProgressCount;

            /* Direction "army composition" (O4, 2026-09-30): measured zealot:dragoon is */
            /* ~1:8 because zealots sit at LOWEST priority and only get leftover resources */
            /* while dragoons are unlimited. Mirror the PvZ ratio mix (PvZ.cpp:744-777): */
            /* while the live zealot share is below the configured ratio, zealots ARE the */
            /* unlimited main line; otherwise dragoons. As a side effect the zealot leg */
            /* upgrade stops being short-circuited (Upgrades.cpp bails on a different-type */
            /* unlimited goal, which no longer coexists with the active line). Off = stock. */
            if (Config::armyCompositionEnabled())
            {
                double actualZealotRatio = 0.0;
                if ((zealotCount + dragoonCount) > 0)
                    actualZealotRatio = (double) zealotCount / (double) (zealotCount + dragoonCount);

                if (actualZealotRatio < Config::armyCompositionZealotRatio())
                {
                    mainArmyProduction(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot, -1, higherPriorityCount);

                    /* Build goons at lowest priority */
                    prioProdGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                             "SE",
                                                                             BWAPI::UnitTypes::Protoss_Dragoon,
                                                                             -1,
                                                                             -1);
                }
                else
                {
                    mainArmyProduction(prioProdGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);

                    /* Build zealots at lowest priority */
                    prioProdGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                             "SE",
                                                                             BWAPI::UnitTypes::Protoss_Zealot,
                                                                             -1,
                                                                             -1);
                }
            }
            else
            {
                /* Prefer goons */
                mainArmyProduction(prioProdGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);

                /* Build zealots at lowest priority */
                prioProdGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE",
                                                                         BWAPI::UnitTypes::Protoss_Zealot,
                                                                         -1,
                                                                         -1);
            }

            /* Default upgrades */
            handleUpgrades(prioProdGoals);

            break;
        }
        case OurStrategy::DTExpand:
        {
            auto hiddenBasePlay = getPlay<HiddenBase>(playList);

            if (dtCount < 2)
            {
                prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE",
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         2 - dtCount,
                                                                         2,
                                                                         hiddenBasePlay
                                                                         ? BuildingPlacement::Neighbourhood::HiddenBase
                                                                         : BuildingPlacement::Neighbourhood::AllMyBases);
            }

            prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1,
                                                                       hiddenBasePlay
                                                                       ? BuildingPlacement::Neighbourhood::MainBase
                                                                       : BuildingPlacement::Neighbourhood::AllMyBases);

            /* Make sure we get dragoon range to defend our choke effectively */
            upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            break;
        }
    }
}

void PvP::handleNaturalExpansion(std::vector<std::shared_ptr<PlayItem>> &playList,
                                 std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Hop out if the natural has already been taken and the nexus is completed */
    auto natBase = Map::getMyNatural();
    if (!natBase)
    {
        Telemetry::setBoardValue("natural", "no-natural-base");
        return;
    }

    bool underConstruction = false;
    if (natBase->ownedSince != -1)
    {
        if (natBase->resourceDepot && !natBase->resourceDepot->completed)
        {
            /* Never cancel a backdoor natural */
            if (Map::mapSpecificOverride()->hasBackdoorNatural())
            {
                Telemetry::setBoardValue("natural", "incomplete-backdoor");
                return;
            }
            underConstruction = true;
        }
        else
        {
            Telemetry::setBoardValue("natural", "complete");
            return;
        }
    }
    else
    {
        /* Hop out if we have taken a different base */
        if (Map::getMyBases().size() > 1)
        {
            Telemetry::setBoardValue("natural", "complete-taken-other");
            return;
        }
    }

    /* If we have a backdoor natural, expand if we are gas blocked */
    /* This generally happens when we are teching as a response to something */
    if (Map::mapSpecificOverride()->hasBackdoorNatural() && ourStrategy != OurStrategy::DTExpand)
    {
        if (BWAPI::Broodwar->self()->minerals() > 400 &&
            BWAPI::Broodwar->self()->gas() < 100 &&
            (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed()) >= 6)
        {
            Telemetry::setBoardValue("natural", "take-backdoor");

            takeNaturalExpansion(playList, prioProdGoals);
            return;
        }
    }

    /* P1-B (2026-09-20 report v2): once the one-base exit is due, take the */
    /* natural unconditionally - the mid-game economy track needs it even */
    /* while the army is playing defensive. Expansion-owning lines (FFE, */
    /* 2GateDT, DTExpand) are handled above or by their own plays. */
    if (allinExitDue())
    {
        Telemetry::setBoardValue("natural", "take-allin-exit");

        takeNaturalExpansion(playList, prioProdGoals);
        return;
    }

    auto hiddenBasePlay = getPlay<HiddenBase>(playList);

    bool cancel = true;
    bool buildProbes = false;
    switch (ourStrategy)
    {
        case OurStrategy::ForgeExpandDT:
            /* Handled by the play */
            Telemetry::setBoardValue("natural", "forge-expand");
            return;

        case OurStrategy::TwoGateDT:
        {
            /* Handled by transition to DTExpand after DTs are created */
            return;
        }

        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiZealotRush:
        case OurStrategy::Defensive:
            /* Don't take our natural if the enemy could be rushing or doing an all-in */
            Telemetry::setBoardValue("natural", "wait-defensive");
            break;

        case OurStrategy::AntiDarkTemplarRush:
        {
            if (underConstruction && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0)
            {
                Telemetry::setBoardValue("natural", "take");
                takeNaturalExpansion(playList, prioProdGoals);
                return;
            }

            Telemetry::setBoardValue("natural", "wait-defensive");
            break;
        }

        case OurStrategy::FastExpansion:
            Telemetry::setBoardValue("natural", "take-fast-expo");
            takeNaturalExpansion(playList, prioProdGoals);
            return;

        case OurStrategy::Normal:
        case OurStrategy::ThreeGateRobo:
        case OurStrategy::MidGame:
        {
            /* In this case we want to expand when we consider it safe to do so: we have an attacking or containing army */
            /* that is close to the enemy base */

            /* Check for an attack play first - if we don't have one, we will cancel a constructing natural nexus */
            /* Exception is if our attack play went defensive because an observer isn't close enough to detect a DT */
            auto mainArmyPlay = getPlay<AttackEnemyBase>(playList);
            if (!mainArmyPlay)
            {
                if (UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0 && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0)
                {
                    cancel = false;
                }

                Telemetry::setBoardValue("natural", "no-attack-play");
                break;
            }

            /* From here we never cancel an already-constructing nexus */
            if (underConstruction)
            {
                Telemetry::setBoardValue("natural", "take");
                takeNaturalExpansion(playList, prioProdGoals);
                return;
            }

            /* We never expand before frame 10000 (12000 if the enemy is doing a dragoon all-in) unless the enemy has done so */
            /* or is doing an opening that will not give immediate pressure */
            if (frameNow < (enemyStrategy == ProtossStrategy::DragoonAllIn ? 12000 : 10000) &&
                UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) < 2 &&
                enemyStrategy != ProtossStrategy::EarlyRobo &&
                enemyStrategy != ProtossStrategy::Turtle)
            {
                Telemetry::setBoardValue("natural", "too-early");
                break;
            }

            /* We expect to want to take our natural soon, so build probes */
            /* Exception if we might want to take our hidden base */
            buildProbes = (hiddenBasePlay == nullptr);

            auto squad = mainArmyPlay->getSquad();
            if (!squad || squad->getUnits().size() < 5)
            {
                Telemetry::setBoardValue("natural", "attack-play-too-small");
                break;
            }

            int span;
            auto vanguardCluster = squad->vanguardCluster(&span);
            if (!vanguardCluster)
            {
                Telemetry::setBoardValue("natural", "no-vanguard-cluster");
                break;
            }

            /* Cluster should be past our own natural */
            int naturalDist = Pathfinder::GetGroundDistance(natBase->getPosition(), mainArmyPlay->baseLoc->getPosition());
            if (naturalDist != -1 && span > (naturalDist - 320))
            {
                Telemetry::setBoardValue("natural", "vanguard-cluster-not-beyond-natural");
                break;
            }

            /* Get an observatory before expanding */
/* if (Units::countAll(BWAPI::UnitTypes::Protoss_Observatory) < 1) */
/* { */
/* // TODO: When producer can handle requesting a building, request the observatory instead */
/* prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>, */
/* "SE-safeexpand", */
/* BWAPI::UnitTypes::Protoss_Observer, */
/* 1, */
/* 1); */
/* CherryVis::setBoardValue("natural", "wait-for-obs"); */
/* break; */
/* } */

            /* Always expand in this situation if we are gas blocked */
            if (BWAPI::Broodwar->self()->minerals() > 500 && BWAPI::Broodwar->self()->gas() < 100)
            {
                Telemetry::setBoardValue("natural", "take-gas-blocked");
                takeNaturalExpansion(playList, prioProdGoals);
                return;
            }

            /* Cluster should not be moving or fleeing */
            /* In other words, we want the cluster to be in some kind of stable attack or contain state */
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
                || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                    && (vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee
                        || vanguardCluster->currentSubActivity == UnitCluster::SubActivity::StandGround)))
            {
                /* We don't cancel a queued expansion in this case */
                cancel = false;
                Telemetry::setBoardValue("natural", "vanguard-cluster-not-attacking");
                break;
            }

            /* If the cluster is attacking, it should be significantly closer to the enemy main, unless we are past frame 12500 */
            if (frameNow < 12500 &&
                vanguardCluster->currentActivity == UnitCluster::Activity::Attacking &&
                vanguardCluster->percentageToEnemyMain < 0.7)
            {
                Telemetry::setBoardValue("natural", "vanguard-cluster-too-close");
                break;
            }

            Telemetry::setBoardValue("natural", "take");
            takeNaturalExpansion(playList, prioProdGoals);
            return;
        }
        case OurStrategy::DTExpand:
        {
            /* We never cancel an already-constructing nexus */
            if (underConstruction)
            {
                Telemetry::setBoardValue("natural", "take");
                takeNaturalExpansion(playList, prioProdGoals);
                return;
            }

            buildProbes = true;

            /* Take our natural as soon as the army has moved beyond it */
            auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyBase))
            {
                Telemetry::setBoardValue("natural", "no-attack-play");
                break;
            }

            auto squad = mainArmyPlay->getSquad();
            if (!squad)
            {
                Telemetry::setBoardValue("natural", "no-attack-play-squad");
                break;
            }

            auto vanguardCluster = squad->vanguardCluster();
            if (!vanguardCluster)
            {
                Telemetry::setBoardValue("natural", "no-vanguard-cluster");
                break;
            }

            /* Ensure the cluster is at least 10 tiles further from the natural than it is from the main */
            int distToMain = Pathfinder::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->midpoint);
            int distToNatural = Pathfinder::GetGroundDistance(natBase->getPosition(), vanguardCluster->midpoint);
            if (distToNatural < 500 || distToMain < (distToNatural + 500))
            {
                Telemetry::setBoardValue("natural", "vanguard-too-close");
                break;
            }

            /* Cluster should not be fleeing */
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee)
            {
                Telemetry::setBoardValue("natural", "vanguard-fleeing");
                break;
            }

            Telemetry::setBoardValue("natural", "take");
            takeNaturalExpansion(playList, prioProdGoals);
            return;
        }
    }

    if (cancel) cancelNaturalExpansion(playList, prioProdGoals);

    /* Take our hidden base expansion in cases where we have been prevented from taking our natural */
    /* It is queued at lowest priority so it will only be taken if we have an excess of minerals */
    if (frameNow > 12000)
    {
        if (hiddenBasePlay && hiddenBasePlay->baseLoc->ownedSince == -1)
        {
            auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(hiddenBasePlay->baseLoc->getTilePosition()),
                                                                  0, 0, 0);
            prioProdGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     "SE-hiddenbase",
                                                                     BWAPI::UnitTypes::Protoss_Nexus,
                                                                     buildLocation);

        }
    }

    /* If requested, build up to 10 probes for our natural */
    /* They are queued at lowest priority, so will not interfere with army production */
    if (buildProbes)
    {
        auto idleWorkers = WorkerMgr::idleWorkerCount();
        if (idleWorkers < 10)
        {
            prioProdGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     "SE",
                                                                     BWAPI::UnitTypes::Protoss_Probe,
                                                                     10 - idleWorkers,
                                                                     1);
        }
    }
}

void PvP::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Basic infantry skill upgrades are queued when we have enough of them and are still building them */
    upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);

    /* Cases where we want the upgrade as soon as we start building one of the units */
    upgradeWhenUnitCreated(prioProdGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, false, true);
    upgradeWhenUnitCreated(prioProdGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, true);

    /* Upgrade observer speed on three gas */
    if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 3)
    {
        upgradeWhenUnitCreated(prioProdGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    }

    defaultGroundUpgrades(prioProdGoals);

    /* TODO: Air upgrades */
}

/* P1-A/P1-B (2026-09-20 report v2): rough army value (mineral+gas price of */
/* completed combat units) for the late-transition and one-base exit gates. */
namespace
{
    int armyValue(bool mine)
    {
        int value = 0;
        if (mine)
        {
            for (auto &unit : UnitMgr::allMine())
            {
                if (!unit || !unit->completed || !UnitHelper::IsCombatUnit(unit->type)) continue;
                value += unit->type.mineralPrice() + unit->type.gasPrice();
            }
        }
        else
        {
            for (auto &unit : UnitMgr::allEnemy())
            {
                if (!UnitHelper::IsCombatUnit(unit->type)) continue;
                value += unit->type.mineralPrice() + unit->type.gasPrice();
            }
        }
        return value;
    }
}

/* P1-B (2026-09-20 report v2): one-base all-in lines have a ~14 minute */
/* decision window (50-game evidence: such wins land <=14.1 minutes while */
/* un-won ones die starved past 15.5 minutes on 20-34 workers). Due = past */
/* the configured frame, still on one base, and not holding an army-value */
/* lead. Cached for 240 frames - the selector runs every frame. */
bool PvP::allinExitDue()
{
    if (!Config::pvpAllinExitEnabled()) return false;
    if (frameNow < Config::pvpAllinExitFrame()) return false;
    if (allinExitCheckedAt >= 0 && frameNow - allinExitCheckedAt < 240)
    {
        return allinExitDueCache;
    }
    allinExitCheckedAt = frameNow;
    allinExitDueCache =
            UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) <= 1 &&
            armyValue(true) < armyValue(false);
    return allinExitDueCache;
}

/* P1-A (2026-09-20 report v2): late-game transition to High Templar with */
/* Psionic Storm plus a reaver branch once a robotics bay exists. 50-game */
/* evidence: zero HT/reavers trained while every >=16min loss faced an */
/* HT/reaver mix (4-13 in that bucket). Fires from the configured frame in */
/* every state - a one-base line stuck in a defensive state still gets the */
/* transition (P1-B handles its economy). Archives and the support bay */
/* emerge from the producer's prerequisite closure. */
void PvP::handleLateTransition(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    if (!Config::pvpLateTransitionEnabled()) return;
    if (frameNow < Config::pvpLateTransitionFrame()) return;

    int htIncomplete = UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_High_Templar);
    int htCount = UnitMgr::countAll(BWAPI::UnitTypes::Protoss_High_Templar)
                + htIncomplete;
    int htTarget = Config::pvpLateTransitionHTTarget();
    /* HTs only pay for themselves when they cast. storm_cast off means no
     * casting exists, so every HT trained here is dead gas weight - batch
     * forensics (2026-09-30) measured the all-off baseline feeding 5-15 HTs
     * plus storm research per game with zero casts. Gate production on the
     * executor switch; with storm_cast on nothing changes. */
    if (Config::stormCastEnabled() && htCount < htTarget)
    {
        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                    "SE-late",
                                                    BWAPI::UnitTypes::Protoss_High_Templar,
                                                    htTarget - htCount,
                                                    1);
    }

    /* HT-pipe diagnostics (2026-09-28), measure-only: batches 14-16 trained zero */
    /* HTs vs 9.3/game in the 60-game baseline. The completed/incomplete split keeps */
    /* the double-counted incomplete HTs in htCount above visible in the log without */
    /* fixing them here - a behaviour change needs its own justification round. */
    {
        static int armedFrame = -1;
        static int lastTickFrame = -10000;
        static int lastGoalCount = -1;
        int htCompleted = UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_High_Templar);
        int goalCount = htCount < htTarget ? htTarget - htCount : 0;
        if (armedFrame < 0)
        {
            armedFrame = frameNow;
            FileTools::appendLine("shushanking_htpipe",
                                  (std::ostringstream() << "HTPIPE ARMED frame=" << frameNow
                                   << " htCompleted=" << htCompleted
                                   << " htIncomplete=" << htIncomplete
                                   << " htTarget=" << htTarget).str());
        }
        if (goalCount != lastGoalCount || frameNow - lastTickFrame >= 1000)
        {
            lastGoalCount = goalCount;
            lastTickFrame = frameNow;
             FileTools::appendLine("shushanking_htpipe",
                                  (std::ostringstream() << "HTPIPE TICK frame=" << frameNow
                                   << " htCompleted=" << htCompleted
                                   << " htIncomplete=" << htIncomplete
                                   << " goalCount=" << goalCount
                                   << " zealot=" << UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Zealot)
                                   << " dragoon=" << UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Dragoon)
                                   << " archives=" << UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Templar_Archives)
                                   << " citadel=" << UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Citadel_of_Adun)
                                   << " gateways=" << UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Gateway)
                                   << " gas=" << BWAPI::Broodwar->self()->gas()
                                   << " mineral=" << BWAPI::Broodwar->self()->minerals()).str());
        }
    }

    auto self = BWAPI::Broodwar->self();
    if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0 &&
        htCount > 0 &&
        !self->hasResearched(BWAPI::TechTypes::Psionic_Storm) &&
        !self->isResearching(BWAPI::TechTypes::Psionic_Storm))
    {
        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                    "SE-late",
                                                    BWAPI::TechTypes::Psionic_Storm,
                                                    1,
                                                    1);
    }

    if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
    {
        int reaverCount = UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Reaver)
                        + UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Reaver);
        int reaverTarget = Config::pvpLateTransitionReaverTarget();
        if (reaverCount < reaverTarget)
        {
            prioProdGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                        "SE-late",
                                                        BWAPI::UnitTypes::Protoss_Reaver,
                                                        reaverTarget - reaverCount,
                                                        1);
        }
    }

    /* Direction "storm timing" (2026-09-28 forensics): our Archives lands ~5.3m vs */
    /* PW's ~9.8m but the 4.5-minute tech lead was never cashed in - 6-12m produced */
    /* only ~2 HTs while PW wins bought Khaydarin Amulet in 88% of their wins and */
    /* used storms in 95%. Amulet gives newly-trained HTs instant storm energy: the */
    /* difference between a storm that can fire the moment the wave hits and one */
    /* that arrives empty. Off by default. */
    if (Config::stormAmuletEnabled()
        && UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0
        && !self->getUpgradeLevel(BWAPI::UpgradeTypes::Khaydarin_Amulet)
        && !self->isUpgrading(BWAPI::UpgradeTypes::Khaydarin_Amulet))
    {
        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                    "SE-storm",
                                                    BWAPI::UpgradeTypes::Khaydarin_Amulet,
                                                    1,
                                                    1);
    }

    /* Direction "storm timing" (2026-09-28): spent HTs (energy < 25) have no threat */
    /* value and die for free in fights. Merging two of them into an Archon gives a */
    /* durable ground unit (the merge is instant, no production slot). Off by default. */
    if (Config::archonMergeEnabled()
        && self->hasResearched(BWAPI::TechTypes::Psionic_Storm))
    {
        std::vector<OwnedUnit> spentHTs;
        for (auto &unit : UnitMgr::allMine())
        {
            if (!unit || !unit->completed || unit->type != BWAPI::UnitTypes::Protoss_High_Templar) continue;
            if (unit->rawUnit->getEnergy() >= 25) continue;
            spentHTs.push_back(unit);
        }
        if (spentHTs.size() >= 2)
        {
            auto &a = spentHTs[spentHTs.size() - 1];
            auto &b = spentHTs[spentHTs.size() - 2];
            a->rawUnit->useTech(BWAPI::TechTypes::Archon_Warp, b->rawUnit);
        }
    }
}

void PvP::handleDetection(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* The main army play will reactively request mobile detection when it sees a cloaked enemy unit */
    /* The logic here is to look ahead to make sure we already have detection available when we need it */

    /* Break out if we already have an observer */
    if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Observer) > 0)
    {
        Telemetry::setBoardValue("detection", "have-observer");
        return;
    }

    /* Break out if we are going 3 gate robo */
    if (ourStrategy == OurStrategy::ThreeGateRobo)
    {
        Telemetry::setBoardValue("detection", "3-gate-robo");
        return;
    }

    auto buildObserver = [&](int frameNeeded = 0)
    {
        auto priority = UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0
                        ? PRIORITY_EMERGENCY
                        : PRIORITY_NORMAL;
        prioProdGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                          "SE-detection",
                                                          BWAPI::UnitTypes::Protoss_Observer,
                                                          1,
                                                          1,
                                                          frameNeeded);
    };

    /* If the enemy is known to have produced a DT, get a cannon and observer */
    if (enemyStrategy == ProtossStrategy::DarkTemplarRush ||
        UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Dark_Templar))
    {
        buildDefensiveCannons(prioProdGoals, !hasCannonAtWall(), 0, 1);
        buildObserver();
        Telemetry::setBoardValue("detection", "emergency-build-cannon-and-observer");
        return;
    }

    /* Get an observer when we have a second gas */
    if ((UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1 ||
        (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1 && frameNow > 10000))
        && Director::pressure() < 0.4)
    {
        Telemetry::setBoardValue("detection", "macro-build-observer");
        buildObserver();
        return;
    }

    /* If our strategy is anti-zealot-rush and there is an enemy combat unit in our base, we have worse things to worry about */
    if (ourStrategy == OurStrategy::AntiZealotRush)
    {
        for (const auto &gameUnit : UnitMgr::allEnemy())
        {
            if (!gameUnit->lastPositionValid) continue;
            if (!UnitHelper::IsCombatUnit(gameUnit->type)) continue;
            if (!gameUnit->type.canAttack()) continue;

            for (const auto &area : Map::getMyMainAreas())
            {
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos)) == area)
                {
                    Telemetry::setBoardValue("detection", "enemy-in-base");
                    return;
                }
            }
        }

        /* Add some frame stops to ensure we don't build a cannon while the enemy is still producing zealots */
        if ((frameNow < 7000 && UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Zealot).size() >= 8) ||
            (frameNow < 8000 && UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Zealot).size() >= 10) ||
            (frameNow < 9000 && UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Zealot).size() >= 12))
        {
            Telemetry::setBoardValue("detection", "anti-zealot-rush");
            return;
        }
    }

    /* Break out if we have detected a strategy that precludes a dark templar rush now */
    bool zealotStrategy = enemyStrategy == ProtossStrategy::ProxyRush ||
                          enemyStrategy == ProtossStrategy::ZealotRush ||
                          enemyStrategy == ProtossStrategy::ZealotAllIn
                          || (enemyStrategy == ProtossStrategy::TwoGate && UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) > 5);
    if ((enemyStrategy == ProtossStrategy::EarlyForge && frameNow < 6000)
        || (zealotStrategy && frameNow < 6500)
        || (zealotStrategy && UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) > 1 && frameNow < 9000)
        || (enemyStrategy == ProtossStrategy::DragoonAllIn && frameNow < 8000)
        || (enemyStrategy == ProtossStrategy::EarlyRobo && frameNow < 8000)
        || (enemyStrategy == ProtossStrategy::FastExpansion && frameNow < 7000)
        || (enemyStrategy == ProtossStrategy::Turtle && frameNow < 8000))
    {
        Telemetry::setBoardValue("detection", "strategy-exception");
        return;
    }

    /* Initialize the expected DT completion frame based on previous game observations */
    /* We assume worst case until we have at least 10 games played against the opponent */
    /* If we have never lost to the opponent, we continue playing conservatively even if we have never seen a DT */
    int expectedCompletionFrame = 7300;
    if (OpponentEconomicModel::enabled())
    {
        expectedCompletionFrame = OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Dark_Templar)
                + UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar);
    }
    if (Opponent::winLossRatio(0.0, 200) < 0.99)
    {
        expectedCompletionFrame = std::min(Opponent::minValueInPreviousGames("firstDarkTemplarCompleted", expectedCompletionFrame, 15, 10), 20000);
    }

    /* If we haven't found the enemy main, be conservative and assume we might see DTs 500 frames after earliest completion */
    auto enemyMain = Map::getEnemyMain();
    if (!enemyMain)
    {
        buildDefensiveCannons(prioProdGoals, !hasCannonAtWall(), expectedCompletionFrame + 500);
        Telemetry::setBoardValue("detection", "cannon-at-worst-case");
        return;
    }

    /* Otherwise compute when the enemy could get DTs based on our scouting information */

    /* First estimate when the enemy's templar archives will / might finish */
    auto &templarArchiveTimings = UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Templar_Archives);
    if (!templarArchiveTimings.empty())
    {
        /* We've scouted a templar archives directly, so use its completion frame */
        /* It's unlikely the enemy is building a templar archives as a fake-out */
        expectedCompletionFrame =
                templarArchiveTimings.begin()->first
                + UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives)
                + UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar);
    }
    else
    {
        /* If we've scouted a citadel, so assume the templar archives is started as soon as the citadel finishes, unless */
        /* we've scouted the base in the meantime or our previous game data indicates it is likely to be a fake-out */
        auto &citadelTimings = UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
        if (!citadelTimings.empty() && expectedCompletionFrame < 10000)
        {
            expectedCompletionFrame =
                    UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives)
                    + std::max(enemyMain->lastScouted,
                               citadelTimings.begin()->first + UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Citadel_of_Adun))
                               + UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar);
        }
    }

    /* Compute the transit time from the enemy's closest gateway */
    auto myMainChoke = Map::getMyMainChoke();
    auto myPosition = myMainChoke ? myMainChoke->midpoint : Map::getMyMain()->getPosition();
    int closestGatewayFrames = Pathfinder::ExpectedTravelTime(enemyMain->getPosition(),
                                                               myPosition,
                                                               BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                               Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                               1.1);
    for (const auto &gameUnit : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Protoss_Gateway))
    {
        if (!gameUnit->completed) continue;

        int frames = Pathfinder::ExpectedTravelTime(gameUnit->prevPos,
                                                     myPosition,
                                                     BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                     Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                     1.1,
                                                     -1);
        if (frames != -1 && frames < closestGatewayFrames)
        {
            closestGatewayFrames = frames;
        }
    }

#if OUTPUT_DETECTION_DEBUG
    Telemetry::log() << "detection: expected DT completion @ " << expectedCompletionFrame
                     << "; DT at our choke @ "
                     << (closestGatewayFrames + expectedCompletionFrame);
#endif

    /* Now sum everything up to get the frame where we need detection */
    int tick = expectedCompletionFrame + closestGatewayFrames;
    if (tick < 0) return;
    if (templarArchiveTimings.empty())
    {
        buildDefensiveCannons(prioProdGoals, !hasCannonAtWall(), tick);
        Telemetry::setBoardValue("detection", (std::ostringstream() << "cannon-at-" << tick).str());
    }
    else
    {
        buildObserver(tick);
        buildDefensiveCannons(prioProdGoals, !hasCannonAtWall(), tick, 1);
        Telemetry::setBoardValue("detection", (std::ostringstream() << "cannon-and-observer-at-" << tick).str());
    }
}
