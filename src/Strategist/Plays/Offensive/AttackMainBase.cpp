#include "AttackMainBase.h"

#include "General.h"
#include "Map.h"
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
    if (!enemyMain)
    {
        status.transitionTo = std::make_shared<MopUp>();
        return;
    }
    if (enemyMain != base)
    {
        status.transitionTo = std::make_shared<AttackMainBase>(enemyMain);
        return;
    }

    MainArmyPlay::update();
}
