#pragma once

#include <fap.h>

#include "Common.h"
#include "Squad.h"
#include "Squads/AttackBaseSquad.h"
#include "Base.h"
#include "EnemyArmy.h"

namespace General
{
    void initialize();

    void updateClusters();

    void issueOrders();

    void addSquad(const std::shared_ptr<Squad> &squad);

    void removeSquad(const std::shared_ptr<Squad> &squad);

    AttackBaseSquad *getAttackBaseSquad(Base *targetBase);

    // Gets the army the specified unit is part of, or nullptr if it is not part of any army
    const EnemyArmy* armyForEnemyUnit(const Unit &enemyUnit);

    // Gets an enemy army matching the given predicate, or nullptr if none match
    const EnemyArmy* getEnemyArmy(const std::function<bool(const EnemyArmy &)> &predicate);

    void writeInstrumentation();
}

namespace CombatSim
{
    void initialize();

    int unitValue(const FAP::FAPUnit<> &unit);

    int unitValue(const Unit &unit);

    int unitValue(BWAPI::UnitType type);

    // Used from tests for sim evaluation
    void setMaxIterations(int iterations);
}
