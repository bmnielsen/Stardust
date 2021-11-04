#pragma once
#include <BWAPI.h>

namespace McRave::Scouts
{
    void onFrame();
    void onStart();
    void removeUnit(UnitInfo&);
    bool gotFullScout();
    bool isSacrificeScout();
    bool enemyDeniedScout();
    std::vector<BWEB::Station*> getScoutOrder(BWAPI::UnitType);
}