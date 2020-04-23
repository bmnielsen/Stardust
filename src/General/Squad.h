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

    [[nodiscard]] BWAPI::Position getTargetPosition() const { return targetPosition; }

    [[nodiscard]] bool needsDetection() const { return !enemiesNeedingDetection.empty(); }

    std::set<MyUnit> &getDetectors() { return detectors; }

    [[nodiscard]] std::vector<MyUnit> getUnits() const;

    [[nodiscard]] std::map<BWAPI::UnitType, int> getUnitCountByType() const;

    [[nodiscard]] bool hasClusterWithActivity(UnitCluster::Activity activity) const;

    explicit Squad(std::string label) : label(std::move(label)), targetPosition(BWAPI::Positions::Invalid) {}

protected:
    BWAPI::Position targetPosition;

    std::set<std::shared_ptr<UnitCluster>> clusters;
    std::map<MyUnit, std::shared_ptr<UnitCluster>> unitToCluster;

    void addUnitToBestCluster(const MyUnit &unit);

    virtual std::shared_ptr<UnitCluster> createCluster(MyUnit unit) { return std::make_shared<UnitCluster>(unit); }

    virtual void execute(UnitCluster &cluster) = 0;

    void updateDetectionNeeds(std::set<Unit> &enemyUnits);

private:
    std::set<MyUnit> detectors;
    std::set<Unit> enemiesNeedingDetection;

    void executeDetectors();
};
