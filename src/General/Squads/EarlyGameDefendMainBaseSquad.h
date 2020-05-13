#pragma once

#include "Squad.h"
#include "Choke.h"

class EarlyGameDefendMainBaseSquad : public Squad
{
public:
    EarlyGameDefendMainBaseSquad();

    virtual ~EarlyGameDefendMainBaseSquad() = default;

private:
    Choke *choke;
    BWAPI::Position chokeDefendEnd;

    void execute(UnitCluster &cluster) override;
};
