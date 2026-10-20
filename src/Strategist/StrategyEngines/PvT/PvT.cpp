#include "StrategyEngines/PvT.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Workers.h"
#include "Builder.h"
#include "Opponent.h"
#include "Config.h"
#include "FileTools.h"

#include <fstream>

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/MainArmy/ForgeFastExpand.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Plays/Scouting/EjectEnemyScout.h"
#include "Plays/SpecialTeams/Elevator.h"
#include "Plays/SpecialTeams/ElevatorRush.h"

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

void PvT::initialize(std::vector<std::shared_ptr<PlayItem>> &playList, bool transitioningFromRandom, const std::string &openingOverride)
{
    if (!openingOverride.empty()) playList.clear();

    if (transitioningFromRandom)
    {
        playList.emplace(beforePlayIt<MainArmyPlay>(playList), std::make_shared<EjectEnemyScout>());
        playList.emplace(beforePlayIt<MainArmyPlay>(playList), std::make_shared<Elevator>());

        if (getPlay<ForgeFastExpand>(playList))
        {
            ourStrategy = OurStrategy::ForgeExpandGoons;
        }
    }
    else
    {
        playList.emplace_back(std::make_shared<SaturateBases>());
        playList.emplace_back(std::make_shared<EarlyGameWorkerScout>());
        playList.emplace_back(std::make_shared<EjectEnemyScout>());
        playList.emplace_back(std::make_shared<Elevator>());
        playList.emplace_back(std::make_shared<DefendMyMain>());
    }

    /* AIST S4 vs. Human match - do a fast expansion on Fighting Spirit and Circuit Breaker */
    /*
    if (BWAPI::Broodwar->mapHash() == "d2f5633cc4bb0fca13cd1250729d5530c82c7451" ||
        BWAPI::Broodwar->mapHash() == "dcabb11c83e68f47c5c5bdbea0204167a00e336f" ||
        BWAPI::Broodwar->mapHash() == "5731c103687826de48ba3cc7d6e37e2537b0e902" ||
        BWAPI::Broodwar->mapHash() == "bf84532dcdd21b3328670d766edc209fa1520149" ||
        BWAPI::Broodwar->mapHash() == "450a792de0e544b51af5de578061cb8a2f020f32" ||
        BWAPI::Broodwar->mapHash() == "1221d83d6ff9a87955d3083257b31131261bc366")
    {
        ourStrategy = OurStrategy::FastExpansion;
    }
    */

    Opponent::addMyStrategyChange(OurStrategyNames[ourStrategy]);
    Opponent::addEnemyStrategyChange(TerranStrategyNames[enemyStrategy]);
}

void PvT::updatePlays(std::vector<std::shared_ptr<PlayItem>> &playList)
{
    auto newEnemyStrategy = recognizeEnemyStrategy();

    auto newStrategy = chooseOurStrategy(newEnemyStrategy, playList);

    if (enemyStrategy != newEnemyStrategy)
    {
#if LOGGING_ENABLED
        AppLog::Get() << "Enemy strategy changed from " << TerranStrategyNames[enemyStrategy] << " to " << TerranStrategyNames[newEnemyStrategy];
#endif
#if CHERRYVIS_ENABLED
        Telemetry::log() << "Enemy strategy changed from " << TerranStrategyNames[enemyStrategy] << " to " << TerranStrategyNames[newEnemyStrategy];
#endif

        enemyStrategy = newEnemyStrategy;
        enemyStrategyChanged = frameNow;
        Opponent::addEnemyStrategyChange(TerranStrategyNames[enemyStrategy]);
    }

    if (ourStrategy != newStrategy)
    {
#if LOGGING_ENABLED
        AppLog::Get() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#endif
#if CHERRYVIS_ENABLED
        Telemetry::log() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#endif

        /* AIST S4 vs. Human match - When we transition from early-game defense to normal on specific maps, do an elevator rush */
/* if (ourStrategy == OurStrategy::EarlyGameDefense && newStrategy == OurStrategy::NormalOpening && */
/* (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b" || */
/* BWAPI::Broodwar->mapHash() == "e39c1c81740a97a733d227e238bd11df734eaf96" || */
/* BWAPI::Broodwar->mapHash() == "6f8da3c3cc8d08d9cf882700efa049280aedca8c" || */
/* BWAPI::Broodwar->mapHash() == "fe25d8b79495870ac1981c2dfee9368f543321e3" || */
/* BWAPI::Broodwar->mapHash() == "d9757c0adcfd61386dff8fe3e493e9e8ef9b45e3" || */
/* BWAPI::Broodwar->mapHash() == "ecb9c70c5594a5c6882baaf4857a61824fba0cfa")) */
/* { */
/* plays.insert(plays.begin(), std::make_shared<Elevator>()); */
/* } */

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
            case OurStrategy::ForgeExpandGoons:
            {
                /* Attack when we have goon range */
                defendOurMain = (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0);
                break;
            }

            case OurStrategy::EarlyGameDefense:
            case OurStrategy::AntiMarineRush:
            case OurStrategy::Defensive:
                defendOurMain = true;
                break;
            default:
            {
                auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
                if (!mainArmyPlay)
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

                defendOurMain = false;

                /* Transition from a defend squad when the vanguard cluster has 2 units */
                if (mainArmyPlay->isDefensive())
                {
                    auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                    defendOurMain = (!vanguard || vanguard->units.size() < 2);
                }

                break;
            }
        }
    }

    updateAttackPlays(playList, defendOurMain);
    updateDefendBasePlays(playList);
    updateSpecialTeamsPlays(playList);
    defaultExpansions(playList);
    scoutExpos(playList, 15000);

    /* AIST S4 vs. Human match - Harass enemy bases with carriers after we have our first two arbiters out */
    /*
    if (!defendOurMain && Units::countAll(BWAPI::UnitTypes::Protoss_Arbiter) > 1)
    {
        auto carrierHarass = getPlay<CarrierHarass>(plays);
        if (!carrierHarass)
        {
            auto beforeMainArmyIt = [&plays]()
            {
                auto it = plays.begin();
                for (; it != plays.end(); it++)
                {
                    if (std::dynamic_pointer_cast<MainArmyPlay>(*it) != nullptr)
                    {
                        break;
                    }
                }
                return it;
            };
            plays.emplace(beforeMainArmyIt(), std::make_shared<CarrierHarass>());
        }
    }
    */
}

