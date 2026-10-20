#include "Play.h"

#include "General.h"
#include "CherryVis.h"

void PlayItem::addUnit(const OwnedUnit &gameUnit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->addUnit(gameUnit);
    }
}

void PlayItem::removeUnit(const OwnedUnit &gameUnit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->removeUnit(gameUnit);
    }
}

void PlayItem::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                   const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    /* By default, all units in the squad except cannons are considered movable */
    if (getSquad() != nullptr)
    {
        for (const auto &gameUnit : getSquad()->getUnits())
        {
            if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
            {
                removedUnitCallback(gameUnit);
            }
            else
            {
                movableUnitCallback(gameUnit);
            }
        }

        Marshal::removeSquad(getSquad());
    }
}
