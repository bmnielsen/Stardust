#pragma once

#include "Common.h"
#include "UnitCluster.h"

class Squad
{
public:
    
    void addUnit(BWAPI::Unit unit);
    void removeUnit(BWAPI::Unit unit);
    
    void updateClusters();
    void execute();

    Squad() : targetPosition(BWAPI::Positions::Invalid) {}

protected:
    BWAPI::Position targetPosition;

    std::set<std::shared_ptr<UnitCluster>>              clusters;
    std::map<BWAPI::Unit, std::shared_ptr<UnitCluster>> unitToCluster;

    virtual void execute(UnitCluster & cluster) = 0;
};
