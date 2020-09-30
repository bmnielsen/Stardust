#pragma once

#include <fap.h>

#include "Common.h"
#include "Squad.h"
#include "Squads/AttackBaseSquad.h"
#include "Base.h"

namespace General
{
    void initialize();

    void updateClusters();

    void issueOrders();

    void addSquad(const std::shared_ptr<Squad> &squad);

    void removeSquad(const std::shared_ptr<Squad> &squad);

    AttackBaseSquad *getAttackBaseSquad(Base *targetBase);
}

namespace CombatSim
{
    void initialize();

    int unitValue(const FAP::FAPUnit<> &unit);

    int unitValue(const Unit &unit);

    int unitValue(BWAPI::UnitType type);
}
