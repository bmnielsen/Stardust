#pragma once

#include "Squad.h"
#include "MyUnit.h"
#include "Base.h"

class WorkerDefenseSquad
{
public:
    explicit WorkerDefenseSquad(Base *base) : base(base) {}

    // Selects a target for each worker in the base.
    std::vector<std::pair<MyUnit, Unit>> selectTargets(std::set<Unit> &enemyUnits);

    void execute(std::vector<std::pair<MyUnit, Unit>> &workersAndTargets, std::vector<std::pair<MyUnit, Unit>> &combatUnitsAndTargets);

    void disband();

private:
    Base *base;
    std::vector<MyUnit> units; // All of the reserved workers

    void executeFullWorkerDefense(std::set<Unit> &enemyUnits, const std::vector<MyUnit> &defendBaseUnits);
};
