#include "CullArmy.h"

#include "Map.h"

void CullArmy::update()
{
    // Clear dead units and count how much supply we have in the current set of units
    int supplyCount = 0;
    for (auto it = units.begin(); it != units.end();)
    {
        if ((*it)->exists())
        {
            supplyCount += (*it)->type.supplyRequired();
            it++;
        }
        else
        {
            it = units.erase(it);
        }
    }

    if (supplyNeeded <= 0)
    {
        status.complete = true;
        return;
    }

    // Reserve units if needed
    int neededUnits = (supplyNeeded - supplyCount + 11) / 4;
    if (neededUnits > 0)
    {
        status.unitRequirements.emplace_back(neededUnits, BWAPI::UnitTypes::Protoss_Dragoon, Map::getMyMain()->getPosition());
    }

    // Micro the units: everybody attacks the next unit
    if (units.size() > 1)
    {
        std::vector<std::pair<MyUnit, Unit>> dummyUnitsAndTargets;
        for (int i = 0; i < (units.size() - 1); i++)
        {
            // If the unit is stuck, unstick it
            if (units[i]->unstick()) continue;

            // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
            if (!units[i]->isReady()) continue;

            // Attack the next unit in the list
            units[i]->attackUnit(units[i + 1], dummyUnitsAndTargets);
        }
    }
}

void CullArmy::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                       const std::function<void(const MyUnit)> &movableUnitCallback)
{
    auto unitsCopy = units;
    units.clear();
    for (const auto &unit : unitsCopy)
    {
        removedUnitCallback(unit);
    }
}

void CullArmy::addUnit(const MyUnit &unit)
{
    units.push_back(unit);
}

void CullArmy::removeUnit(const MyUnit &unit)
{
    for (auto it = units.begin(); it != units.end();)
    {
        if ((*it) == unit)
        {
            it = units.erase(it);
        }
        else
        {
            it++;
        }
    }
}
