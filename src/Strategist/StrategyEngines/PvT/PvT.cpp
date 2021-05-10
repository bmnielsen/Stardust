#include "StrategyEngines/PvT.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Workers.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Plays/Scouting/EjectEnemyScout.h"
#include "Plays/SpecialTeams/CarrierHarass.h"
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

void PvT::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<EjectEnemyScout>());
    plays.emplace_back(std::make_shared<DefendMyMain>());

    // AIST S4 vs. Human match - do a fast expansion on Fighting Spirit and Circuit Breaker
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
}

void PvT::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto newEnemyStrategy = recognizeEnemyStrategy();
    auto newStrategy = chooseOurStrategy(newEnemyStrategy, plays);

    if (enemyStrategy != newEnemyStrategy)
    {
        Log::Get() << "Enemy strategy changed from " << TerranStrategyNames[enemyStrategy] << " to " << TerranStrategyNames[newEnemyStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Enemy strategy changed from " << TerranStrategyNames[enemyStrategy] << " to " << TerranStrategyNames[newEnemyStrategy];
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

        // AIST S4 vs. Human match - When we transition from early-game defense to normal on specific maps, do an elevator rush
        /*
        if (ourStrategy == OurStrategy::EarlyGameDefense && newStrategy == OurStrategy::NormalOpening &&
            (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b" ||
             BWAPI::Broodwar->mapHash() == "e39c1c81740a97a733d227e238bd11df734eaf96" ||
             BWAPI::Broodwar->mapHash() == "6f8da3c3cc8d08d9cf882700efa049280aedca8c" ||
             BWAPI::Broodwar->mapHash() == "fe25d8b79495870ac1981c2dfee9368f543321e3" ||
             BWAPI::Broodwar->mapHash() == "d9757c0adcfd61386dff8fe3e493e9e8ef9b45e3" ||
             BWAPI::Broodwar->mapHash() == "ecb9c70c5594a5c6882baaf4857a61824fba0cfa"))
        {
            //plays.insert(plays.begin(), std::make_shared<ElevatorRush>());
        }
        */

        ourStrategy = newStrategy;
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
            case OurStrategy::EarlyGameDefense:
            case OurStrategy::AntiMarineRush:
            case OurStrategy::Defensive:
                defendOurMain = true;
                break;
            default:
            {
                auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
                if (!mainArmyPlay)
                {
                    defendOurMain = true;
                    break;
                }

                defendOurMain = false;

                // Transition from a defend squad when the vanguard cluster has 2 units
                if (typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                    defendOurMain = (!vanguard || vanguard->units.size() < 2);
                }

                break;
            }
        }
    }

    updateAttackPlays(plays, defendOurMain);
    updateDefendBasePlays(plays);
    defaultExpansions(plays);
    scoutExpos(plays, 15000);

    // AIST S4 vs. Human match - Harass enemy bases with carriers after we have our first two arbiters out
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

void PvT::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(plays, prioritizedProductionGoals);

    if (handleIslandExpansionProduction(plays, prioritizedProductionGoals)) return;

    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
    auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
    auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

    int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
    int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

    int inProgressCount = Units::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dragoon)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dark_Templar);

    handleGasStealProduction(prioritizedProductionGoals, zealotCount);

    auto midAndLateGameMainArmyProduction = [&]()
    {
        // Keep a baseline production until we have a large army
        int higherPriorityCount = 0;
        if ((zealotCount + dragoonCount) < 25)
        {
            // Roughly two units per mining base
            higherPriorityCount = (Workers::mineralWorkers() / 8) - inProgressCount;
        }

        // Counter tanks with speedlots once the enemy has at least four
        int enemyTanks = Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                         Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
        if (enemyTanks > 3)
        {
            int desiredZealots = std::min(dragoonCount, enemyTanks * 2);
            if (desiredZealots > zealotCount)
            {
                mainArmyProduction(prioritizedProductionGoals,
                                   BWAPI::UnitTypes::Protoss_Zealot,
                                   desiredZealots - zealotCount,
                                   higherPriorityCount);
            }

            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 0);
        }

        mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);
        mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Zealot, -1, higherPriorityCount);
    };

    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        {
            oneGateCoreOpening(prioritizedProductionGoals, dragoonCount, zealotCount, 1);
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);

            break;
        }
        case OurStrategy::AntiMarineRush:
        {
            // If we haven't reached a "critical mass" of dragoons yet, get at least four zealots
            // Otherwise keep two zealots while pumping dragoons
            int desiredZealots = (dragoonCount < 3 ? 4 : 2) + std::max(0, (Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) - 6) / 3);
            int zealotsRequired = desiredZealots - zealotCount;

            handleAntiRushProduction(prioritizedProductionGoals, dragoonCount, zealotCount, zealotsRequired);

            break;
        }

        case OurStrategy::FastExpansion:
        case OurStrategy::Defensive:
        case OurStrategy::NormalOpening:
        {
            // If any zealots are in production, and we don't have an emergency production goal, cancel them
            if (Units::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot) > 0)
            {
                cancelTrainingUnits(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Zealot);
            }

            // Try to keep at least two army units in production while taking our natural
            int higherPriorityCount = 2 - inProgressCount;
            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }

        case OurStrategy::MidGame:
        {
            // Have some DTs harrassing until the enemy has vessels
            // TODO: Implement DT tactics and re-enable
            /*
            int dtCount = Units::countAll(BWAPI::UnitTypes::Protoss_Dark_Templar);
            if (dtCount < 4 &&
                !Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Vessel) &&
                !Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Facility))
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                                         1,
                                                                         1);
            }
             */

            // Build arbiters on three completed nexuses
            int arbiterCount = Units::countAll(BWAPI::UnitTypes::Protoss_Arbiter);
            if (arbiterCount < 2 && Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 2)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         BWAPI::UnitTypes::Protoss_Arbiter,
                                                                         1,
                                                                         1);
            }

            midAndLateGameMainArmyProduction();

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }

        case OurStrategy::LateGameCarriers:
        {
            // Produce unlimited carriers off of two stargates, at slightly higher priority than main army
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     BWAPI::UnitTypes::Protoss_Carrier,
                                                                     -1,
                                                                     2);

            midAndLateGameMainArmyProduction();

            // Besides default upgrades, upgrade air weapons immediately
            int weaponLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons);
            if (weaponLevel < 3)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                         BWAPI::UpgradeTypes::Protoss_Air_Weapons,
                                                                         weaponLevel + 1,
                                                                         1);
            }
            handleUpgrades(prioritizedProductionGoals);

            break;
        }
    }
}

