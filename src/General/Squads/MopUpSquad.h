#pragma once

#include "Squad.h"

class MopUpSquad : public Squad
{
public:
    MopUpSquad() : Squad("Mop Up") {}

    virtual ~MopUpSquad() = default;

private:
    void execute(UnitCluster &cluster) override;
};
