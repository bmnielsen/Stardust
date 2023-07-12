#pragma once
#include <BWAPI.h>

namespace McRave::Units {
    
    std::shared_ptr<UnitInfo> getUnitInfo(BWAPI::Unit);
    std::set<std::shared_ptr<UnitInfo>>& getUnits(PlayerState);
    std::map<BWAPI::UnitSizeType, int>& getAllyGrdSizes();
    std::map<BWAPI::UnitSizeType, int>& getEnemyGrdSizes();
    std::map<BWAPI::UnitSizeType, int>& getAllyAirSizes();
    std::map<BWAPI::UnitSizeType, int>& getEnemyAirSizes();
    BWAPI::Position getEnemyArmyCenter();
    double getImmThreat();

    int getMyVisible(BWAPI::UnitType);
    int getMyComplete(BWAPI::UnitType);

    void onFrame();
}