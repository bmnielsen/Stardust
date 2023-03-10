#pragma once

#include "Squad.h"

class DefendWallSquad : public Squad
{
public:
    DefendWallSquad();

    virtual ~DefendWallSquad() = default;

private:
    void execute(UnitCluster &cluster) override;
};
