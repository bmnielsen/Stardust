#pragma once

#include "Squad.h"
#include "MyUnit.h"
#include "MyWorker.h"
#include "Base.h"

class WorkerDefenseSquad
{
public:
    explicit WorkerDefenseSquad(Base *baseLoc) : baseLoc(baseLoc) {}

    /* Selects a target for each worker in the base. */
    std::vector<std::pair<OwnedWorker, Unit>> selectTargets(std::set<Unit> &enemyUnits);

    void execute(std::vector<std::pair<OwnedWorker, Unit>> &workersAndTargets, std::vector<std::pair<OwnedUnit, Unit>> &combatUnitsAndTargets);

    void disband();

private:
    Base *baseLoc;
    std::vector<OwnedWorker> units; // All of the reserved workers
};
