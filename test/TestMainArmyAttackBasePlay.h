#pragma once

#include "Plays/Offensive/MainArmyPlay.h"
#include "General.h"
#include "Squads/AttackBaseSquad.h"

// Differs from the normal attack base play in that it will never change to another base
class TestMainArmyAttackBasePlay : public MainArmyPlay
{
public:
    explicit TestMainArmyAttackBasePlay(Base *base) : squad(std::make_shared<AttackBaseSquad>(base)) { General::addSquad(squad); };

    [[nodiscard]] const char *label() const override { return "TestMainArmyAttackBasePlay"; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