void PvT::updateProduction(std::vector<std::shared_ptr<PlayItem>> &playList,
                           std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
    handleNaturalExpansion(playList, prioProdGoals);
    handleDetection(playList, prioProdGoals);

    if (handleIslandExpansionProduction(playList, prioProdGoals)) return;

    auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
    auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
    auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

    int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
    int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

    int inProgressCount = UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot)
                          + UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Dragoon)
                          + UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Dark_Templar);

    if (!getPlay<ForgeFastExpand>(playList))
    {
        handleGasStealProduction(prioProdGoals, zealotCount);
    }

    auto midAndLateGameMainArmyProduction = [&]()
    {
        /* Baseline production is one combat unit for every 6 workers (approximately 3 units per mining base) */
        int higherPriorityCount = (WorkerMgr::mineralWorkers() / 6) - inProgressCount;

        /* Counter tanks with speedlots once the enemy has at least four */
        int enemyTanks = UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                         UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
        if (enemyTanks > 2)
        {
            /* Keep proportionally more dragoons if the enemy has a lot of vultures */
            int desiredZealots = std::min((dragoonCount * 3) / 2, enemyTanks * 3);
            if (UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Vulture) > enemyTanks)
            {
                desiredZealots = std::min(dragoonCount, enemyTanks * 2);
            }

            if (desiredZealots > zealotCount)
            {
                mainArmyProduction(prioProdGoals,
                                   BWAPI::UnitTypes::Protoss_Zealot,
                                   desiredZealots - zealotCount,
                                   higherPriorityCount);
            }

            upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 0);
        }

        mainArmyProduction(prioProdGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);
        mainArmyProduction(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot, -1, higherPriorityCount);
    };

    switch (ourStrategy)
    {
        case OurStrategy::ForgeExpandGoons:
        {
            prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);
            break;
        }

        case OurStrategy::EarlyGameDefense:
        {
            oneGateCoreOpening(prioProdGoals, dragoonCount, zealotCount, 1);
            upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);

            break;
        }
        case OurStrategy::AntiMarineRush:
        {
            auto buildCannons = [&prioProdGoals](int desiredCount)
            {
                auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(Map::getMyMain());
                if (!baseStaticDefenseLocations.isValid()) return 0;

                std::vector<BWAPI::TilePosition> locations = baseStaticDefenseLocations.workerDefenseCannons;
                if (baseStaticDefenseLocations.startBlockCannon != BWAPI::TilePositions::Invalid)
                {
                    locations.insert(locations.begin() + 1, baseStaticDefenseLocations.startBlockCannon);
                }

                int completedCannons = 0;
                for (const auto &location : locations)
                {
                    if (!location.isValid()) continue;

                    auto cannon = UnitMgr::myBuildingAt(location);
                    if (cannon && cannon->type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
                    {
                        desiredCount--;

                        if (cannon->completed || cannon->estimatedCompletionFrame < (frameNow + 200))
                        {
                            completedCannons++;
                        }

                        continue;
                    }

                    if (desiredCount > 0)
                    {
                        auto buildAtTile = [&prioProdGoals](BWAPI::TilePosition tilePos, BWAPI::UnitType type)
                        {
                            if (Constructor::isPendingHere(tilePos)) return;

                            auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(tilePos),
                                                                                  BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                                                   tilePos,
                                                                                                                   type),
                                                                                  0,
                                                                                  0);
                            prioProdGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                                        "SE-antirush",
                                                                                        type,
                                                                                        buildLocation);
                        };

                        auto pylon = UnitMgr::myBuildingAt(baseStaticDefenseLocations.powerPylon);
                        if (pylon)
                        {
                            if (pylon->completed)
                            {
                                buildAtTile(location, BWAPI::UnitTypes::Protoss_Photon_Cannon);
                                desiredCount--;
                            }
                        }
                        else if (!pylon)
                        {
                            buildAtTile(baseStaticDefenseLocations.powerPylon, BWAPI::UnitTypes::Protoss_Pylon);
                            return completedCannons;
                        }
                    }
                }

                return completedCannons;
            };

            /* Build cannons if the strategy has been stable for 5 seconds */
            int desiredCannons = 0;
            if (enemyStrategyChanged < (frameNow - 120))
            {
                desiredCannons = 2;
            }
            int completedCannons = buildCannons(desiredCannons);

            /* Determine how many zealots we want */
            /* Zealots are relatively useless against groups of kiting marines, so we want to transition to dragoons as quickly as possible */
            int zealotsRequired = std::max(0, (dragoonCount < 3 ? 3 : 2) - (completedCannons * 2) - zealotCount);
            handleAntiRushProduction(prioProdGoals, dragoonCount, zealotCount, zealotsRequired, 1);

            break;
        }

        case OurStrategy::FastExpansion:
        case OurStrategy::Defensive:
        case OurStrategy::NormalOpening:
        {
            /* If any zealots are in production, and we don't have an emergency production goal, cancel them */
            if (UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot) > 0)
            {
                cancelTrainingUnits(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot);
            }

            /* Try to keep at least two army units in production while taking our natural */
            int higherPriorityCount = 2 - inProgressCount;
            mainArmyProduction(prioProdGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);

            /* Default upgrades */
            handleUpgrades(prioProdGoals);

            break;
        }

        case OurStrategy::MidGame:
        {
            /* Have some DTs harrassing until the enemy has vessels */
            /* TODO: Implement DT tactics and re-enable */
            /*
            int dtCount = Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar);
            if (dtCount < 4 &&
                !Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Vessel) &&
                !Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Facility))
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE-nodetect",
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         1,
                                                                         1);
            }
             */

            /* Against mech, get one shuttle unless the enemy has more than one goliath or many marines */
            /* Obs may get higher priority depending on what we have scouted */
            if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Shuttle) < 1 &&
                enemyStrategy == TerranStrategy::MidGameMech &&
                UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Goliath) < 2 &&
                UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Marine) < 10)
            {
                prioProdGoals[PRIORITY_SPECIALTEAMS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                               "SE",
                                                                               BWAPI::UnitTypes::Protoss_Shuttle,
                                                                               1,
                                                                               1);
            }

            /* Build arbiters on three completed nexuses */
            int arbiterCount = UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Arbiter);
            if (arbiterCount < 2 && UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 2)
            {
                prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         "SE",
                                                                         BWAPI::UnitTypes::Protoss_Arbiter,
                                                                         1,
                                                                         1);
            }

            midAndLateGameMainArmyProduction();

            /* Default upgrades */
            handleUpgrades(prioProdGoals);

            break;
        }
    }
}

