#include "EnemyArmy.h"

// Distance threshold to center of army required to add another unit
#define ADD_THRESHOLD 480

EnemyArmy::EnemyArmy(const Unit &unit) : center(unit->simPosition)
{
    units.insert(unit);
}

bool EnemyArmy::tryAddUnit(const Unit &unit)
{
    auto dist = unit->getDistance(center);
    if (dist > (ADD_THRESHOLD + (units.size() * 120) / (units.size() + 10))) return false;

    units.insert(unit);

    center = BWAPI::Position(
            ((center.x * ((int)units.size() - 1)) + unit->simPosition.x) / (int)units.size(),
            ((center.y * ((int)units.size() - 1)) + unit->simPosition.y) / (int)units.size());
    return true;
}
