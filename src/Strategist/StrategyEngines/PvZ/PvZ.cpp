#include "StrategyEngines/PvZ.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
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

void PvZ::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<EjectEnemyScout>());
    plays.emplace_back(std::make_shared<DefendMyMain>());
}

void PvZ::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto newEnemyStrategy = recognizeEnemyStrategy();
    auto newStrategy = chooseOurStrategy(newEnemyStrategy, plays);

    if (enemyStrategy != newEnemyStrategy)
    {
        Log::Get() << "Enemy strategy changed from " << ZergStrategyNames[enemyStrategy] << " to " << ZergStrategyNames[newEnemyStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Enemy strategy changed from " << ZergStrategyNames[enemyStrategy] << " to " << ZergStrategyNames[newEnemyStrategy];
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
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        auto canTransitionToAttack = [&](int requiredUnitCount, bool requireDragoon)
        {
            if (!mainArmyPlay) return false;

            auto vanguardCluster = mainArmyPlay->getSquad()->vanguardCluster();
            if (!vanguardCluster || !vanguardCluster->vanguard) return false;

            bool hasDragoon = false;
            int count = 0;
            for (const auto &unit : vanguardCluster->units)
            {
                if (unit->getDistance(vanguardCluster->vanguard) < 200)
                {
                    if (unit->type == BWAPI::UnitTypes::Protoss_Dragoon) hasDragoon = true;
                    count++;
                }
            }

            return (count >= requiredUnitCount && (!requireDragoon || hasDragoon));
        };

        switch (ourStrategy)
        {
            case OurStrategy::EarlyGameDefense:
            case OurStrategy::AntiAllIn:
                defendOurMain = true;
                break;
            default:
            {
                if (!mainArmyPlay)
                {
                    defendOurMain = true;
                    break;
                }

                defendOurMain = false;

                // Transition from defense when appropriate
                if (typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    if (ourStrategy == OurStrategy::FastExpansion)
                    {
                        defendOurMain = !canTransitionToAttack(3, false);
                    }
                    else
                    {
                        defendOurMain = !canTransitionToAttack(4, true);
                    }
                }

                break;
            }
        }
    }

    updateAttackPlays(plays, defendOurMain);

    // Set the worker scout to monitor the enemy choke once the pool is finished
    if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted)
    {
        auto play = getPlay<EarlyGameWorkerScout>(plays);
        if (play)
        {
            auto pools = Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Spawning_Pool);
            if (!pools.empty() && (*pools.begin())->completed)
            {
                play->monitorEnemyChoke();
            }
        }
    }

    updateDefendBasePlays(plays);
    defaultExpansions(plays);
    scoutExpos(plays, 10000);
}

void PvZ::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
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

    int inProgressCount = Units::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dragoon)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dark_Templar);

    handleGasStealProduction(prioritizedProductionGoals, zealotCount);

    // Main army production
    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        {
            // We start with two-gate zealots until we have more scouting information
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       2);
            break;
        }
        case OurStrategy::AntiAllIn:
        {
            // Get at least six zealots before dragoons, more if our choke is hard to defend
            int desiredZealots = 6;
            auto mainChoke = Map::getMyMainChoke();
            if (mainChoke && !mainChoke->isNarrowChoke)
            {
                desiredZealots = 10;
            }

            // Also bump up the number of zealots if the enemy has a lot of lings
            desiredZealots = std::max(desiredZealots, 2 + Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) / 3);

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

            // Upgrade goon range at 2 dragoons
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            break;
        }

        case OurStrategy::FastExpansion:
        {
            // We've scouted a non-threatening opening, so generally skip zealots and go straight for dragoons
            // Build a couple of zealots though if we have seen zerglings on the way and have nothing to defend with
            if (Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 0)
            {
                int unitCount = zealotCount + dragoonCount;
                if (unitCount < 2)
                {
                    prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                                  BWAPI::UnitTypes::Protoss_Zealot,
                                                                                  2 - unitCount,
                                                                                  2);
                }
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

        case OurStrategy::Defensive:
        {
            // Scale our desired zealots based on enemy ling count, with a minimum of 3
            int desiredZealots = std::max(3, 1 + (Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) + 2) / 3);
            int unitCount = zealotCount + dragoonCount;
            if (unitCount < desiredZealots)
            {
                prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                              BWAPI::UnitTypes::Protoss_Zealot,
                                                                              desiredZealots - unitCount,
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

            handleUpgrades(prioritizedProductionGoals);

            break;
        }

        case OurStrategy::Normal:
        {
            // Build at least three zealots then transition into dragoons
            int unitCount = zealotCount + dragoonCount;
            if (unitCount < 3)
            {
                prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                              BWAPI::UnitTypes::Protoss_Zealot,
                                                                              3 - unitCount,
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

            handleUpgrades(prioritizedProductionGoals);

            break;
        }
        case OurStrategy::MidGame:
        {
            // TODO: Higher-tech units

            int higherPriorityCount = (Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) / 10) - inProgressCount;

            // Keep some zealots in the mix if the opponent has a lot of lings
            int requiredZealots = 0;
            if (Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 6)
            {
                requiredZealots = std::min(10, Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) / 2) - zealotCount;
            }

            if (requiredZealots > 0)
            {
                mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Zealot, requiredZealots, higherPriorityCount);
            }

            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);
            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Zealot, -1, higherPriorityCount);

            handleUpgrades(prioritizedProductionGoals);

            break;
        }
    }
}

void PvZ::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1)
    {
        CherryVis::setBoardValue("natural", "complete");
        return;
    }

    // If we have a backdoor natural, expand when our third goon is being produced or we have lots of money
    if (Map::mapSpecificOverride()->hasBackdoorNatural())
    {
        if (BWAPI::Broodwar->self()->minerals() > 450 ||
            Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon) > 2)
        {
            CherryVis::setBoardValue("natural", "take-backdoor");

            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
    }

    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiAllIn:
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
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyBase))
            {
                CherryVis::setBoardValue("natural", "no-attack-play");
                break;
            }

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

void PvZ::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, false, true);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, true);

    defaultGroundUpgrades(prioritizedProductionGoals);

    // TODO: Air upgrades
}

void PvZ::handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
    // The logic here is to look ahead to make sure we already have detection available when we need it

    // Break out if we already have an observer
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0 || Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0)
    {
        return;
    }

    // Build an observer when we are on two gas or the enemy has lurker tech
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1 ||
        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1 && BWAPI::Broodwar->getFrameCount() > 10000) ||
        Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Lurker_Egg) ||
        Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Lurker) ||
        Players::hasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Lurker_Aspect))
    {
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    }
}
