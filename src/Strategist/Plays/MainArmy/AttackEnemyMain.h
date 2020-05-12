#pragma once

#include "MainArmyPlay.h"
#include "Squads/AttackBaseSquad.h"

class AttackEnemyMain : public MainArmyPlay
{
public:
    explicit AttackEnemyMain(Base *base);

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

private:
    Base *base;
    std::shared_ptr<AttackBaseSquad> squad;
};
