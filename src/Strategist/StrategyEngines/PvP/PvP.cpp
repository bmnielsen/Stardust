#include "StrategyEngines/PvP.h"

#include "Units.h"
#include "Map.h"
#include "Builder.h"
#include "UnitUtil.h"
#include "Strategist.h"
#include "Workers.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyMain.h"
#include "Plays/MainArmy/MopUp.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Plays/Scouting/EjectEnemyScout.h"
#include "Plays/Defensive/AntiCannonRush.h"
#include "Plays/SpecialTeams/DarkTemplarHarass.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define OUTPUT_DETECTION_DEBUG false
#endif

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;

    template<class T, class ...Args>
    void setMainPlay(MainArmyPlay *current, Args &&...args)
    {
        if (typeid(*current) == typeid(T)) return;
        current->status.transitionTo = std::make_shared<T>(std::forward<Args>(args)...);
    }
}

void PvP::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<EjectEnemyScout>());
    plays.emplace_back(std::make_shared<DefendMyMain>());
}

void PvP::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto newEnemyStrategy = recognizeEnemyStrategy();
    auto newStrategy = chooseOurStrategy(newEnemyStrategy, plays);

    if (enemyStrategy != newEnemyStrategy)
    {
        Log::Get() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#endif

        enemyStrategy = newEnemyStrategy;
        enemyStrategyChanged = BWAPI::Broodwar->getFrameCount();
    }

    if (ourStrategy != newStrategy)
    {
        Log::Get() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#endif

        ourStrategy = newStrategy;
    }

    // Ensure we have the correct main army play
    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
    if (mainArmyPlay)
    {
        if (hasEnemyStolenOurGas())
        {
            setMainPlay<DefendMyMain>(mainArmyPlay);
        }
        else
        {
            auto setAttackPlay = [&mainArmyPlay]()
            {
                auto enemyMain = Map::getEnemyMain();
                if (enemyMain)
                {
                    setMainPlay<AttackEnemyMain>(mainArmyPlay, Map::getEnemyMain());
                }
                else
                {
                    setMainPlay<MopUp>(mainArmyPlay);
                }
            };

            switch (ourStrategy)
            {
                case OurStrategy::EarlyGameDefense:
                case OurStrategy::AntiZealotRush:
                case OurStrategy::Defensive:
                    setMainPlay<DefendMyMain>(mainArmyPlay);
                    break;
                case OurStrategy::DTExpand:
                    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Dark_Templar) < 1)
                    {
                        setMainPlay<DefendMyMain>(mainArmyPlay);
                    }
                    else
                    {
                        setAttackPlay();
                    }
                    break;
                case OurStrategy::FastExpansion:
                case OurStrategy::Normal:
                case OurStrategy::MidGame:
                {
                    // Always use a defend play if the squad has no units or we have no obs to counter DTs
                    auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                    if (!vanguard
                        || ((Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Dark_Templar) || enemyStrategy == ProtossStrategy::DarkTemplarRush)
                            && Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) == 0))
                    {
                        setMainPlay<DefendMyMain>(mainArmyPlay);
                        break;
                    }

                    // Transition from a defend squad when the vanguard cluster has 3 units and can do so
                    if (typeid(*mainArmyPlay) == typeid(DefendMyMain) &&
                        vanguard->units.size() >= 3 &&
                        ((DefendMyMain *) mainArmyPlay)->canTransitionToAttack())
                    {
                        setAttackPlay();
                    }

                    // Transition to a defend squad if our attack squad has been pushed back into our main
                    if (typeid(*mainArmyPlay) == typeid(AttackEnemyMain))
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
                                setMainPlay<DefendMyMain>(mainArmyPlay);
                            }
                        }
                    }

                    break;
                }
            }
        }
    }

    // Ensure we have an anti cannon rush play if the enemy strategy warrants it
    if (BWAPI::Broodwar->getFrameCount() < 6000)
    {
        auto antiCannonRushPlay = getPlay<AntiCannonRush>(plays);
        if (enemyStrategy == ProtossStrategy::EarlyForge ||
            enemyStrategy == ProtossStrategy::ProxyRush ||
            enemyStrategy == ProtossStrategy::BlockScouting ||
            (enemyStrategy == ProtossStrategy::Unknown && BWAPI::Broodwar->getFrameCount() > 2000))
        {
            if (!antiCannonRushPlay)
            {
                plays.insert(plays.begin(), std::make_shared<AntiCannonRush>());
            }
        }
        else if (antiCannonRushPlay && enemyStrategy == ProtossStrategy::FastExpansion)
        {
            antiCannonRushPlay->status.complete = true;
        }
    }

    // Ensure we have a DarkTemplarHarass play if we have any DTs
    {
        auto darkTemplarHarassPlay = getPlay<DarkTemplarHarass>(plays);
        bool haveDarkTemplar = Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0;
        if (darkTemplarHarassPlay && !haveDarkTemplar)
        {
            darkTemplarHarassPlay->status.complete = true;
        }
        else if (haveDarkTemplar && !darkTemplarHarassPlay)
        {
            plays.emplace_back(std::make_shared<DarkTemplarHarass>());
        }
    }

    // Set the worker scout mode
    if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted ||
        Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::MonitoringEnemyChoke)
    {
        auto play = getPlay<EarlyGameWorkerScout>(plays);
        if (play)
        {
            auto setScoutHiding = [&play](int hideUntil)
            {
                if (BWAPI::Broodwar->getFrameCount() > hideUntil) return;
                play->hideUntil(hideUntil);
            };

            switch (enemyStrategy)
            {
                case ProtossStrategy::Unknown:
                case ProtossStrategy::ProxyRush:
                case ProtossStrategy::BlockScouting:
                case ProtossStrategy::DragoonAllIn:
                case ProtossStrategy::DarkTemplarRush:
                    setScoutHiding(0);
                    break;
                case ProtossStrategy::ZealotRush:
                case ProtossStrategy::TwoGate:
                case ProtossStrategy::ZealotAllIn:
                    setScoutHiding(0);
                    play->monitorEnemyChoke();
                    break;
                case ProtossStrategy::EarlyForge:
                case ProtossStrategy::OneGateCore:
                case ProtossStrategy::FastExpansion:
                case ProtossStrategy::EarlyRobo:
                case ProtossStrategy::Turtle:
                case ProtossStrategy::MidGame:
                    setScoutHiding(6000);
                    break;
            }
        }
    }

    UpdateDefendBasePlays(plays);

    defaultExpansions(plays);
}

