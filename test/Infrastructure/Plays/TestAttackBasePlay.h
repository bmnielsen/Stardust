#pragma once

#include "Play.h"
#include "General.h"
#include "Squads/AttackBaseSquad.h"

class TestAttackBasePlay : public Play
{
public:
    explicit TestAttackBasePlay(Base *base, bool ignoreCombatSim = false)
            : Play((std::ostringstream() << "Test attack base @ " << base->getTilePosition()).str())
            , squad(std::make_shared<AttackBaseSquad>(base))
    {
        squad->ignoreCombatSim = ignoreCombatSim;
        General::addSquad(squad);
    };

    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

private:
    std::shared_ptr<AttackBaseSquad> squad;
};
