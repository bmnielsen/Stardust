#include "StrategyEngines/PvP.h"

#include "Units.h"
#include "Map.h"
#include "Builder.h"
#include "UnitUtil.h"
#include "Strategist.h"
#include "Workers.h"
#include "Opponent.h"
#include "OpponentEconomicModel.h"

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
}

void PvP::initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom)
{
    if (transitioningFromRandom)
    {
        plays.emplace(beforePlayIt<MainArmyPlay>(plays), std::make_shared<EjectEnemyScout>());

        if (getPlay<ForgeFastExpand>(plays))
        {
            ourStrategy = OurStrategy::ForgeExpandDT;
        }
    }
    else
    {
        plays.emplace_back(std::make_shared<SaturateBases>());
        plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
        plays.emplace_back(std::make_shared<EjectEnemyScout>());

        auto opening = Opponent::selectOpeningUCB1(
                {
                        OurStrategyNames[OurStrategy::EarlyGameDefense],
                        OurStrategyNames[OurStrategy::ForgeExpandDT]
                });
        if (opening == OurStrategyNames[OurStrategy::ForgeExpandDT] && BuildingPlacement::hasForgeGatewayWall())
        {
            plays.emplace_back(std::make_shared<ForgeFastExpand>());
            ourStrategy = OurStrategy::ForgeExpandDT;
        }
        else
        {
            plays.emplace_back(std::make_shared<DefendMyMain>());
        }
        Log::Get() << "Selected opening " << OurStrategyNames[ourStrategy];

//      plays.emplace_back(std::make_shared<HiddenBase>());
    }

    Opponent::addMyStrategyChange(OurStrategyNames[ourStrategy]);
    Opponent::addEnemyStrategyChange(ProtossStrategyNames[enemyStrategy]);
}

