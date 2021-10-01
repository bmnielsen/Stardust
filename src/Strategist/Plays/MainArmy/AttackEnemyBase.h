#pragma once

#include "MainArmyPlay.h"
#include "Squads/AttackBaseSquad.h"

class AttackEnemyBase : public MainArmyPlay
{
public:
    Base *base;

    explicit AttackEnemyBase(Base *base);

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
