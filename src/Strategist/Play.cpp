#include "Play.h"

#include "CherryVis.h"

void Play::addUnit(BWAPI::Unit unit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->addUnit(unit);
    }
    else
    {
        Log::Get() << "WARNING: Adding unit to play without a Squad";
    }

    CherryVis::log(unit) << "Added to play: " << label();
}

void Play::removeUnit(BWAPI::Unit unit)
{
    if (getSquad() != nullptr)
    {
        getSquad()->removeUnit(unit);
    }
    else
    {
        Log::Get() << "WARNING: Removing unit from play without a Squad";
    }

    CherryVis::log(unit) << "Removed from play: " << label();
}
