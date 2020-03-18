#include "RallyArmy.h"

#include "Map.h"
#include "General.h"
#include "AttackMainBase.h"

RallyArmy::RallyArmy() : squad(std::make_shared<DefendBaseSquad>(Map::getMyMain()))
{
    General::addSquad(squad);
}

void RallyArmy::update()
{
    // This play is obsolete as soon as we know where the enemy's main base is
    auto enemyMain = Map::getEnemyStartingMain();
    if (enemyMain)
    {
        status.transitionTo = std::make_shared<AttackMainBase>(enemyMain);
        return;
    }

    MainArmyPlay::update();
}
