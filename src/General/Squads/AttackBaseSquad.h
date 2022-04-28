#pragma once

#include "Squad.h"
#include "Base.h"

class AttackBaseSquad : public Squad
{
public:
    explicit AttackBaseSquad(Base *base)
            : Squad((std::ostringstream() << "Attack base @ " << base->getTilePosition()).str())
            , base(base)
            , ignoreCombatSim(false)
    {
        targetPosition = base->getPosition();
    };

    virtual ~AttackBaseSquad() = default;

    Base *base;
    bool ignoreCombatSim;

private:
    void execute(UnitCluster &cluster) override;
};
