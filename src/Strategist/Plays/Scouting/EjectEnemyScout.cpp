#include "EjectEnemyScout.h"

#include "Map.h"
#include "Units.h"

namespace
{
    bool inMainArea(const Unit &unit)
    {
        if (!unit->lastPositionValid) return false;

        auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition));
        for (const auto &mainArea : Map::getMyMainAreas())
        {
            if (area == mainArea)
            {
                return true;
            }
        }

        return false;
    }
}

void EjectEnemyScout::update()
{
    // First check if our current enemy scout should be cleared
    if (scout && (!scout->exists() || !inMainArea(scout)))
    {
        scout = nullptr;
    }

    // Next scan to get an enemy scout or combat unit
    bool enemyCombatUnitInBase = false;
    Unit newScout = nullptr;
    for (auto &unit : Units::allEnemy())
    {
        if (!unit->exists()) continue;

        if (unit->type.isWorker() || unit->type == BWAPI::UnitTypes::Zerg_Overlord)
        {
            if (inMainArea(unit)) newScout = unit;
        }
        else if (unit->canAttackGround() && inMainArea(unit))
        {
            enemyCombatUnitInBase = true;
            break;
        }
    }

    if (!scout) scout = newScout;

    // End the play if there is no scout after frame 10000
    if (!scout && currentFrame > 10000)
    {
        status.complete = true;
    }

    // Release the dragoon if there is no scout or an enemy combat unit in the base
    if (dragoon && (enemyCombatUnitInBase || !scout))
    {
        status.removedUnits.push_back(dragoon);
        return;
    }

    // If there is a scout and no enemy unit in our base, make sure we get a dragoon
    if (scout && !dragoon && !enemyCombatUnitInBase)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Dragoon, Map::getMyMain()->getPosition(), 1000);
    }

    // If we have a dragoon and a scout, attack!
    if (scout && dragoon && !dragoon->unstick() && dragoon->isReady())
    {
        std::vector<std::pair<MyUnit, Unit>> dummyUnitsAndTargets;
        dragoon->attackUnit(scout, dummyUnitsAndTargets);
    }
}
