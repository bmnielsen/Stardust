#include "StrategyEngines/PvZ.h"

#include "Units.h"
#include "Map.h"
#include "Builder.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyMain.h"
#include "Plays/MainArmy/MopUp.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;

    template<class T, class ...Args>
    void setMainPlay(MainArmyPlay *current, Args&& ...args)
    {
        if (typeid(*current) == typeid(T)) return;
        current->status.transitionTo = std::make_shared<T>(std::forward<Args>(args)...);
    }
}

void PvZ::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
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

    // Ensure we have the correct main army play
    auto mainArmyPlay = getMainArmyPlay(plays);
    if (mainArmyPlay)
    {
        if (enemyStrategy == ZergStrategy::GasSteal)
        {
            setMainPlay<DefendMyMain>(mainArmyPlay);
        }
        else
        {
            switch (ourStrategy)
            {
                case OurStrategy::EarlyGameDefense:
                case OurStrategy::AntiAllIn:
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

    defaultExpansions(plays);
}

void PvZ::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(prioritizedProductionGoals);

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
            auto mainArmyPlay = getMainArmyPlay(plays);
            auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
            auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

            int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];

            // Get six zealots before dragoons
            int zealotsRequired = 6 - zealotCount;

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
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
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
                auto mainArmyPlay = getMainArmyPlay(plays);
                auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
                auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

                int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                                completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];
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
            // Build at least four zealots then transition into dragoons
            auto mainArmyPlay = getMainArmyPlay(plays);
            auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
            auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

            int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                            completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];
            if (unitCount < 4)
            {
                prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                              BWAPI::UnitTypes::Protoss_Zealot,
                                                                              4 - unitCount,
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
            // Build at least two zealots then transition into dragoons
            auto mainArmyPlay = getMainArmyPlay(plays);
            auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
            auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

            int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                            completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];
            if (unitCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                              BWAPI::UnitTypes::Protoss_Zealot,
                                                                              2 - unitCount,
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
    }
}

void PvZ::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been (or is being) taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1) return;
    if (Builder::isPendingHere(natural->getTilePosition())) return;

    // Return if our strategy doesn't warrant taking the natural now, otherwise fall through to queue it
    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiAllIn:
        case OurStrategy::Defensive:
            // Don't take our natural if the enemy could be rushing or doing an all-in
            return;

        case OurStrategy::FastExpansion:
            // We want to expand now, so fall through
            break;

        case OurStrategy::Normal:
        case OurStrategy::MidGame:
        {
            // In this case we want to expand when we consider it safe to do so: we have an attacking or containing army
            // that is close to the enemy base

            auto mainArmyPlay = getMainArmyPlay(plays);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyMain)) return;

            auto squad = mainArmyPlay->getSquad();
            if (!squad || squad->getUnits().size() < 5) return;

            int dist;
            auto vanguardCluster = squad->vanguardCluster(&dist);
            if (!vanguardCluster) return;

            // Cluster should be at least 2/3 of the way to the target base
            int distToMain = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->center);
            if (dist * 2 > distToMain) return;

            // Cluster should not be moving or fleeing
            // In other words, we want the cluster to be in some kind of stable attack or contain state
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
                || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                    && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee))
            {
                return;
            }
            break;
        }
    }

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           natural->getTilePosition(),
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0, 0);
    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Nexus, buildLocation);
}

void PvZ::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, true);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier);

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

    // TODO: Use scouting information

    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1)
    {
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    }
}