void PvP::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(prioritizedProductionGoals);

    // Temporary hack to set the number of gas workers needed until the producer can do it
    auto setGasGathering = [](bool gather)
    {
        int current = Workers::desiredGasWorkers();
        int desired = (gather || Workers::availableMineralAssignments() < 2)
                      ? (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) * 3)
                      : 0;
        Workers::addDesiredGasWorkers(desired - current);
    };

    // Default to gather gas - we will only set to false later if we are being rushed
    setGasGathering(true);

    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
    auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
    auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

    int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
    int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

    handleGasStealProduction(prioritizedProductionGoals, zealotCount);

    auto oneGateCoreOpening = [&](int numZealots)
    {
        // If our core is done or we want no zealots just return dragoons
        if (numZealots == 0 || Units::countCompleted(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            return;
        }

        // Ensure gas before zealot
        if (Units::countAll(BWAPI::UnitTypes::Protoss_Assimilator) == 0)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       1,
                                                                       -1);
            return;
        }

        if (zealotCount == 0)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       1,
                                                                       1);
        }
        if (dragoonCount == 0)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       1,
                                                                       1);
        }
        if (zealotCount < numZealots)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       numZealots - zealotCount,
                                                                       1);
        }

        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Dragoon,
                                                                   -1,
                                                                   -1);
    };

    // Main army production
    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        {
            // Start with one-gate core with two zealots until we have more scouting information
            oneGateCoreOpening(2);
            break;
        }
        case OurStrategy::AntiZealotRush:
        {
            // We get at least four zealots, but ensure we match enemy zealot production to avoid getting overrun
            int desiredZealots = 4 + (Units::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) * 3) / 4;
            int zealotsRequired = desiredZealots - zealotCount;

            // Get two zealots at highest priority
            if (zealotCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                            BWAPI::UnitTypes::Protoss_Zealot,
                                                                            2 - zealotCount,
                                                                            2);
                zealotsRequired -= 2 - zealotCount;
            }

            if (zealotsRequired > 0)
            {
                prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                              BWAPI::UnitTypes::Protoss_Zealot,
                                                                              zealotsRequired,
                                                                              -1);
                if (zealotsRequired > 1 || BWAPI::Broodwar->self()->gas() >= 50)
                {
                    setGasGathering(false);
                }
            }

            // If the dragoon transition is just beginning, only order one so we keep producing zealots
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       dragoonCount == 0 ? 1 : -1,
                                                                       -1);

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            // Upgrade goon range at 2 dragoons unless we are still behind in zealots
            if (zealotsRequired == 0)
            {
                upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);
            }

            break;
        }

        case OurStrategy::FastExpansion:
        {
            oneGateCoreOpening(0);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }

        case OurStrategy::Defensive:
        case OurStrategy::Normal:
        {
            oneGateCoreOpening(1);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }
        case OurStrategy::MidGame:
        {
            // Have a couple of DTs on hand if we have a templar archives and the enemy hasn't built mobile detection
            int dtCount = Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar);
            if (Units::countAll(BWAPI::UnitTypes::Protoss_Templar_Archives) > 0 && dtCount < 2 &&
                !Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer) &&
                !Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory))
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                           2 - dtCount,
                                                                           2);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }
        case OurStrategy::DTExpand:
        {
            int dtCount = Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar);
            if (dtCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         2 - dtCount,
                                                                         1);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);

            // Make sure we get dragoon range to defend our choke effectively
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            break;
        }
    }
}

