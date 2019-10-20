#pragma once

#include "Common.h"
#include "UnitCluster.h"

class Squad
{
public:

    std::string label;

    void addUnit(BWAPI::Unit unit);

    void removeUnit(BWAPI::Unit unit);

    void updateClusters();

    void execute();

    BWAPI::Position getTargetPosition() { return targetPosition; }

    std::vector<BWAPI::Unit> getUnits();

    explicit Squad(std::string label) : label(std::move(label)), targetPosition(BWAPI::Positions::Invalid) {}

protected:
    BWAPI::Position targetPosition;

    std::set<std::shared_ptr<UnitCluster>> clusters;
    std::map<BWAPI::Unit, std::shared_ptr<UnitCluster>> unitToCluster;

    void addUnitToBestCluster(BWAPI::Unit unit);

    virtual std::shared_ptr<UnitCluster> createCluster(BWAPI::Unit unit) { return std::make_shared<UnitCluster>(unit); }

    virtual void execute(UnitCluster &cluster) = 0;
};
