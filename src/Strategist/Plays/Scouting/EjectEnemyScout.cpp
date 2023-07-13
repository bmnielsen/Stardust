#include "EjectEnemyScout.h"

#include "Map.h"
#include "Units.h"

#if INSTRUMENTATION_ENABLED
#define DEBUG_LOGGING true
#endif

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
#if DEBUG_LOGGING
        CherryVis::log() << "EjectEnemyScout: Removing dead or left scout";
#endif
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
#if DEBUG_LOGGING
            CherryVis::log() << "EjectEnemyScout: Found enemy combat unit in base: " << *unit;
#endif
            enemyCombatUnitInBase = true;
            break;
        }
    }

    if (!scout)
    {
        scout = newScout;
#if DEBUG_LOGGING
        if (newScout)
        {
            CherryVis::log() << "EjectEnemyScout: Found enemy scout: " << *scout;
        }
#endif
    }

    // End the play if there is no scout after frame 10000
    if (!scout && currentFrame > 10000)
    {
#if DEBUG_LOGGING
        CherryVis::log() << "EjectEnemyScout: Ending play";
#endif
        status.complete = true;
        return;
    }

    // Release the dragoon if there is no scout or an enemy combat unit in the base
    if (dragoon && (enemyCombatUnitInBase || !scout))
    {
        status.removedUnits.push_back(dragoon);
#if DEBUG_LOGGING
        if (enemyCombatUnitInBase)
        {
            CherryVis::log() << "EjectEnemyScout: Releasing dragoon (enemy combat unit in base)";
        }
        else
        {
            CherryVis::log() << "EjectEnemyScout: Releasing dragoon (no scout)";
        }
#endif
        return;
    }

    // If there is a scout and no enemy unit in our base, make sure we get a dragoon
    if (scout && !dragoon && !enemyCombatUnitInBase)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Dragoon, Map::getMyMain()->getPosition(), 1500);
    }

    // If we have a dragoon and a scout, attack!
    if (scout && dragoon && !dragoon->unstick() && dragoon->isReady())
    {
        if (dragoon->unstick())
        {
#if DEBUG_LOGGING
            CherryVis::log() << "EjectEnemyScout: Unsticking dragoon " << *dragoon;
#endif
        }
        else if (!dragoon->isReady())
        {
#if DEBUG_LOGGING
            CherryVis::log() << "EjectEnemyScout: Not ready dragoon " << *dragoon;
#endif
        }
        else
        {
#if DEBUG_LOGGING
            CherryVis::log() << "EjectEnemyScout: Attack " << *scout << " with " << *dragoon;
#endif
            dragoon->attackUnit(scout);
        }
    }
}

void EjectEnemyScout::addUnit(const MyUnit &unit)
{
    dragoon = unit;

#if DEBUG_LOGGING
    CherryVis::log() << "EjectEnemyScout: Assigned dragoon " << *dragoon;
#endif
}