void PvT::handleNaturalExpansion(std::vector<std::shared_ptr<PlayItem>> &playList,
                                 std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Hop out if the natural has already been taken */
    auto natBase = Map::getMyNatural();
    if (!natBase || natBase->ownedSince != -1)
    {
        Telemetry::setBoardValue("natural", "complete");
        return;
    }

    /* Don't expand while we are setting up an elevator rush */
    auto elevatorRush = getPlay<ElevatorRush>(playList);
    if (elevatorRush && elevatorRush->getSquad()->empty())
    {
        Telemetry::setBoardValue("natural", "wait-elevator");
        return;
    }

    /* If we have a backdoor natural, expand when our second goon is being produced or we have lots of money */
    if (Map::mapSpecificOverride()->hasBackdoorNatural())
    {
        if (BWAPI::Broodwar->self()->minerals() > 450 ||
            UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Dragoon) > 1)
        {
            Telemetry::setBoardValue("natural", "take-backdoor");

            takeNaturalExpansion(playList, prioProdGoals);
            return;
        }
    }

    switch (ourStrategy)
    {
        case OurStrategy::ForgeExpandGoons:
            /* Handled by the play */
            Telemetry::setBoardValue("natural", "forge-expand");
            return;

        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiMarineRush:
        case OurStrategy::Defensive:
            /* Don't take our natural if the enemy could be rushing or doing an all-in */
            Telemetry::setBoardValue("natural", "wait-defensive");
            break;

        case OurStrategy::FastExpansion:
            Telemetry::setBoardValue("natural", "take-fast-expo");
            takeNaturalExpansion(playList, prioProdGoals);
            return;

        default:
        {
            /* Expand as soon as our main army transitions to attack */
            auto mainArmyPlay = getPlay<AttackEnemyBase>(playList);
            if (!mainArmyPlay)
            {
                Telemetry::setBoardValue("natural", "no-attack-play");
                break;
            }

            Telemetry::setBoardValue("natural", "take");
            takeNaturalExpansion(playList, prioProdGoals);
            return;
        }
    }

    cancelNaturalExpansion(playList, prioProdGoals);
}

