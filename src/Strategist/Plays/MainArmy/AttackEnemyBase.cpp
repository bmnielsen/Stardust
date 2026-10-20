#include "AttackEnemyBase.h"

#include "General.h"

AttackEnemyBase::AttackEnemyBase(Base *baseLoc)
        : MainArmyPlay((std::ostringstream() << "Attack base @ " << baseLoc->getTilePosition()).str())
        , baseLoc(baseLoc)
        , squad(std::make_shared<AttackBaseSquad>(baseLoc))
{
    Marshal::addSquad(squad);
}
