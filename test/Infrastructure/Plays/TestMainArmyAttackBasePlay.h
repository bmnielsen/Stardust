#pragma once

#include "Plays/MainArmy/MainArmyPlay.h"
#include "General.h"
#include "Squads/AttackBaseSquad.h"

// Differs from the normal attack base play in that it will never change to another base
class TestMainArmyAttackBasePlay : public MainArmyPlay
{
public:
    explicit TestMainArmyAttackBasePlay(Base *base, bool ignoreCombatSim = false)
            : MainArmyPlay((std::ostringstream() << "Test main army attack base @ " << base->getTilePosition()).str())
            , squad(std::make_shared<AttackBaseSquad>(base))
    {
        squad->ignoreCombatSim = ignoreCombatSim;
        General::addSquad(squad);
    };

    [[nodiscard]] bool isDefensive() const override { return false; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
