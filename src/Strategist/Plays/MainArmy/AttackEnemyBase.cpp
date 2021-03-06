#include "AttackEnemyBase.h"

#include "General.h"

AttackEnemyBase::AttackEnemyBase(Base *base)
        : MainArmyPlay((std::ostringstream() << "Attack base @ " << base->getTilePosition()).str())
        , base(base)
        , squad(std::make_shared<AttackBaseSquad>(base))
{
    General::addSquad(squad);
}
