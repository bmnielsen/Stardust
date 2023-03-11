#pragma once

#include "Squad.h"
#include "ForgeGatewayWall.h"

class DefendWallSquad : public Squad
{
public:
    DefendWallSquad();

    virtual ~DefendWallSquad() = default;

private:
    ForgeGatewayWall wall;

    void execute(UnitCluster &cluster) override;
};
