#pragma once

#include "MainArmyPlay.h"
#include "Squads/AttackBaseSquad.h"

class AttackMainBase : public MainArmyPlay
{
public:
    explicit AttackMainBase(Base *base);

    [[nodiscard]] const char *label() const override { return "AttackMainBase"; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

private:
    Base *base;
    std::shared_ptr<AttackBaseSquad> squad;
};
