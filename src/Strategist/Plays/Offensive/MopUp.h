#pragma once

#include "Play.h"
#include "Squads/MopUpSquad.h"

class MopUp : public Play
{
public:
    MopUp();

    const char *label() const override { return "MopUp"; }

    bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

private:
    std::shared_ptr<MopUpSquad> squad;
};
