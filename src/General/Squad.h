#pragma once

#include "Common.h"
#include "UnitCluster.h"

class Squad
{
public:

    std::string label;

    void addUnit(const MyUnit &unit);

    void removeUnit(const MyUnit &unit);

    void updateClusters();

    void execute();

    BWAPI::Position getTargetPosition() { return targetPosition; }

    bool getNeedsDetection() { return needsDetection; }

    std::vector<MyUnit> getUnits();

    bool hasClusterWithActivity(UnitCluster::Activity activity);

    explicit Squad(std::string label) : label(std::move(label)), targetPosition(BWAPI::Positions::Invalid), needsDetection(false) {}

protected:
    BWAPI::Position targetPosition;

    std::set<std::shared_ptr<UnitCluster>> clusters;
    std::map<MyUnit, std::shared_ptr<UnitCluster>> unitToCluster;

    bool needsDetection;

    void addUnitToBestCluster(const MyUnit &unit);

    virtual std::shared_ptr<UnitCluster> createCluster(MyUnit unit) { return std::make_shared<UnitCluster>(unit); }

    virtual void execute(UnitCluster &cluster) = 0;

    void updateNeedsDetection(std::set<Unit> &enemyUnits);
};
