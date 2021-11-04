#pragma once
#include "McRave.h"

namespace McRave::Planning {

    void onFrame();
    void onStart();

    BWAPI::UnitType whatPlannedHere(BWAPI::TilePosition);
    bool overlapsPlan(UnitInfo&, BWAPI::Position);
    int getPlannedMineral();
    int getPlannedGas();
    BWEB::Station * getCurrentExpansion();
};
