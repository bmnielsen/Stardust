#pragma once

#include <nlohmann.h>

#include "Common.h"
#include "UnitCluster.h"

class Squad
{
public:

    std::string label;

    void addUnit(const MyUnit &unit);

    void removeUnit(const MyUnit &unit);

    void updateClusters();

    virtual void execute();

    virtual void disband() {}

    [[nodiscard]] BWAPI::Position getTargetPosition() const { return targetPosition; }

    [[nodiscard]] bool needsDetection() const { return !enemiesNeedingDetection.empty(); }

    std::set<MyUnit> &getDetectors() { return detectors; }

    std::set<MyUnit> &getArbiters() { return arbiters; }

    [[nodiscard]] std::vector<MyUnit> getUnits() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] int combatUnitCount() const;

    [[nodiscard]] std::map<BWAPI::UnitType, int> getUnitCountByType() const;

    [[nodiscard]] bool hasClusterWithActivity(UnitCluster::Activity activity) const;

    [[nodiscard]] std::shared_ptr<UnitCluster> vanguardCluster(int *distToTargetPosition = nullptr) const;

    [[nodiscard]] bool canReassignFromVanguardCluster(MyUnit &unit) const;

    void addInstrumentation(nlohmann::json &squadArray) const;

    explicit Squad(std::string label)
            : label(std::move(label))
            , targetPosition(BWAPI::Positions::Invalid)
            , vanguardClusterDistToTargetPosition(INT_MAX) {}

protected:
    BWAPI::Position targetPosition;

    std::set<std::shared_ptr<UnitCluster>> clusters;
    std::map<MyUnit, std::shared_ptr<UnitCluster>> unitToCluster;

    std::shared_ptr<UnitCluster> currentVanguardCluster;
    int vanguardClusterDistToTargetPosition;

    std::set<Unit> enemiesNeedingDetection;
    std::set<MyUnit> detectors; // only mobile detectors
    std::set<MyUnit> arbiters;

    [[nodiscard]] virtual bool canAddUnitToCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster, int dist) const;

    [[nodiscard]] virtual bool shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const;

    [[nodiscard]] virtual bool shouldRemoveFromCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster) const;

    void addUnitToBestCluster(const MyUnit &unit);

    virtual std::shared_ptr<UnitCluster> createCluster(const MyUnit &unit) { return std::make_shared<UnitCluster>(unit); }

    virtual void execute(UnitCluster &cluster) {}

    void updateDetectionNeeds(std::set<Unit> &enemyUnits);

private:
    void executeDetectors();
    void executeArbiters();
};