void PvP::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been (or is being) taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1) return;
    if (Builder::isPendingHere(natural->getTilePosition())) return;

    auto takeNatural = [&]()
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                              BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                               natural->getTilePosition(),
                                                                                               BWAPI::UnitTypes::Protoss_Nexus),
                                                              0, 0);
        prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 BWAPI::UnitTypes::Protoss_Nexus,
                                                                 buildLocation);
    };

    // If we have a backdoor natural, expand if we are gas blocked
    // This generally happens when we are teching to DT or observers
    if (Map::mapSpecificOverride()->hasBackdoorNatural())
    {
        if (BWAPI::Broodwar->self()->minerals() > 400 &&
            BWAPI::Broodwar->self()->gas() < 100 &&
            (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed()) >= 6)
        {
            takeNatural();
            return;
        }
    }

    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiZealotRush:
        case OurStrategy::Defensive:
            // Don't take our natural if the enemy could be rushing or doing an all-in
            break;

        case OurStrategy::FastExpansion:
            takeNatural();
            break;

        case OurStrategy::Normal:
        case OurStrategy::MidGame:
        {
            // In this case we want to expand when we consider it safe to do so: we have an attacking or containing army
            // that is close to the enemy base

            // We never expand before frame 10000 unless the enemy has done so
            if (BWAPI::Broodwar->getFrameCount() < 10000 && Units::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) < 2) return;

            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyMain)) return;

            auto squad = mainArmyPlay->getSquad();
            if (!squad || squad->getUnits().size() < 5) return;

            int dist;
            auto vanguardCluster = squad->vanguardCluster(&dist);
            if (!vanguardCluster) return;

            // Cluster should be at least 2/3 of the way to the target base
            int distToMain = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->center);
            if (dist * 2 > distToMain) return;

            // Always expand in this situation if we are gas blocked
            if (BWAPI::Broodwar->self()->minerals() > 500 && BWAPI::Broodwar->self()->gas() < 100)
            {
                takeNatural();
                break;
            }

            // Cluster should not be moving or fleeing
            // In other words, we want the cluster to be in some kind of stable attack or contain state
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
                || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                    && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee))
            {
                return;
            }

            takeNatural();
            break;
        }
        case OurStrategy::DTExpand:
        {
            // Take our natural as soon as the army has moved beyond it
            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyMain)) return;

            auto squad = mainArmyPlay->getSquad();
            if (!squad) return;

            auto vanguardCluster = squad->vanguardCluster();
            if (!vanguardCluster) return;

            // Ensure the cluster is at least 10 tiles further from the natural than it is from the main
            int distToMain = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->center);
            int distToNatural = PathFinding::GetGroundDistance(natural->getPosition(), vanguardCluster->center);
            if (distToNatural < 500 || distToMain < (distToNatural + 500)) return;

            // Cluster should not be fleeing
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee)
            {
                return;
            }

            takeNatural();
            break;
        }
    }
}