void PvP::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    if (OpponentEconomicModel::enabled())
    {
        CherryVis::setBoardValue(
                "modelled-earliest-dt",
                (std::ostringstream() << OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Dark_Templar)).str());
        CherryVis::setBoardValue(
                "modelled-earliest-nexus",
                (std::ostringstream() << OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Nexus)).str());
        auto zealots = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
        auto dragoons = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);

        CherryVis::setBoardValue(
                "modelled-zealots-now",
                (std::ostringstream() << "[" << zealots.first << "," << zealots.second << "]").str());
        CherryVis::setBoardValue(
                "modelled-dragoons-now",
                (std::ostringstream() << "[" << dragoons.first << "," << dragoons.second << "]").str());
    }

    auto newEnemyStrategy = recognizeEnemyStrategy();
    auto newStrategy = chooseOurStrategy(newEnemyStrategy, plays);

    if (enemyStrategy != newEnemyStrategy)
    {
        Log::Get() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#endif

        enemyStrategy = newEnemyStrategy;
        enemyStrategyChanged = currentFrame;
        Opponent::addEnemyStrategyChange(ProtossStrategyNames[enemyStrategy]);
    }

    if (ourStrategy != newStrategy)
    {
        Log::Get() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
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
                // Attack when we have goon range
                defendOurMain = (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0);
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
                // Wait until we have at least completed two dark templar that aren't in our main
                auto &mainAreas = Map::getMyMainAreas();
                int dtCount = 0;
                for (const auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Dark_Templar))
                {
                    auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition));
                    if (mainAreas.find(area) == mainAreas.end()) dtCount++;
                }

                defendOurMain = dtCount < 2;
                break;
            }
            case OurStrategy::FastExpansion:
            case OurStrategy::Normal:
            case OurStrategy::ThreeGateRobo:
            case OurStrategy::MidGame:
            {
                auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
                if (!mainArmyPlay)
                {
                    defendOurMain = true;
                    break;
                }

                // Always use a defend play if the squad has no units
                auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                if (!vanguard)
                {
                    defendOurMain = true;
                    break;
                }

                // Require range off of an FFE
                if (getPlay<ForgeFastExpand>(plays) && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0)
                {
                    defendOurMain = true;
                    break;
                }

                // Use a defend play if we are doing a 3 gate robo or the enemy has dark templar and our army has no observers
                if ((ourStrategy == OurStrategy::ThreeGateRobo && Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) == 0)
                    || (Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0 && mainArmyPlay->getSquad()->getDetectors().empty()))
                {
                    defendOurMain = true;
                    break;
                }

                defendOurMain = false;

                // Transition from a defend squad when the vanguard cluster has 3 units and can do so
                // Exception: always attack if the enemy strategy is fast expansion
                if (typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    defendOurMain = enemyStrategy != ProtossStrategy::FastExpansion &&
                                    (vanguard->units.size() < 3 ||
                                     !((DefendMyMain *) mainArmyPlay)->canTransitionToAttack());
                }

                // Transition to a defend squad if our attack squad has been pushed back into our main and we haven't yet taken our natural
                auto natural = Map::getMyNatural();
                if (typeid(*mainArmyPlay) == typeid(AttackEnemyBase)
                    && (!natural || natural->owner != BWAPI::Broodwar->self() || !natural->resourceDepot || !natural->resourceDepot->completed))
                {
                    if (vanguard->currentActivity == UnitCluster::Activity::Regrouping &&
                        vanguard->currentSubActivity == UnitCluster::SubActivity::Flee)
                    {
                        auto inMain = [](BWAPI::Position pos)
                        {
                            auto choke = Map::getMyMainChoke();
                            if (choke && choke->center.getApproxDistance(pos) < 320) return true;

                            auto mainAreas = Map::getMyMainAreas();
                            return mainAreas.find(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(pos))) != mainAreas.end();
                        };

                        if (inMain(vanguard->center) || (vanguard->vanguard && inMain(vanguard->vanguard->lastPosition)))
                        {
                            defendOurMain = true;
                        }
                    }
                }

                break;
            }
        }
    }

    updateAttackPlays(plays, defendOurMain);

    // Ensure we have an anti cannon rush play if the enemy strategy warrants it
    if (currentFrame < 4000)
    {
        // If we've detected a proxy, keep track of whether we have identified it as a zealot rush
        // Once we've either seen the proxied gateway or a zealot, we assume the enemy isn't doing a "proxy" cannon rush
        bool zealotProxy = enemyStrategy == ProtossStrategy::ProxyRush &&
                           (Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Gateway) || Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Zealot));

        auto antiCannonRushPlay = getPlay<AntiCannonRush>(plays);
        if (enemyStrategy == ProtossStrategy::EarlyForge ||
            (enemyStrategy == ProtossStrategy::ProxyRush && !zealotProxy) ||
            enemyStrategy == ProtossStrategy::BlockScouting ||
            (enemyStrategy == ProtossStrategy::Unknown && currentFrame > 2000))
        {
            if (!antiCannonRushPlay)
            {
                plays.emplace(plays.begin(), std::make_shared<AntiCannonRush>());
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

    // Set the worker scout mode
    // This is just completing the play when we don't expect the scout to be able to gather any additional useful information
    if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted ||
        Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::MonitoringEnemyChoke)
    {
        auto play = getPlay<EarlyGameWorkerScout>(plays);
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
                    break;
                case ProtossStrategy::FastExpansion:
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

    updateDefendBasePlays(plays);
    updateSpecialTeamsPlays(plays);
    defaultExpansions(plays);
    scoutExpos(plays, 15000);
}

void PvP::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(prioritizedProductionGoals);

    if (handleIslandExpansionProduction(plays, prioritizedProductionGoals)) return;

    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
    auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
    auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

    int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
    int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

    int inProgressCount = Units::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dragoon)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dark_Templar);

    if (!getPlay<ForgeFastExpand>(plays))
    {
        handleGasStealProduction(prioritizedProductionGoals, zealotCount);
    }

    // Main army production
    switch (ourStrategy)
    {
        case OurStrategy::ForgeExpandDT:
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);

            // Keep a single zealot once we have a gateway until we are ready for dragoons
            if (zealotCount == 0
                && Units::countAll(BWAPI::UnitTypes::Protoss_Gateway) > 0
                && Units::countCompleted(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
            {
                auto incompleteCyberCore = Units::allMineIncompleteOfType(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
                if (incompleteCyberCore.empty() || (*incompleteCyberCore.begin())->estimatedCompletionFrame > (currentFrame + 250))
                {
                    prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                             "SE",
                                                                             BWAPI::UnitTypes::Protoss_Zealot,
                                                                             1,
                                                                             1);
                }
            }

            // Get a DT at a timing that can be used to discourage a dragoon all-in
            // We push back the timing if the enemy has already taken their natural
            auto dtTiming = 9000;
            if (Map::getEnemyBases().size() > 1) dtTiming = 10000;
            if (currentFrame < dtTiming && Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                        "SE",
                        BWAPI::UnitTypes::Protoss_Dark_Templar,
                        1,
                        1,
                        dtTiming - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar));
            }

            break;
        }
        case OurStrategy::EarlyGameDefense:
        {
            // Start with one-gate core with two zealots until we have more scouting information
            oneGateCoreOpening(prioritizedProductionGoals, dragoonCount, zealotCount, 2);

            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);
            break;
        }
        case OurStrategy::AntiZealotRush:
        {
            // We get at least four zealots, but ensure we match enemy zealot production to avoid getting overrun
            // When they are doing a proxy, add in a couple of extra zealots to handle the ones in production
            int enemyZealots = Units::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) + 1;
            if (enemyStrategy == ProtossStrategy::ProxyRush) enemyZealots += 2;
            int desiredZealots = std::max(4, enemyZealots);
            int zealotsRequired = desiredZealots - zealotCount;

            handleAntiRushProduction(prioritizedProductionGoals, dragoonCount, zealotCount, zealotsRequired);

            break;
        }

        case OurStrategy::AntiDarkTemplarRush:
        {
            // handleDetection takes care of ordering cannons and obs

            // Expand to our hidden base
            auto hiddenBasePlay = getPlay<HiddenBase>(plays);
            if (hiddenBasePlay && hiddenBasePlay->base->ownedSince == -1)
            {
                auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(hiddenBasePlay->base->getTilePosition()),
                                                                      0, 0, 0);
                prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE-antidt",
                                                                         BWAPI::UnitTypes::Protoss_Nexus,
                                                                         buildLocation);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);

            // Produce probes at our main so we can quickly ramp up our natural after moving out
            prioritizedProductionGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Probe,
                                                                       -1,
                                                                       1,
                                                                       BuildingPlacement::Neighbourhood::MainBase);

            break;
        }

        case OurStrategy::FastExpansion:
        {
            oneGateCoreOpening(prioritizedProductionGoals, dragoonCount, zealotCount, 0);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }

        case OurStrategy::Defensive:
        case OurStrategy::Normal:
        {
            int desiredZealots = 1;

            // Modify desired zealots based on detected enemy strategy
            if (enemyStrategy == ProtossStrategy::NoZealotCore)
            {
                desiredZealots = 0;
            }
            else if (enemyStrategy == ProtossStrategy::BlockScouting)
            {
                desiredZealots = 3;
            }
            
            oneGateCoreOpening(prioritizedProductionGoals, dragoonCount, zealotCount, desiredZealots);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }

        case OurStrategy::ThreeGateRobo:
        {
            if (zealotCount == 0 && dragoonCount == 0)
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-3grobo",
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           1,
                                                                           1);
            }

            if (dragoonCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-3grobo",
                                                                           BWAPI::UnitTypes::Protoss_Dragoon,
                                                                           2 - dragoonCount,
                                                                           1);
            }

            int observerCount = completedUnits[BWAPI::UnitTypes::Protoss_Observer] + incompleteUnits[BWAPI::UnitTypes::Protoss_Observer];
            if (observerCount == 0)
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE-3grobo",
                                                                           BWAPI::UnitTypes::Protoss_Observer,
                                                                           1,
                                                                           1);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE-3grobo",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);

            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);
            break;
        }

        case OurStrategy::MidGame:
        {
            // Have one DT out on the map if we have a templar archives
            // Two if the enemy doesn't have mobile detection
            int desiredDarkTemplar = 0;
            if (Units::countAll(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0)
            {
                if (!Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer) &&
                    !Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory))
                {
                    desiredDarkTemplar = 2;
                }
                else
                {
                    desiredDarkTemplar = 1;
                }
            }
            int dtCount = Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar);
            if (desiredDarkTemplar > dtCount)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE-nodetect",
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         desiredDarkTemplar - dtCount,
                                                                         -1);
            }

            // Baseline production is one combat unit for every 6 workers (approximately 3 units per mining base)
            int higherPriorityCount = (Workers::mineralWorkers() / 6) - inProgressCount;

            // Prefer goons
            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);

            // Build zealots at lowest priority
            prioritizedProductionGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     "SE",
                                                                     BWAPI::UnitTypes::Protoss_Zealot,
                                                                     -1,
                                                                     -1);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }
        case OurStrategy::DTExpand:
        {
            auto hiddenBasePlay = getPlay<HiddenBase>(plays);

            int dtCount = Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar);
            if (dtCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE",
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         2 - dtCount,
                                                                         2,
                                                                         hiddenBasePlay
                                                                         ? BuildingPlacement::Neighbourhood::HiddenBase
                                                                         : BuildingPlacement::Neighbourhood::AllMyBases);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1,
                                                                       hiddenBasePlay
                                                                       ? BuildingPlacement::Neighbourhood::MainBase
                                                                       : BuildingPlacement::Neighbourhood::AllMyBases);

            // Make sure we get dragoon range to defend our choke effectively
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            break;
        }
    }
}

