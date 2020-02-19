#include "Play.h"

#include "CherryVis.h"

void Play::addUnit(MyUnit unit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->addUnit(unit);
    }
    else
    {
        Log::Get() << "WARNING: Adding unit to play without a Squad";
    }

    CherryVis::log(unit->id) << "Added to play: " << label();
}

void Play::removeUnit(MyUnit unit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->removeUnit(unit);
    }
    else
    {
        Log::Get() << "WARNING: Removing unit from play without a Squad";
    }

    CherryVis::log(unit->id) << "Removed from play: " << label();
}
