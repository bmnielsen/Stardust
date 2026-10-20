#pragma once

#include "Squad.h"

class MopUpSquad : public UnitSquad
{
public:
    MopUpSquad();

    virtual ~MopUpSquad() = default;

private:
    void execute(UnitCluster &clump) override;
};
