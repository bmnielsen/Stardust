#include "AttackBase.h"

#include "General.h"

AttackBase::AttackBase(Base *base) : squad(std::make_shared<AttackBaseSquad>(base))
{
    General::addSquad(squad);
}