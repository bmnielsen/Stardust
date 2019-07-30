#pragma once

#include "Squad.h"
#include "Base.h"

class DefendBaseSquad : public Squad
{
public:
    DefendBaseSquad(Base* base)
            : Squad((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
              , base(base)
    {
        targetPosition = base->getPosition();
    };

private:
    Base* base;

    void execute(UnitCluster & cluster);
};
