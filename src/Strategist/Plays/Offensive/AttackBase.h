#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackBase : public Play
{
public:
    AttackBase(Base *base);

    const char *label() const { return "AttackBase"; }

    bool receivesUnassignedUnits() const { return true; }

    std::shared_ptr<Squad> getSquad() { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
