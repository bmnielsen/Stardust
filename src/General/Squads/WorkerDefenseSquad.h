#pragma once

#include "Squad.h"
#include "MyUnit.h"
#include "Base.h"

class WorkerDefenseSquad
{
public:
    explicit WorkerDefenseSquad(Base *base) : base(base) {}

    void execute(std::set<Unit> &enemiesInBase, const std::shared_ptr<Squad> &defendBaseSquad);

    void disband();

private:
    Base *base;
    std::vector<MyUnit> units;

    void executeFullWorkerDefense(std::set<Unit> &enemyUnits, const std::vector<MyUnit> &defendBaseUnits);
};
