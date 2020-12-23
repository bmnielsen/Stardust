#pragma once

#include "MainArmyPlay.h"
#include "Squads/MopUpSquad.h"

class MopUp : public MainArmyPlay
{
public:
    MopUp();

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<MopUpSquad> squad;
};