void PvP::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, true);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier);

    // Upgrade observer speed on two gas
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 2)
    {
        upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
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

    auto framesNeededFor = [](BWAPI::UnitType buildingType)
    {
        int earliestCompletion = INT_MAX;
        for (const auto &unit : Units::allMineIncompleteOfType(buildingType))
        {
            if (unit->estimatedCompletionFrame < earliestCompletion)
            {
                earliestCompletion = unit->estimatedCompletionFrame;
            }
        }

        if (earliestCompletion == INT_MAX)
        {
            return UnitUtil::BuildTime(buildingType);
        }

        return earliestCompletion - BWAPI::Broodwar->getFrameCount();
    };

    auto buildObserver = [&](int frameNeeded = 0)
    {
        // If we know what frame the observer is needed, check if we need to start building it now
        if (frameNeeded > 0)
        {
            int frameStarted = frameNeeded - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Observer);
            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observatory) == 0)
            {
                frameStarted -= framesNeededFor(BWAPI::UnitTypes::Protoss_Observatory);

                if (Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observatory) == 0 &&
                    Units::countCompleted(BWAPI::UnitTypes::Protoss_Robotics_Facility) == 0)
                {
                    frameStarted -= framesNeededFor(BWAPI::UnitTypes::Protoss_Robotics_Facility);
                }
            }

            // TODO: When the Producer understands to build something at a specific frame, use that
            if ((BWAPI::Broodwar->getFrameCount() + 500) < frameStarted) return;
        }

        auto priority = Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0
                        ? PRIORITY_EMERGENCY
                        : PRIORITY_NORMAL;
        prioritizedProductionGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                          BWAPI::UnitTypes::Protoss_Observer,
                                                          1,
                                                          1);
    };

    auto buildCannon = [&](int frameNeeded = 0, bool alsoAtBases = false)
    {
        auto buildCannonAt = [&](BWAPI::TilePosition pylonTile, BWAPI::TilePosition cannonTile)
        {
            if (!pylonTile.isValid()) return;
            if (!cannonTile.isValid()) return;

            // If we know what frame the cannon is needed, check if we need to start building it now
            if (frameNeeded > 0)
            {
                int frameStarted = frameNeeded - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
                if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Forge) == 0)
                {
                    frameStarted -= framesNeededFor(BWAPI::UnitTypes::Protoss_Forge);
                }
                else if (!Units::myBuildingAt(pylonTile))
                {
                    frameStarted -= UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);
                }

                // TODO: When the Producer understands to build something at a specific frame, use that
                if ((BWAPI::Broodwar->getFrameCount() + 500) < frameStarted) return;
            }

            auto buildAtTile = [&prioritizedProductionGoals](BWAPI::TilePosition tile, BWAPI::UnitType type)
            {
                if (Units::myBuildingAt(tile) != nullptr) return;
                if (Builder::isPendingHere(tile)) return;

                auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile), 0, 0, 0);
                auto priority = Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0
                                ? PRIORITY_EMERGENCY
                                : PRIORITY_NORMAL;
                prioritizedProductionGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                  type,
                                                                  buildLocation);
            };

            auto pylon = Units::myBuildingAt(pylonTile);
            if (pylon && pylon->completed)
            {
                buildAtTile(cannonTile, BWAPI::UnitTypes::Protoss_Photon_Cannon);
            }
            else
            {
                buildAtTile(pylonTile, BWAPI::UnitTypes::Protoss_Pylon);
            }
        };

        // Get the cannon location
        auto cannonLocations = BuildingPlacement::mainChokeCannonLocations();
        if (cannonLocations.first != BWAPI::TilePositions::Invalid)
        {
            buildCannonAt(cannonLocations.first, cannonLocations.second);
        }
        else
        {
            alsoAtBases = true;
        }

        if (alsoAtBases)
        {
            auto buildAtBase = [&buildCannonAt](Base *base)
            {
                if (!base || base->owner != BWAPI::Broodwar->self()) return;

                auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                if (baseStaticDefenseLocations.first != BWAPI::TilePositions::Invalid)
                {
                    buildCannonAt(baseStaticDefenseLocations.first, *baseStaticDefenseLocations.second.begin());
                }
            };

            buildAtBase(Map::getMyMain());
            if (!Map::mapSpecificOverride()->hasBackdoorNatural())
            {
                buildAtBase(Map::getMyNatural());
            }
        }
    };

    // If the enemy is known to have produced a DT, get a cannon and observer
    if (enemyStrategy == ProtossStrategy::DarkTemplarRush ||
        Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Dark_Templar))
    {
        buildCannon(0, true);
        buildObserver();
        CherryVis::setBoardValue("detection", "emergency-build-cannon-and-observer");
        return;
    }

    // Get an observer when we have a second gas
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1)
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
                    CherryVis::setBoardValue("detection", "anti-zealot-rush");
                    return;
                }
            }
        }
    }

    // Break out if we have detected a strategy that precludes a dark templar rush now
    if ((enemyStrategy == ProtossStrategy::EarlyForge && BWAPI::Broodwar->getFrameCount() < 6000)
        || (enemyStrategy == ProtossStrategy::ProxyRush && BWAPI::Broodwar->getFrameCount() < 6000)
        || (enemyStrategy == ProtossStrategy::ZealotRush && BWAPI::Broodwar->getFrameCount() < 6000)
        || (enemyStrategy == ProtossStrategy::ZealotAllIn && BWAPI::Broodwar->getFrameCount() < 6000)
        || (enemyStrategy == ProtossStrategy::DragoonAllIn && BWAPI::Broodwar->getFrameCount() < 8000)
        || (enemyStrategy == ProtossStrategy::EarlyRobo && BWAPI::Broodwar->getFrameCount() < 8000)
        || (enemyStrategy == ProtossStrategy::FastExpansion && BWAPI::Broodwar->getFrameCount() < 7000)
        || (enemyStrategy == ProtossStrategy::Turtle && BWAPI::Broodwar->getFrameCount() < 8000))
    {
        CherryVis::setBoardValue("detection", "strategy-exception");
        return;
    }

    // If we haven't found the enemy main, be conservative and assume we might see DTs by frame 7800
    auto enemyMain = Map::getEnemyMain();
    if (!enemyMain)
    {
        buildCannon(7800);
        CherryVis::setBoardValue("detection", "cannon-at-7000");
        return;
    }

    // Otherwise compute when the enemy could get DTs based on our scouting information

    // First estimate when the enemy's templar archives will / might finish
    int templarArchivesFinished;
    auto &templarArchiveTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Templar_Archives);
    if (!templarArchiveTimings.empty())
    {
        // We've scouted a templar archives directly, so use its completion frame
        templarArchivesFinished = templarArchiveTimings.begin()->first + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives);
    }
    else
    {
        auto &citadelTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
        if (!citadelTimings.empty())
        {
            // We've scouted a citadel, so assume the templar archives is started as soon as the citadel finishes, unless
            // we've scouted the base in the meantime
            templarArchivesFinished = UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives) +
                                      std::max(enemyMain->lastScouted,
                                               citadelTimings.begin()->first + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Citadel_of_Adun));
        }
        else
        {
            // We haven't scouted a templar archives or citadel, so assume the enemy started at frame 4000 unless we have scouted
            // since then
            templarArchivesFinished = UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives) +
                                      UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) +
                                      std::max(4000, enemyMain->lastScouted);
        }
    }

    // Next compute the transit time from the enemy's closest gateway
    auto myMainChoke = Map::getMyMainChoke();
    auto myPosition = myMainChoke ? myMainChoke->center : Map::getMyMain()->getPosition();
    int closestGatewayFrames = PathFinding::ExpectedTravelTime(enemyMain->getPosition(),
                                                               myPosition,
                                                               BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                               PathFinding::PathFindingOptions::UseNearestBWEMArea);
    for (const auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Gateway))
    {
        if (!unit->completed) continue;

        int frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                     myPosition,
                                                     BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                     PathFinding::PathFindingOptions::UseNearestBWEMArea);
        if (frames < closestGatewayFrames)
        {
            closestGatewayFrames = frames;
        }
    }

#if OUTPUT_DETECTION_DEBUG
    CherryVis::log() << "detection: expect templar archives @ " << templarArchivesFinished
                     << "; DT @ " << (UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar) + templarArchivesFinished)
                     << "; DT at our choke @ "
                     << (closestGatewayFrames + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar) + templarArchivesFinished);
#endif

    // Now sum everything up to get the frame where we need detection
    int frame = templarArchivesFinished + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar) + closestGatewayFrames;
    if (templarArchiveTimings.empty())
    {
        buildCannon(frame);
        CherryVis::setBoardValue("detection", (std::ostringstream() << "cannon-at-" << frame).str());
    }
    else
    {
        buildObserver(frame);
        buildCannon(frame, true);
        CherryVis::setBoardValue("detection", (std::ostringstream() << "cannon-and-observer-at-" << frame).str());
    }
}
