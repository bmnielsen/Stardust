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

    // Next try to find an enemy scout
    if (!scout)
    {
        for (auto &unit : Units::allEnemy())
        {
            if (!(unit->type.isWorker() || unit->type == BWAPI::UnitTypes::Zerg_Overlord)) continue;
            if (inMainArea(unit))
            {
                scout = unit;
                break;
            }
        }
    }

    // If there is no enemy scout, make sure we release our dragoon
    if (dragoon && !scout)
    {
        status.removedUnits.push_back(dragoon);
    }

    // End the play if there is no scout after frame 10000
    if (!scout && BWAPI::Broodwar->getFrameCount() > 10000)
    {
        status.complete = true;
    }

    // If there is a scout, make sure we get a dragoon
    if (scout && !dragoon)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Dragoon, Map::getMyMain()->getPosition());
    }

    // If we have a dragoon and a scout, attack!
    if (scout && dragoon && !dragoon->unstick() && dragoon->isReady())
    {
        std::vector<std::pair<MyUnit, Unit>> dummyUnitsAndTargets;
        dragoon->attackUnit(scout, dummyUnitsAndTargets);
    }
}
