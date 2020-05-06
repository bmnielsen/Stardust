#pragma once

#include "Squad.h"
#include "Base.h"
#include "Map.h"

class DefendBaseSquad : public Squad
{
public:
    explicit DefendBaseSquad(Base *base);

    virtual ~DefendBaseSquad() = default;

private:
    Base *base;
    Choke *choke;
    BWAPI::Position chokeDefendEnd;

    void execute(UnitCluster &cluster) override;
};
