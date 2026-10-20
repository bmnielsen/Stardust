#include "EjectEnemyScout.h"

#include "Map.h"
#include "Units.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_LOGGING true
#endif

namespace
{
    bool inMainArea(const Unit &gameUnit)
    {
        if (!gameUnit->lastPositionValid) return false;

        auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos));
        auto &mainAreas = Map::getMyMainAreas();
        return std::any_of(mainAreas.begin(), mainAreas.end(), [&area](const auto &mainArea)
        {
            return area == mainArea;
        });
    }
}

void EjectEnemyScout::update()
{
    /* First check if our current enemy scout should be cleared */
    if (scout && (!scout->exists() || !inMainArea(scout)))
    {
#if DEBUG_LOGGING
        Telemetry::log() << "EjectEnemyScout: Removing dead or left scout";
#endif
        scout = nullptr;
    }

    /* Next scan to get an enemy scout or combat unit */
    bool enemyCombatUnitInBase = false;
    Unit newScout = nullptr;
    for (auto &gameUnit : UnitMgr::allEnemy())
    {
        if (!gameUnit->exists()) continue;

        if (gameUnit->type.isWorker() || gameUnit->type == BWAPI::UnitTypes::Zerg_Overlord)
        {
            if (inMainArea(gameUnit)) newScout = gameUnit;
        }
        else if (gameUnit->canAttackGround() && inMainArea(gameUnit))
        {
#if DEBUG_LOGGING
            Telemetry::log() << "EjectEnemyScout: Found enemy combat unit in base: " << *gameUnit;
#endif
            enemyCombatUnitInBase = true;
            break;
        }
    }

    if (!scout)
    {
        scout = newScout;
        if (newScout)
        {
            hasHadScout = true;
#if DEBUG_LOGGING
            Telemetry::log() << "EjectEnemyScout: Found enemy scout: " << *scout;
#endif
        }
    }

    /* End the play if there is no scout after frame 10000 */
    if (!scout && frameNow > 10000)
    {
#if DEBUG_LOGGING
        Telemetry::log() << "EjectEnemyScout: Ending play";
#endif
        status.complete = true;
        return;
    }

    /* Release the dragoon if there is no scout or an enemy combat unit in the base */
    if (dragoon && (enemyCombatUnitInBase || !scout))
    {
        status.removedUnits.push_back(dragoon);
#if DEBUG_LOGGING
        if (enemyCombatUnitInBase)
        {
            Telemetry::log() << "EjectEnemyScout: Releasing dragoon (enemy combat unit in base)";
        }
        else
        {
            Telemetry::log() << "EjectEnemyScout: Releasing dragoon (no scout)";
        }
#endif
        return;
    }

    /* If there is a scout and no enemy unit in our base, make sure we get a dragoon */
    if (scout && !dragoon && !enemyCombatUnitInBase)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Dragoon, Map::getMyMain()->getPosition(), 1500);
    }

    /* If we have a dragoon and a scout, attack! */
    if (scout && dragoon && !dragoon->unstick() && dragoon->isReady())
    {
        if (dragoon->unstick())
        {
#if DEBUG_LOGGING
            Telemetry::log() << "EjectEnemyScout: Unsticking dragoon " << *dragoon;
#endif
        }
        else if (!dragoon->isReady())
        {
#if DEBUG_LOGGING
            Telemetry::log() << "EjectEnemyScout: Not ready dragoon " << *dragoon;
#endif
        }
        else
        {
#if DEBUG_LOGGING
            Telemetry::log() << "EjectEnemyScout: Attack " << *scout << " with " << *dragoon;
#endif
            dragoon->attackUnit(scout);
        }
    }
}

void EjectEnemyScout::addUnit(const OwnedUnit &gameUnit)
{
    dragoon = gameUnit;

#if DEBUG_LOGGING
    Telemetry::log() << "EjectEnemyScout: Assigned dragoon " << *dragoon;
#endif
}

bool EjectEnemyScout::hasEjectedScout() const
{
    return (hasHadScout && !scout) || (frameNow > 5000 && !scout);
}