void PvP::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been taken and the nexus is completed
    auto natural = Map::getMyNatural();
    if (!natural)
    {
        CherryVis::setBoardValue("natural", "no-natural-base");
        return;
    }

    bool underConstruction = false;
    if (natural->ownedSince != -1)
    {
        if (natural->resourceDepot && !natural->resourceDepot->completed)
        {
            underConstruction = true;
        }
        else
        {
            CherryVis::setBoardValue("natural", "complete");
            return;
        }
    }
    else
    {
        // Hop out if we have taken a different base
        if (Map::getMyBases().size() > 1)
        {
            CherryVis::setBoardValue("natural", "complete-taken-other");
            return;
        }
    }

    // If we have a backdoor natural, expand if we are gas blocked
    // This generally happens when we are teching as a response to something
    if (Map::mapSpecificOverride()->hasBackdoorNatural() && ourStrategy != OurStrategy::DTExpand)
    {
        if (BWAPI::Broodwar->self()->minerals() > 400 &&
            BWAPI::Broodwar->self()->gas() < 100 &&
            (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed()) >= 6)
        {
            CherryVis::setBoardValue("natural", "take-backdoor");

            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
    }

    auto hiddenBasePlay = getPlay<HiddenBase>(plays);

    bool cancel = true;
    bool buildProbes = false;
    switch (ourStrategy)
    {
        case OurStrategy::ForgeExpandDT:
            // Handled by the play
            CherryVis::setBoardValue("natural", "forge-expand");
            return;

        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiZealotRush:
        case OurStrategy::Defensive:
            // Don't take our natural if the enemy could be rushing or doing an all-in
            CherryVis::setBoardValue("natural", "wait-defensive");
            break;

        case OurStrategy::AntiDarkTemplarRush:
        {
            if (underConstruction && Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0)
            {
                CherryVis::setBoardValue("natural", "take");
                takeNaturalExpansion(plays, prioritizedProductionGoals);
                return;
            }

            CherryVis::setBoardValue("natural", "wait-defensive");
            break;
        }

        case OurStrategy::FastExpansion:
            CherryVis::setBoardValue("natural", "take-fast-expo");
            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;

        case OurStrategy::Normal:
        case OurStrategy::ThreeGateRobo:
        case OurStrategy::MidGame:
        {
            // In this case we want to expand when we consider it safe to do so: we have an attacking or containing army
            // that is close to the enemy base

            // Check for an attack play first - if we don't have one, we will cancel a constructing natural nexus
            // Exception is if our attack play went defensive because an observer isn't close enough to detect a DT
            auto mainArmyPlay = getPlay<AttackEnemyBase>(plays);
            if (!mainArmyPlay)
            {
                if (Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0 && Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0)
                {
                    cancel = false;
                }

                CherryVis::setBoardValue("natural", "no-attack-play");
                break;
            }

            // From here we never cancel an already-constructing nexus
            if (underConstruction)
            {
                CherryVis::setBoardValue("natural", "take");
                takeNaturalExpansion(plays, prioritizedProductionGoals);
                return;
            }

            // We never expand before frame 10000 (12000 if the enemy is doing a dragoon all-in) unless the enemy has done so
            // or is doing an opening that will not give immediate pressure
            if (currentFrame < (enemyStrategy == ProtossStrategy::DragoonAllIn ? 12000 : 10000) &&
                Units::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) < 2 &&
                enemyStrategy != ProtossStrategy::EarlyRobo &&
                enemyStrategy != ProtossStrategy::Turtle)
            {
                CherryVis::setBoardValue("natural", "too-early");
                break;
            }

            // We expect to want to take our natural soon, so build probes
            // Exception if we might want to take our hidden base
            buildProbes = (hiddenBasePlay == nullptr);

            auto squad = mainArmyPlay->getSquad();
            if (!squad || squad->getUnits().size() < 5)
            {
                CherryVis::setBoardValue("natural", "attack-play-too-small");
                break;
            }

            int dist;
            auto vanguardCluster = squad->vanguardCluster(&dist);
            if (!vanguardCluster)
            {
                CherryVis::setBoardValue("natural", "no-vanguard-cluster");
                break;
            }

            // Cluster should be past our own natural
            int naturalDist = PathFinding::GetGroundDistance(natural->getPosition(), mainArmyPlay->base->getPosition());
            if (naturalDist != -1 && dist > (naturalDist - 320))
            {
                CherryVis::setBoardValue("natural", "vanguard-cluster-not-beyond-natural");
                break;
            }

            // Get an observatory before expanding
//            if (Units::countAll(BWAPI::UnitTypes::Protoss_Observatory) < 1)
//            {
//                // TODO: When producer can handle requesting a building, request the observatory instead
//                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
//                                                                         "SE-safeexpand",
//                                                                         BWAPI::UnitTypes::Protoss_Observer,
//                                                                         1,
//                                                                         1);
//
//                CherryVis::setBoardValue("natural", "wait-for-obs");
//                break;
//            }

            // Always expand in this situation if we are gas blocked
            if (BWAPI::Broodwar->self()->minerals() > 500 && BWAPI::Broodwar->self()->gas() < 100)
            {
                CherryVis::setBoardValue("natural", "take-gas-blocked");
                takeNaturalExpansion(plays, prioritizedProductionGoals);
                return;
            }

            // Cluster should not be moving or fleeing
            // In other words, we want the cluster to be in some kind of stable attack or contain state
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
                || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                    && (vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee
                        || vanguardCluster->currentSubActivity == UnitCluster::SubActivity::StandGround)))
            {
                // We don't cancel a queued expansion in this case
                cancel = false;
                CherryVis::setBoardValue("natural", "vanguard-cluster-not-attacking");
                break;
            }

            // If the cluster is attacking, it should be significantly closer to the enemy main, unless we are past frame 12500
            if (currentFrame < 12500 &&
                vanguardCluster->currentActivity == UnitCluster::Activity::Attacking &&
                vanguardCluster->percentageToEnemyMain < 0.7)
            {
                CherryVis::setBoardValue("natural", "vanguard-cluster-too-close");
                break;
            }

            CherryVis::setBoardValue("natural", "take");
            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
        case OurStrategy::DTExpand:
        {
            // We never cancel an already-constructing nexus
            if (underConstruction)
            {
                CherryVis::setBoardValue("natural", "take");
                takeNaturalExpansion(plays, prioritizedProductionGoals);
                return;
            }

            buildProbes = true;

            // Take our natural as soon as the army has moved beyond it
            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyBase))
            {
                CherryVis::setBoardValue("natural", "no-attack-play");
                break;
            }

            auto squad = mainArmyPlay->getSquad();
            if (!squad)
            {
                CherryVis::setBoardValue("natural", "no-attack-play-squad");
                break;
            }

            auto vanguardCluster = squad->vanguardCluster();
            if (!vanguardCluster)
            {
                CherryVis::setBoardValue("natural", "no-vanguard-cluster");
                break;
            }

            // Ensure the cluster is at least 10 tiles further from the natural than it is from the main
            int distToMain = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->center);
            int distToNatural = PathFinding::GetGroundDistance(natural->getPosition(), vanguardCluster->center);
            if (distToNatural < 500 || distToMain < (distToNatural + 500))
            {
                CherryVis::setBoardValue("natural", "vanguard-too-close");
                break;
            }

            // Cluster should not be fleeing
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee)
            {
                CherryVis::setBoardValue("natural", "vanguard-fleeing");
                break;
            }

            CherryVis::setBoardValue("natural", "take");
            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
    }

    if (cancel) cancelNaturalExpansion(plays, prioritizedProductionGoals);

    // Take our hidden base expansion in cases where we have been prevented from taking our natural
    // It is queued at lowest priority so it will only be taken if we have an excess of minerals
    if (currentFrame > 12000)
    {
        if (hiddenBasePlay && hiddenBasePlay->base->ownedSince == -1)
        {
            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(hiddenBasePlay->base->getTilePosition()),
                                                                  0, 0, 0);
            prioritizedProductionGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     "SE-hiddenbase",
                                                                     BWAPI::UnitTypes::Protoss_Nexus,
                                                                     buildLocation);

        }
    }

    // If requested, build up to 10 probes for our natural
    // They are queued at lowest priority, so will not interfere with army production
    if (buildProbes)
    {
        auto idleWorkers = Workers::idleWorkerCount();
        if (idleWorkers < 10)
        {
            prioritizedProductionGoals[PRIORITY_LOWEST].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     "SE",
                                                                     BWAPI::UnitTypes::Protoss_Probe,
                                                                     10 - idleWorkers,
                                                                     1);
        }
    }
}

