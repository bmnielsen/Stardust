#pragma once

#include "Squad.h"
#include "Map.h"

class CorsairSquad : public UnitSquad
{
public:
    explicit CorsairSquad()
            : UnitSquad("Corsairs")
    {
        auto enemyMain = Map::getEnemyMain();
        targetPosition = (enemyMain ? enemyMain : Map::getMyMain())->getPosition();
    };

    virtual ~CorsairSquad() = default;

private:
    void execute() override;

    void clusterAttack(UnitCluster &clump, std::set<Unit> &targets);

    void clusterDefend(UnitCluster &clump, Base *baseLoc, std::set<Unit> &targets);
};
