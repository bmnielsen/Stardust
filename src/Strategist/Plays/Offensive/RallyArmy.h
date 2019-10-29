#pragma once

#include "Play.h"
#include "Squads/DefendBaseSquad.h"

class RallyArmy : public Play
{
public:
    RallyArmy();

    const char *label() const override { return "RallyArmy"; }

    bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

private:
    std::shared_ptr<DefendBaseSquad> squad;
};