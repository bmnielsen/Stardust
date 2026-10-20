#pragma once

#include <nlohmann.h>

#include "Common.h"
#include "UnitCluster.h"

class UnitSquad
{
public:

    std::string label;

    void addUnit(const OwnedUnit &gameUnit);

    void removeUnit(const OwnedUnit &gameUnit);

    void updateClusters();

    virtual void execute();

    virtual void disband() {}

    [[nodiscard]] BWAPI::Position getTargetPosition() const { return targetPosition; }

    [[nodiscard]] bool needsDetection() const { return !enemiesNeedingDetection.empty(); }

    [[nodiscard]] bool hasDetection() const;

    std::set<OwnedUnit> &getDetectors() { return detectors; }

    std::set<OwnedUnit> &getArbiters() { return arbiters; }

    [[nodiscard]] std::vector<OwnedUnit> getUnits() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] int combatUnitCount() const;

    [[nodiscard]] std::map<BWAPI::UnitType, int> getUnitCountByType() const;

    [[nodiscard]] bool hasClusterWithActivity(UnitCluster::Activity activity) const;

    [[nodiscard]] std::shared_ptr<UnitCluster> vanguardCluster(int *distToTargetPosition = nullptr) const;

    [[nodiscard]] bool canReassignFromVanguardCluster(OwnedUnit &gameUnit) const;

    void addInstrumentation(nlohmann::json &squadArray) const;

    explicit UnitSquad(std::string label)
            : label(std::move(label))
            , targetPosition(BWAPI::Positions::Invalid)
            , vanguardClusterDistToTargetPosition(INT_MAX) {}

protected:
    BWAPI::Position targetPosition;

    std::set<std::shared_ptr<UnitCluster>> clusters;
    std::map<OwnedUnit, std::shared_ptr<UnitCluster>> unitToCluster;

    std::shared_ptr<UnitCluster> currentVanguardCluster;
    int vanguardClusterDistToTargetPosition;

    std::set<Unit> enemiesNeedingDetection;
    std::set<OwnedUnit> detectors; // only mobile detectors
    std::set<OwnedUnit> arbiters;
    std::set<OwnedUnit> templars; // High Templar, only extracted when storm_cast is on

    [[nodiscard]] virtual bool canAddUnitToCluster(const OwnedUnit &gameUnit, const std::shared_ptr<UnitCluster> &clump, int span) const;

    [[nodiscard]] virtual bool shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const;

    [[nodiscard]] virtual bool shouldRemoveFromCluster(const OwnedUnit &gameUnit, const std::shared_ptr<UnitCluster> &clump) const;

    void addUnitToBestCluster(const OwnedUnit &gameUnit);

    virtual std::shared_ptr<UnitCluster> createCluster(const OwnedUnit &gameUnit) { return std::make_shared<UnitCluster>(gameUnit); }

    virtual void execute(UnitCluster &clump) {}

    void updateDetectionNeeds(std::set<Unit> &enemyUnits);

private:
    void executeDetectors();
    void executeArbiters();
    void executeTemplars();
};
