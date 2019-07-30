#include "RallyArmy.h"

#include "Map.h"
#include "General.h"
#include "AttackBase.h"

RallyArmy::RallyArmy() : squad(std::make_shared<DefendBaseSquad>(Map::getMyMain()))
{
    General::addSquad(squad);
}

void RallyArmy::update()
{
    auto enemyMain = Map::getEnemyMain();
    if (enemyMain)
    {
        // Transition to attack the enemy main when we know where it is
        status.transitionTo = std::make_shared<AttackBase>(enemyMain);
    }
}