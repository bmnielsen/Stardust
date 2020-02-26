#include "AttackMainBase.h"

#include "General.h"
#include "Map.h"
#include "Units.h"
#include "MopUp.h"

AttackMainBase::AttackMainBase(Base *base) : base(base), squad(std::make_shared<AttackBaseSquad>(base))
{
    General::addSquad(squad);
}

void AttackMainBase::update()
{
    // When the enemy's main base changes, either transition this play to attack the new main base,
    // or to a mop up play if the enemy no longer has a main
    auto enemyMain = Map::getEnemyMain();
    if (enemyMain == base) return;

    if (enemyMain)
    {
        status.transitionTo = std::make_shared<AttackMainBase>(enemyMain);
    }
    else
    {
        status.transitionTo = std::make_shared<MopUp>();
    }
}

void AttackMainBase::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    MainArmyProduction(prioritizedProductionGoals);
}

void AttackMainBase::MainArmyProduction(std::map<int, std::vector<ProductionGoal> > & prioritizedProductionGoals)
{
    // TODO
    int percentZealots = 0;

    int currentZealots = Units::countAll(BWAPI::UnitTypes::Protoss_Zealot);
    int currentDragoons = Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon);

    int currentZealotRatio = (100 * currentZealots) / std::max(1, currentZealots + currentDragoons);

    if (currentZealotRatio >= percentZealots)
    {
        prioritizedProductionGoals[30].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Dragoon, -1, -1);
    }
    prioritizedProductionGoals[30].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Zealot, -1, -1);
}