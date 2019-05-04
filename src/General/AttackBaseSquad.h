#pragma once

#include "Squad.h"
#include "Base.h"

class AttackBaseSquad : public Squad
{
public:
    AttackBaseSquad(Base* base)
        : Squad((std::ostringstream() << "Attack base @ " << base->getTilePosition()).str())
        , base(base)
    {
        targetPosition = base->getPosition();
    };

private:
    Base* base;

    void execute(UnitCluster & cluster);
};
