#include "MopUp.h"

#include "General.h"
#include "Map.h"
#include "AttackMainBase.h"

MopUp::MopUp() : squad(std::make_shared<MopUpSquad>())
{
    General::addSquad(squad);
}

void MopUp::update()
{
    // If we scout a new enemy base, transition this play to attack it instead
    auto enemyMain = Map::getEnemyMain();
    if (enemyMain)
    {
        status.transitionTo = std::make_shared<AttackMainBase>(enemyMain);
    }
}
