#pragma once

#include "MainArmyPlay.h"
#include "Squads/DefendBaseSquad.h"

class RallyArmy : public MainArmyPlay
{
public:
    RallyArmy();

    [[nodiscard]] const char *label() const override { return "RallyArmy"; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

private:
    std::shared_ptr<DefendBaseSquad> squad;
};
