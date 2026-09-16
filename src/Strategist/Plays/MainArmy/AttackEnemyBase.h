#pragma once

#include "MainArmyPlay.h"
#include "Squads/AttackBaseSquad.h"

class AttackEnemyBase : public MainArmyPlay
{
public:
    Base *base;

    explicit AttackEnemyBase(Base *base);

    [[nodiscard]] bool isDefensive() const override { return false; }

    [[nodiscard]] std::shared_ptr<Squad> getSquad() const override { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
