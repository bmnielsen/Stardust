#pragma once

#include "Squad.h"
#include "Base.h"

class AttackBaseSquad : public Squad
{
public:
    explicit AttackBaseSquad(Base *base)
            : Squad((std::ostringstream() << "Attack base @ " << base->getTilePosition()).str())
    {
        targetPosition = base->getPosition();
    };

    virtual ~AttackBaseSquad() = default;

private:
    void execute(UnitCluster &cluster) override;
};
