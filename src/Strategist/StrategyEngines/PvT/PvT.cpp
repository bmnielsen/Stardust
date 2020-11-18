#include "StrategyEngines/PvT.h"

#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"
#include "Workers.h"
#include "Players.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyMain.h"
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
            switch (ourStrategy)
            {
                case OurStrategy::EarlyGameDefense:
                case OurStrategy::AntiMarineRush:
                case OurStrategy::Defensive:
                    setMainPlay<DefendMyMain>(mainArmyPlay);
                    break;
                case OurStrategy::FastExpansion:
                case OurStrategy::Normal:
                case OurStrategy::MidGame:
                {
                    // Transition from a defend squad when the vanguard cluster has 3 units
                    if (typeid(*mainArmyPlay) == typeid(DefendMyMain))
                    {
                        auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                        if (vanguard && vanguard->units.size() >= 3)
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
                        }
                    }

                    break;
                }
            }
        }
    }

    updateDefendBasePlays(plays);
    updateAttackExpansionPlays(plays);
    defaultExpansions(plays);
    scoutExpos(plays, 15000);
}

void PvT::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(prioritizedProductionGoals);

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

            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

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
        case OurStrategy::Normal:
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

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            // Try to keep at least two army units in production while taking our natural
            int higherPriorityCount = 2 - inProgressCount;
            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);

            break;
        }

        case OurStrategy::MidGame:
        {
            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            int higherPriorityCount = (Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) / 10) - inProgressCount;

            // Produce zealots if the enemy has a lot of tanks
            int enemyTanks = Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
                             Units::countEnemy(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
            if (enemyTanks > 4)
            {
                int desiredZealots = std::min(dragoonCount / 2, 5 + enemyTanks);
                if (desiredZealots > zealotCount)
                {
                    mainArmyProduction(prioritizedProductionGoals,
                                       BWAPI::UnitTypes::Protoss_Zealot,
                                       desiredZealots - zealotCount,
                                       higherPriorityCount);
                }
            }

            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);
            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Zealot, -1, higherPriorityCount);

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

        case OurStrategy::Normal:
        case OurStrategy::MidGame:
        {
            // In this case we want to expand when we consider it safe to do so: we have an attacking or containing army
            // that is close to the enemy base

            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyMain))
            {
                CherryVis::setBoardValue("natural", "no-attack-play");
                break;
            }

            auto squad = mainArmyPlay->getSquad();
            if (!squad || squad->getUnits().size() < 4)
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

            // Cluster should be at least 2/3 of the way to the target base
            int distToMain = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->center);
            if (dist * 2 > distToMain)
            {
                CherryVis::setBoardValue("natural", "vanguard-cluster-too-close");
                break;
            }

            // Cluster should not be moving or fleeing
            // In other words, we want the cluster to be in some kind of stable attack or contain state
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
                || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                    && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee))
            {
                // We don't cancel a queued expansion in this case
                CherryVis::setBoardValue("natural", "vanguard-cluster-not-attacking");
                return;
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
    // For PvT dragoon range is important to get early, so start it as soon as we have a finished core unless the enemy is rushing us
    if (ourStrategy != OurStrategy::AntiMarineRush)
    {
        upgradeWhenUnitStarted(prioritizedProductionGoals,
                               BWAPI::UpgradeTypes::Singularity_Charge,
                               BWAPI::UnitTypes::Protoss_Cybernetics_Core,
                               false,
                               PRIORITY_MAINARMYBASEPRODUCTION);
    }
    else
    {
        upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);
    }

    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, true);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier);

    defaultGroundUpgrades(prioritizedProductionGoals);

    // TODO: Air upgrades
}

void PvT::handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
    // The logic here is to look ahead to make sure we already have detection available when we need it

    // Break out if we already have an observer
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0 || Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0)
    {
        return;
    }

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

    // Never build obs on one base
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) < 2 &&
        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) < 2 || BWAPI::Broodwar->getFrameCount() < 10000))
    {
        return;
    }

    // Build an observer if we have seen a spider mine
    if (Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine))
    {
        buildObserver();
        return;
    }

    // Build an observer when we are on three bases
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 2 ||
        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 2 && BWAPI::Broodwar->getFrameCount() > 15000))
    {
        buildObserver();
        return;
    }
}
