#pragma once
#include <BWAPI.h>

namespace McRave::Resources
{
    int getMineralCount();
    int getGasCount();
    int getIncomeMineral();
    int getIncomeGas();
    bool isMineralSaturated();
    bool isGasSaturated();
    bool isHalfMineralSaturated();
    bool isHalfGasSaturated();
    std::set <std::shared_ptr<ResourceInfo>>& getMyMinerals();
    std::set <std::shared_ptr<ResourceInfo>>& getMyGas();
    std::set <std::shared_ptr<ResourceInfo>>& getMyBoulders();

    void recheckSaturation();
    void onFrame();
    void onStart();
    void onEnd();
    void storeResource(BWAPI::Unit);
    void removeResource(BWAPI::Unit);

    std::shared_ptr<ResourceInfo> getResourceInfo(BWAPI::Unit);
}
