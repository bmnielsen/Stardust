#pragma once

#include "MainArmyPlay.h"
#include "Squads/MopUpSquad.h"

class MopUp : public MainArmyPlay
{
public:
    MopUp();

    [[nodiscard]] bool isDefensive() const override { return false; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<MopUpSquad> squad;
};
