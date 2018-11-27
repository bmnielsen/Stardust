#pragma once

#include "MapSpecificOverride.h"

class Plasma : public MapSpecificOverride
{
public:
    bool hasMineralWalking() { return true; }
    void initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> & chokes);
    bool canUseBwemPath(BWAPI::UnitType unitType)
    {
        // On Plasma, BWEM doesn't mark the mineral walking chokes as blocked
        // So we can use BWEM pathing for workers but nothing else
        return unitType.isWorker();
    }
};
