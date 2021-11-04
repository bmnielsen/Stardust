#pragma once
#include <BWAPI.h>

namespace McRave::Workers {

    void onFrame();
    void removeUnit(UnitInfo&);

    int getMineralWorkers();
    int getGasWorkers();
    int getBoulderWorkers();
    bool canAssignToBuild(UnitInfo&);
    bool shouldMoveToBuild(UnitInfo&, BWAPI::TilePosition, BWAPI::UnitType);
}
