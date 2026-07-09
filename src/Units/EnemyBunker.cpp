#include "EnemyBunker.h"

#include "Units.h"

EnemyBunker::EnemyBunker(BWAPI::Unit unit) : UnitImpl(unit), loadedMarines(0), myUnitInRange(false), frameMyUnitInRangeLastChanged(-1)
{
}

void EnemyBunker::update(BWAPI::Unit unit)
{
    UnitImpl::update(unit);

    updateMyUnitInRange();
}

void EnemyBunker::updateUnitInFog()
{
    UnitImpl::updateUnitInFog();

    updateMyUnitInRange();
}

void EnemyBunker::updateMyUnitInRange()
{
    // Look for a unit well inside the bunker's range that the bunker should be able to see
    // We're not checking for detection on DTs or Observers since we want to be sure there is something the bunker could shoot at
    auto bunkerElevation = BWAPI::Broodwar->getGroundHeight(getTilePosition());
    bool currentMyUnitInRange = false;
    for (auto unit : Units::allMine())
    {
        if (!unit->exists()) continue;
        if (unit->bwapiUnit->isLoaded()) continue;
        if (unit->bwapiUnit->isStasised()) continue;
        if (unit->type.hasPermanentCloak()) continue;
        if (BWAPI::Broodwar->getGroundHeight(unit->getTilePosition()) > bunkerElevation) continue;

        if (isInOurWeaponRange(unit, -32))
        {
            currentMyUnitInRange = true;
            break;
        }
    }

    if (currentMyUnitInRange != myUnitInRange)
    {
        myUnitInRange = currentMyUnitInRange;
        frameMyUnitInRangeLastChanged = currentFrame;

#if CHERRYVIS_ENABLED
        CherryVis::log(id) << (myUnitInRange ? "Has" : "Does not have") << " one of my units in range";
#endif
    }
}