void PvT::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* For PvT dragoon range is important to get early, so get it first unless the enemy is rushing us */
    upgradeAtCount(
            prioProdGoals,
            BWAPI::UpgradeTypes::Singularity_Charge,
            BWAPI::UnitTypes::Protoss_Dragoon,
            ourStrategy == OurStrategy::AntiMarineRush ? 2 : 0);

    /* Basic infantry skill upgrades are queued when we have enough of them and are still building them */
    upgradeAtCount(prioProdGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);

    /* Cases where we want the upgrade as soon as we start building one of the units */
    upgradeWhenUnitCreated(prioProdGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitCreated(prioProdGoals,
                           BWAPI::UpgradeTypes::Gravitic_Drive,
                           BWAPI::UnitTypes::Protoss_Shuttle,
                           false,
                           UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) < 3);
    upgradeWhenUnitCreated(prioProdGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, true);
    upgradeWhenUnitCreated(prioProdGoals, BWAPI::UpgradeTypes::Khaydarin_Core, BWAPI::UnitTypes::Protoss_Arbiter, false);
    upgradeWhenUnitCreated(prioProdGoals, BWAPI::TechTypes::Stasis_Field, BWAPI::UnitTypes::Protoss_Arbiter, false);

    defaultGroundUpgrades(prioProdGoals);

    /* TODO: Air upgrades */
}

void PvT::handleDetection(std::vector<std::shared_ptr<PlayItem>> &playList, std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* The main army play will reactively request mobile detection when it sees a cloaked enemy unit */
    /* The logic here is to look ahead to make sure we already have detection available when we need it */

    /* Break out if we are already building an observer */
    if (UnitMgr::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0) return;

    /* Break out if we already have an observer in the main army */
    /* This means that all plays requiring an observer have one */
    auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
    if (mainArmyPlay && mainArmyPlay->getSquad() && !mainArmyPlay->getSquad()->getDetectors().empty()) return;

    auto buildObserver = [&](int priority = PRIORITY_NORMAL)
    {
        prioProdGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                          "SE-detection",
                                                          BWAPI::UnitTypes::Protoss_Observer,
                                                          1,
                                                          1);
    };

    /* Build an observer if the enemy has cloaked wraith tech */
    if (UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Control_Tower) ||
        PlayerInfo::hasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Cloaking_Field))
    {
        buildObserver(PRIORITY_SPECIALTEAMS);
        return;
    }

    /* In all other cases, wait until we are on two bases */
    if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) < 2) return;

    /* Get obs immediately if we've seen a spider mine */
    if (UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine))
    {
        buildObserver(PRIORITY_SPECIALTEAMS);
        return;
    }

    /* Get obs earlier if we've seen a vulture or tank, indicating mech play and likely spider mines */
    if (frameNow > 10000 && (UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) ||
                                                     UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) ||
                                                     UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture)))
    {
        buildObserver();
        return;
    }

    /* Otherwise start getting obs at frame 14000 */
    if (frameNow > 14000)
    {
        buildObserver();
        return;
    }
}
