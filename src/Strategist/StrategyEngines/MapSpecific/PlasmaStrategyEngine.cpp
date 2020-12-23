#include "StrategyEngines/MapSpecific/PlasmaStrategyEngine.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"

#include "Map.h"
#include "Builder.h"
#include "Opponent.h"
#include "Units.h"

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;
}

void PlasmaStrategyEngine::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<DefendMyMain>());
}

void PlasmaStrategyEngine::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto newEnemyStrategy = recognizeEnemyStrategy();

    if (enemyStrategy != newEnemyStrategy)
    {
        Log::Get() << "Enemy strategy changed from " << EnemyStrategyNames[enemyStrategy] << " to " << EnemyStrategyNames[newEnemyStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Enemy strategy changed from " << EnemyStrategyNames[enemyStrategy] << " to " << EnemyStrategyNames[newEnemyStrategy];
#endif

        enemyStrategy = newEnemyStrategy;
    }

    // On Plasma we don't currently use scouting information since we expect enemies to do weird stuff
    // So recall the worker scout as soon as we know the enemy main and race
    if (Map::getEnemyStartingMain() && !Opponent::isUnknownRace())
    {
        for (auto &play : plays)
        {
            if (auto workerScoutPlay = std::dynamic_pointer_cast<EarlyGameWorkerScout>(play))
            {
                workerScoutPlay->status.complete = true;
                break;
            }
        }
    }

    // Determine whether to defend our main
    bool defendOurMain =
            hasEnemyStolenOurGas() ||
            enemyStrategy == EnemyStrategy::Unknown ||
            enemyStrategy == EnemyStrategy::ProxyRush;

    updateAttackPlays(plays, defendOurMain);
    updateDefendBasePlays(plays);
    defaultExpansions(plays);
    scoutExpos(plays, 15000);
}

void PlasmaStrategyEngine::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                                            std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                            std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
    handleNaturalExpansion(plays, prioritizedProductionGoals);

    switch (enemyStrategy)
    {
        case EnemyStrategy::Unknown:
        {
            // Get two zealots before goons
            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
            auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

            int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];

            if (zealotCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           2 - zealotCount,
                                                                           2);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);

            break;
        }
        case EnemyStrategy::ProxyRush:
        {
            // Get four zealots before starting the dragoon transition
            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
            auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

            int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
            int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

            int zealotsRequired = 4 - zealotCount;

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

            // If the dragoon transition is just beginning, only order one so we keep producing zealots
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       dragoonCount == 0 ? 1 : -1,
                                                                       -1);

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);
            break;
        }
        case EnemyStrategy::Normal:
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);
            break;
    }

    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, false, true);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, true);

    defaultGroundUpgrades(prioritizedProductionGoals);

    // Get an observer when on 2 or more gas
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1 &&
        Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) == 0 &&
        Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) == 0)
    {
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    }
}

void PlasmaStrategyEngine::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                                  std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been (or is being) taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1) return;
    if (Builder::isPendingHere(natural->getTilePosition())) return;

    // Expand as soon as our army is attacking
    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
    if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyBase)) return;

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           natural->getTilePosition(),
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0, 0);
    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Nexus, buildLocation);
}
