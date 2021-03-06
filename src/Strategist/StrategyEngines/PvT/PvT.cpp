#include "StrategyEngines/PvT.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Workers.h"
#include "Players.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/MainArmy/MopUp.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Plays/Scouting/EjectEnemyScout.h"

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
}

void PvT::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(plays, prioritizedProductionGoals);

    // Temporary hack to set the number of gas workers needed until the producer can do it
    auto setGasGathering = [](bool gather)
    {
        int current = Workers::desiredGasWorkers();
        int delta;
        if (gather)
        {
            delta = Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) * 3 - current;
        }
        else
        {
            delta = -std::min(current, Workers::availableMineralAssignments());
        }
        Workers::addDesiredGasWorkers(delta);
    };

    // Default to gather gas - we will only set to false later if we are being rushed
    setGasGathering(true);

    // Main army production
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
        int higherPriorityCount = (Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) / 10) - inProgressCount;

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
            // Start with dragoons
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 1);

            break;
        }
        case OurStrategy::AntiMarineRush:
        {
            // If we have no dragoons yet, get four zealots
            // Otherwise keep two zealots while pumping dragoons
            int desiredZealots = (dragoonCount == 0 ? 4 : 2) + std::max(0, (Units::countEnemy(BWAPI::UnitTypes::Terran_Marine) - 6) / 3);
            int zealotsRequired = desiredZealots - zealotCount;

            // Get two zealots at highest priority
            if (dragoonCount == 0 && zealotCount < 2)
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

            // Upgrade goon range at 2 dragoons
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            break;
        }

        case OurStrategy::FastExpansion:
        case OurStrategy::Defensive:
        case OurStrategy::NormalOpening:
        {
            // If any zealots are in production, and we don't have an emergency production goal, cancel them
            // This happens when the enemy strategy was misrecognized as a rush
            if (Units::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot) > 0)
            {
                bool hasEmergencyZealotProduction = false;
                for (const auto &productionGoal : prioritizedProductionGoals[PRIORITY_EMERGENCY])
                {
                    if (auto unitProductionGoal = std::get_if<UnitProductionGoal>(&productionGoal))
                    {
                        if (unitProductionGoal->unitType() == BWAPI::UnitTypes::Protoss_Zealot)
                        {
                            hasEmergencyZealotProduction = true;
                            break;
                        }
                    }
                }

                if (!hasEmergencyZealotProduction)
                {
                    for (const auto &gateway : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Gateway))
                    {
                        if (gateway->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Cancel_Train) continue;
                        if (gateway->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Cancel_Train_Slot) continue;

                        auto trainingQueue = gateway->bwapiUnit->getTrainingQueue();
                        if (trainingQueue.empty()) continue;
                        if (*trainingQueue.begin() != BWAPI::UnitTypes::Protoss_Zealot) continue;

                        Log::Get() << "Cancelling zealot production from gateway @ " << gateway->getTilePosition();
                        gateway->bwapiUnit->cancelTrain(0);
                    }
                }
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
            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyBase))
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
