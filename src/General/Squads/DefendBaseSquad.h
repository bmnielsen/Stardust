#pragma once

#include "Squad.h"
#include "Base.h"
#include "Unit.h"

class DefendBaseSquad : public Squad
{
public:
    explicit DefendBaseSquad(Base *base)
            : Squad((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
    {
        targetPosition = base->getPosition();
    };

    virtual ~DefendBaseSquad() = default;

    std::set<Unit> enemyUnits;

private:
    void execute(UnitCluster &cluster) override;
};