void PvT::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1)
    {
        CherryVis::setBoardValue("natural", "complete");
        return;
    }

    // Don't expand while we are setting up an elevator rush
    auto elevatorRush = getPlay<ElevatorRush>(plays);
    if (elevatorRush && elevatorRush->getSquad()->empty())
    {
        CherryVis::setBoardValue("natural", "wait-elevator");
        return;
    }

    // If we have a backdoor natural, expand when our second goon is being produced or we have lots of money
    if (Map::mapSpecificOverride()->hasBackdoorNatural())
    {
        if (BWAPI::Broodwar->self()->minerals() > 450 ||
            Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon) > 1)
        {
            CherryVis::setBoardValue("natural", "take-backdoor");

            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
    }

    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiMarineRush:
        case OurStrategy::Defensive:
            // Don't take our natural if the enemy could be rushing or doing an all-in
            CherryVis::setBoardValue("natural", "wait-defensive");
            break;

        case OurStrategy::FastExpansion:
            CherryVis::setBoardValue("natural", "take-fast-expo");
            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;

        default:
        {
            // Expand as soon as our main army transitions to attack
            auto mainArmyPlay = getPlay<AttackEnemyBase>(plays);
            if (!mainArmyPlay)
            {
                CherryVis::setBoardValue("natural", "no-attack-play");
                break;
            }

            CherryVis::setBoardValue("natural", "take");
            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
    }

    cancelNaturalExpansion(plays, prioritizedProductionGoals);
}

void PvT::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // For PvT dragoon range is important to get early, so get it first unless the enemy is rushing us
    upgradeAtCount(
            prioritizedProductionGoals,
            BWAPI::UpgradeTypes::Singularity_Charge,
            BWAPI::UnitTypes::Protoss_Dragoon,
            ourStrategy == OurStrategy::AntiMarineRush ? 2 : 0);

    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, false, true);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, true);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::TechTypes::Stasis_Field, BWAPI::UnitTypes::Protoss_Arbiter, false);

    defaultGroundUpgrades(prioritizedProductionGoals);

    // TODO: Air upgrades
}

void PvT::handleDetection(std::vector<std::shared_ptr<Play>> &plays, std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
    // The logic here is to look ahead to make sure we already have detection available when we need it

    // Break out if we are already building an observer
    if (Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0) return;

    // Break out if we already have an observer in the main army
    // This means that all plays requiring an observer have one
    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
    if (mainArmyPlay && mainArmyPlay->getSquad() && !mainArmyPlay->getSquad()->getDetectors().empty()) return;

    auto buildObserver = [&]()
    {
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    };

    // Build an observer if the enemy has cloaked wraith tech
    if (Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Control_Tower) ||
        Players::hasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Cloaking_Field))
    {
        buildObserver();
        return;
    }

    // In all other cases, wait until we are on two bases
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) < 2) return;

    // Get obs immediately if we've seen a spider mine
    if (Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine))
    {
        buildObserver();
        return;
    }

    // Get obs earlier if we've seen a vulture or tank, indicating mech play and likely spider mines
    if (BWAPI::Broodwar->getFrameCount() > 10000 && (Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) ||
                                                     Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) ||
                                                     Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture)))
    {
        buildObserver();
        return;
    }

    // Otherwise start getting obs at frame 14000
    if (BWAPI::Broodwar->getFrameCount() > 14000)
    {
        buildObserver();
        return;
    }
}
