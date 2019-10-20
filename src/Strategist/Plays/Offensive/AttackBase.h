#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackBase : public Play
{
public:
    explicit AttackBase(Base *base);

    const char *label() const override { return "AttackBase"; }

    bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
