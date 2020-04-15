#pragma once

#include <utility>
#include "Common.h"

#include "UpgradeTracker.h"

class Grid
{
public:
    struct GridData
    {
        std::vector<long> data;

        int maxX;
        int maxY;

        int frameLastUpdated;
        mutable int frameLastDumped;

        GridData() : frameLastUpdated(-1), frameLastDumped(-1)
        {
            maxX = BWAPI::Broodwar->mapWidth() * 4;
            maxY = BWAPI::Broodwar->mapHeight() * 4;
            data.assign(maxX * maxY, 0);
        }

        long operator[](BWAPI::Position pos) const
        {
            return data[(pos.x >> 3U) + (pos.y >> 3U) * maxX];
        }

        long operator[](BWAPI::WalkPosition pos) const
        {
            return data[pos.x + pos.y * maxX];
        }

        void add(BWAPI::UnitType type, int range, BWAPI::Position position, int delta);
    };

    explicit Grid(std::shared_ptr<UpgradeTracker> upgradeTracker) : upgradeTracker(std::move(upgradeTracker)) {}

    void unitCreated(BWAPI::UnitType type, BWAPI::Position position, bool completed);

    void unitCompleted(BWAPI::UnitType type, BWAPI::Position position);

    void unitMoved(BWAPI::UnitType type, BWAPI::Position position, BWAPI::UnitType fromType, BWAPI::Position fromPosition);

    void unitDestroyed(BWAPI::UnitType type, BWAPI::Position position, bool completed);

    void unitWeaponDamageUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerDamage, int newDamage);

    void unitWeaponRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerRange, int newRange);

    void unitSightRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, int formerRange, int newRange);

    long collision(BWAPI::Position position) const { return _collision[position]; };

    long collision(BWAPI::WalkPosition position) const { return _collision[position]; };

    long groundThreat(BWAPI::Position position) const { return _groundThreat[position]; };

    long groundThreat(BWAPI::WalkPosition position) const { return _groundThreat[position]; };

    long airThreat(BWAPI::Position position) const { return _airThreat[position]; };

    long airThreat(BWAPI::WalkPosition position) const { return _airThreat[position]; };

    long detection(BWAPI::Position position) const { return _detection[position]; };

    long detection(BWAPI::WalkPosition position) const { return _detection[position]; };

    void dumpCollisionHeatmapIfChanged(const std::string& heatmapName) const { dumpHeatmapIfChanged(heatmapName, _collision); };

    void dumpGroundThreatHeatmapIfChanged(const std::string& heatmapName) const { dumpHeatmapIfChanged(heatmapName, _groundThreat); };

    void dumpAirThreatHeatmapIfChanged(const std::string& heatmapName) const { dumpHeatmapIfChanged(heatmapName, _airThreat); };

    void dumpDetectionHeatmapIfChanged(const std::string& heatmapName) const { dumpHeatmapIfChanged(heatmapName, _detection); };

private:
    std::shared_ptr<UpgradeTracker> upgradeTracker;

    GridData _collision;
    GridData _groundThreat;
    GridData _airThreat;
    GridData _detection;

    void dumpHeatmapIfChanged(const std::string &heatmapName, const GridData &data) const;
};
