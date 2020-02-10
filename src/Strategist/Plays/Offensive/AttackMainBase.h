#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackMainBase : public Play
{
public:
    explicit AttackMainBase(Base *base);

    const char *label() const override { return "AttackMainBase"; }

    bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

private:
    Base *base;
    std::shared_ptr<AttackBaseSquad> squad;
};