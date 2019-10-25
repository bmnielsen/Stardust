#pragma once

#include "Squad.h"
#include "Base.h"
#include "Map.h"

class DefendBaseSquad : public Squad
{
public:
    explicit DefendBaseSquad(Base *base)
            : Squad((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
            , base(base)
    {
        setTargetPosition();
    }

    virtual ~DefendBaseSquad() = default;

private:
    Base *base;

    void setTargetPosition();

    void execute(UnitCluster &cluster) override;
};
