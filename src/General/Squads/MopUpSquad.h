#pragma once

#include "Squad.h"

class MopUpSquad : public Squad
{
public:
    MopUpSquad();

    virtual ~MopUpSquad() = default;

private:
    void execute(UnitCluster &cluster) override;
};
