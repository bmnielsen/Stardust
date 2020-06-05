#include "StrategyEngines/MapSpecific/PlasmaStrategyEngine.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyMain.h"
#include "Plays/MainArmy/MopUp.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"

#include "Map.h"
#include "Builder.h"

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

void PlasmaStrategyEngine::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<DefendMyMain>());
}

void PlasmaStrategyEngine::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    // Transition from a defend squad when the vanguard cluster has 3 units
    auto mainArmyPlay = getMainArmyPlay(plays);
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

    UpdateDefendBasePlays(plays);
    defaultExpansions(plays);
}

void PlasmaStrategyEngine::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                                            std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                            std::vector<std::pair<int, int>> &mineralReservations)
{
    handleNaturalExpansion(plays, prioritizedProductionGoals);

    auto mainArmyPlay = getMainArmyPlay(plays);
    auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
    auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

    int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
    int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

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
    if (zealotCount < 2)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Zealot,
                                                                   1,
                                                                   1);
    }

    prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                               BWAPI::UnitTypes::Protoss_Dragoon,
                                                               -1,
                                                               -1);

    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, true);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier);

    defaultGroundUpgrades(prioritizedProductionGoals);
}

void PlasmaStrategyEngine::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                                  std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been (or is being) taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1) return;
    if (Builder::isPendingHere(natural->getTilePosition())) return;

    // Expand as soon as our army is attacking
    auto mainArmyPlay = getMainArmyPlay(plays);
    if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyMain)) return;

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           natural->getTilePosition(),
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0, 0);
    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Nexus, buildLocation);
}
