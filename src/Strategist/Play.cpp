#include "Play.h"

#include "General.h"
#include "CherryVis.h"

void Play::addUnit(const MyUnit &unit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->addUnit(unit);
    }
}

void Play::removeUnit(const MyUnit &unit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->removeUnit(unit);
    }
}

void Play::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                   const std::function<void(const MyUnit)> &movableUnitCallback)
{
    // By default. all units in the squad are considered movable
    if (getSquad() != nullptr)
    {
        for (const auto &unit : getSquad()->getUnits())
        {
            movableUnitCallback(unit);
        }

        General::removeSquad(getSquad());
    }
}
