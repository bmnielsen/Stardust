#pragma once

#include <fap.h>

#include "Common.h"
#include "Squad.h"
#include "Squads/AttackBaseSquad.h"
#include "Base.h"

namespace Marshal
{
    void initialize();

    void updateClusters();

    void issueOrders();

    void addSquad(const std::shared_ptr<UnitSquad> &squad);

    void removeSquad(const std::shared_ptr<UnitSquad> &squad);

    AttackBaseSquad *getAttackBaseSquad(Base *targetBase);

    void writeInstrumentation();
}

namespace CombatSim
{
    void initialize();

    int unitValue(const FAP::FAPUnit<> &gameUnit);

    int unitValue(const Unit &gameUnit);

    int unitValue(BWAPI::UnitType type);

    /* Used from tests for sim evaluation */
    void setMaxIterations(int iterations);
}
