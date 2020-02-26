#pragma once

#include "Play.h"
#include "General.h"
#include "Squads/AttackBaseSquad.h"

class TestAttackBasePlay : public Play
{
public:
    explicit TestAttackBasePlay(Base *base) : squad(std::make_shared<AttackBaseSquad>(base)) { General::addSquad(squad); };

    [[nodiscard]] const char *label() const override { return "TestAttackBasePlay"; }

    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
