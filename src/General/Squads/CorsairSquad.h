#pragma once

#include "Squad.h"
#include "Map.h"

class CorsairSquad : public Squad
{
public:
    explicit CorsairSquad()
            : Squad("Corsairs")
    {
        auto enemyMain = Map::getEnemyMain();
        targetPosition = (enemyMain ? enemyMain : Map::getMyMain())->getPosition();
    };

    virtual ~CorsairSquad() = default;

private:
    void execute() override;

    void clusterAttack(UnitCluster &cluster, std::set<Unit> &targets);
};
