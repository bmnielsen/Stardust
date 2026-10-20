#pragma once

#include "MainArmyPlay.h"
#include "Squads/AttackBaseSquad.h"

class AttackEnemyBase : public MainArmyPlay
{
public:
    Base *baseLoc;

    explicit AttackEnemyBase(Base *baseLoc);

    [[nodiscard]] bool isDefensive() const override { return false; }

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    /* window_push (2026-10-08): puts the squad into all-in mode - it bypasses
     * the whole fight-or-flight decision and presses the attack continuously */
    void setAllIn(bool v) { squad->ignoreCombatSim = v; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
