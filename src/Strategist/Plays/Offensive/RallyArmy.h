#pragma once

#include "Play.h"
#include "Squads/DefendBaseSquad.h"

class RallyArmy : public Play
{
public:
    RallyArmy();

    const char *label() const { return "RallyArmy"; }

    bool receivesUnassignedUnits() const { return true; }

    std::shared_ptr<Squad> getSquad() { return squad; }

    void update();

private:
    std::shared_ptr<DefendBaseSquad> squad;
};
