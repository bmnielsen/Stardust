#pragma once

#include "Common.h"
#include "Unit.h"

namespace UnitEventDispatcher
{
    // A new unit has been observed for the first time
    void unitCreated(const Unit& unit);

    // A unit has moved from its previous position
    // We may not know where it is now if it is not currently visible
    void unitMoved(const Unit& unit, BWAPI::Position previousPosition);

    // A unit has changed into a different type
    void unitMorphed(const Unit& unit);

    // A unit has changed owner
    void unitRenegade(const Unit& unit, BWAPI::Player previousOwner);

    // A unit has been destroyed
    void unitDestroyed(const Unit& unit);

    // A Terran building has lifted
    void unitLifted(const Unit& unit);

    // A Terran building that was previously lifted has landed
    void unitLanded(const Unit& unit);
}
