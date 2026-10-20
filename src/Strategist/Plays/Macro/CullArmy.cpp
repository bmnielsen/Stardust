#include "CullArmy.h"

#include "Map.h"

void CullArmy::update()
{
    /* Clear dead units and count how much supply we have in the current set of units */
    int supplyCount = 0;
    for (auto iterRef = units.begin(); iterRef != units.end();)
    {
        if ((*iterRef)->exists())
        {
            supplyCount += (*iterRef)->type.supplyRequired();
            iterRef++;
        }
        else
        {
            iterRef = units.erase(iterRef);
        }
    }

    if (supplyNeeded <= 0)
    {
        status.complete = true;
        return;
    }

    /* Reserve units if needed */
    int neededUnits = (supplyNeeded - supplyCount + 11) / 4;
    if (neededUnits > 0)
    {
        status.unitRequirements.emplace_back(neededUnits, BWAPI::UnitTypes::Protoss_Dragoon, Map::getMyMain()->getPosition());
    }

    /* Micro the units: everybody attacks the next unit */
    if (units.size() > 1)
    {
        for (int i = 0; i < (units.size() - 1); i++)
        {
            /* If the unit is stuck, unstick it */
            if (units[i]->unstick()) continue;

            /* If the unit is not ready (i.e. is already in the middle of an attack), don't touch it */
            if (!units[i]->isReady()) continue;

            /* Attack the next unit in the list */
            units[i]->attackUnit(units[i + 1]);
        }
    }
}

void CullArmy::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                       const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    auto unitsCopy = units;
    units.clear();
    for (const auto &gameUnit : unitsCopy)
    {
        removedUnitCallback(gameUnit);
    }
}

void CullArmy::addUnit(const OwnedUnit &gameUnit)
{
    units.push_back(gameUnit);
}

void CullArmy::removeUnit(const OwnedUnit &gameUnit)
{
    for (auto iterRef = units.begin(); iterRef != units.end();)
    {
        if ((*iterRef) == gameUnit)
        {
            iterRef = units.erase(iterRef);
        }
        else
        {
            iterRef++;
        }
    }
}
