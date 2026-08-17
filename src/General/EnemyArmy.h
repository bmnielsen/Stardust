#pragma once

#include "Common.h"
#include "Unit.h"

class EnemyArmy
{
public:
    BWAPI::Position center;
    std::set<Unit> units;

    EnemyArmy(const Unit &unit);

    bool tryAddUnit(const Unit &unit);
};
