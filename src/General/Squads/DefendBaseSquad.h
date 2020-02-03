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
        defaultTargetPosition = targetPosition;
    }

    virtual ~DefendBaseSquad() = default;

private:
    Base *base;
    BWAPI::Position defaultTargetPosition;

    void setTargetPosition();

    void execute(UnitCluster &cluster) override;
};