void PvP::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, false, true);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, true);

    // Upgrade observer speed on three gas
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 3)
    {
        upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    }

    defaultGroundUpgrades(prioritizedProductionGoals);

    // TODO: Air upgrades
}

void PvP::handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
    // The logic here is to look ahead to make sure we already have detection available when we need it

    // Break out if we already have an observer
    if (Units::countAll(BWAPI::UnitTypes::Protoss_Observer) > 0)
    {
        CherryVis::setBoardValue("detection", "have-observer");
        return;
    }

    // Break out if we are going 3 gate robo
    if (ourStrategy == OurStrategy::ThreeGateRobo)
    {
        CherryVis::setBoardValue("detection", "3-gate-robo");
        return;
    }

    auto buildObserver = [&](int frameNeeded = 0)
    {
        auto priority = Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0
                        ? PRIORITY_EMERGENCY
                        : PRIORITY_NORMAL;
        prioritizedProductionGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                          "SE-detection",
                                                          BWAPI::UnitTypes::Protoss_Observer,
                                                          1,
                                                          1,
                                                          frameNeeded);
    };

    // If the enemy is known to have produced a DT, get a cannon and observer
    if (enemyStrategy == ProtossStrategy::DarkTemplarRush ||
        Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Dark_Templar))
    {
        buildDefensiveCannons(prioritizedProductionGoals, !hasCannonAtWall(), 0, 1);
        buildObserver();
        CherryVis::setBoardValue("detection", "emergency-build-cannon-and-observer");
        return;
    }

    // Get an observer when we have a second gas
    if ((Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1 ||
        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1 && currentFrame > 10000))
        && Strategist::pressure() < 0.4)
    {
        CherryVis::setBoardValue("detection", "macro-build-observer");
        buildObserver();
        return;
    }

    // If our strategy is anti-zealot-rush and there is an enemy combat unit in our base, we have worse things to worry about
    if (ourStrategy == OurStrategy::AntiZealotRush)
    {
        for (const auto &unit : Units::allEnemy())
        {
            if (!unit->lastPositionValid) continue;
            if (!UnitUtil::IsCombatUnit(unit->type)) continue;
            if (!unit->type.canAttack()) continue;

            for (const auto &area : Map::getMyMainAreas())
            {
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition)) == area)
                {
                    CherryVis::setBoardValue("detection", "enemy-in-base");
                    return;
                }
            }
        }

        // Add some frame stops to ensure we don't build a cannon while the enemy is still producing zealots
        if ((currentFrame < 7000 && Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Zealot).size() >= 8) ||
            (currentFrame < 8000 && Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Zealot).size() >= 10) ||
            (currentFrame < 9000 && Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Zealot).size() >= 12))
        {
            CherryVis::setBoardValue("detection", "anti-zealot-rush");
            return;
        }
    }

    // Break out if we have detected a strategy that precludes a dark templar rush now
    bool zealotStrategy = enemyStrategy == ProtossStrategy::ProxyRush ||
                          enemyStrategy == ProtossStrategy::ZealotRush ||
                          enemyStrategy == ProtossStrategy::ZealotAllIn
                          || (enemyStrategy == ProtossStrategy::TwoGate && Units::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) > 5);
    if ((enemyStrategy == ProtossStrategy::EarlyForge && currentFrame < 6000)
        || (zealotStrategy && currentFrame < 6500)
        || (zealotStrategy && Units::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) > 1 && currentFrame < 9000)
        || (enemyStrategy == ProtossStrategy::DragoonAllIn && currentFrame < 8000)
        || (enemyStrategy == ProtossStrategy::EarlyRobo && currentFrame < 8000)
        || (enemyStrategy == ProtossStrategy::FastExpansion && currentFrame < 7000)
        || (enemyStrategy == ProtossStrategy::Turtle && currentFrame < 8000))
    {
        CherryVis::setBoardValue("detection", "strategy-exception");
        return;
    }

    // Initialize the expected DT completion frame based on previous game observations
    // We assume worst case until we have at least 10 games played against the opponent
    // If we have never lost to the opponent, we continue playing conservatively even if we have never seen a DT
    int expectedCompletionFrame = 7300;
    if (OpponentEconomicModel::enabled())
    {
        expectedCompletionFrame = OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Dark_Templar)
                + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar);
    }
    if (Opponent::winLossRatio(0.0, 200) < 0.99)
    {
        expectedCompletionFrame = std::min(Opponent::minValueInPreviousGames("firstDarkTemplarCompleted", expectedCompletionFrame, 15, 10), 20000);
    }

    // If we haven't found the enemy main, be conservative and assume we might see DTs 500 frames after earliest completion
    auto enemyMain = Map::getEnemyMain();
    if (!enemyMain)
    {
        buildDefensiveCannons(prioritizedProductionGoals, !hasCannonAtWall(), expectedCompletionFrame + 500);
        CherryVis::setBoardValue("detection", "cannon-at-worst-case");
        return;
    }

    // Otherwise compute when the enemy could get DTs based on our scouting information

    // First estimate when the enemy's templar archives will / might finish
    auto &templarArchiveTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Templar_Archives);
    if (!templarArchiveTimings.empty())
    {
        // We've scouted a templar archives directly, so use its completion frame
        // It's unlikely the enemy is building a templar archives as a fake-out
        expectedCompletionFrame =
                templarArchiveTimings.begin()->first
                + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives)
                + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar);
    }
    else
    {
        // If we've scouted a citadel, so assume the templar archives is started as soon as the citadel finishes, unless
        // we've scouted the base in the meantime or our previous game data indicates it is likely to be a fake-out
        auto &citadelTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
        if (!citadelTimings.empty() && expectedCompletionFrame < 10000)
        {
            expectedCompletionFrame =
                    UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives)
                    + std::max(enemyMain->lastScouted,
                               citadelTimings.begin()->first + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Citadel_of_Adun))
                               + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar);
        }
    }

    // Compute the transit time from the enemy's closest gateway
    auto myMainChoke = Map::getMyMainChoke();
    auto myPosition = myMainChoke ? myMainChoke->center : Map::getMyMain()->getPosition();
    int closestGatewayFrames = PathFinding::ExpectedTravelTime(enemyMain->getPosition(),
                                                               myPosition,
                                                               BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                               PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                               1.1);
    for (const auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Gateway))
    {
        if (!unit->completed) continue;

        int frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                     myPosition,
                                                     BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                     PathFinding::PathFindingOptions::UseNearestBWEMArea,
                                                     1.1,
                                                     -1);
        if (frames != -1 && frames < closestGatewayFrames)
        {
            closestGatewayFrames = frames;
        }
    }

#if OUTPUT_DETECTION_DEBUG
    CherryVis::log() << "detection: expected DT completion @ " << expectedCompletionFrame
                     << "; DT at our choke @ "
                     << (closestGatewayFrames + expectedCompletionFrame);
#endif

    // Now sum everything up to get the frame where we need detection
    int frame = expectedCompletionFrame + closestGatewayFrames;
    if (frame < 0) return;
    if (templarArchiveTimings.empty())
    {
        buildDefensiveCannons(prioritizedProductionGoals, !hasCannonAtWall(), frame);
        CherryVis::setBoardValue("detection", (std::ostringstream() << "cannon-at-" << frame).str());
    }
    else
    {
        buildObserver(frame);
        buildDefensiveCannons(prioritizedProductionGoals, !hasCannonAtWall(), frame, 1);
        CherryVis::setBoardValue("detection", (std::ostringstream() << "cannon-and-observer-at-" << frame).str());
    }
}
