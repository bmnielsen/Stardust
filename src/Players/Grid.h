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

        long at(int walkX, int walkY) const
        {
            return data[walkX + walkY * maxX];
        }

        void add(BWAPI::UnitType type, int range, BWAPI::Position position, int delta);
    };

    explicit Grid(std::shared_ptr<UpgradeTracker> upgradeTracker) : upgradeTracker(std::move(upgradeTracker)) {}

    void unitCreated(BWAPI::UnitType type, BWAPI::Position position, bool completed, bool burrowed, bool immobile);

    void unitCompleted(BWAPI::UnitType type, BWAPI::Position position, bool burrowed, bool immobile);

    void unitMoved(BWAPI::UnitType type,
                   BWAPI::Position position,
                   bool burrowed,
                   bool immobile,
                   BWAPI::UnitType fromType,
                   BWAPI::Position fromPosition,
                   bool fromBurrowed,
                   bool fromImmobile);

    void unitDestroyed(BWAPI::UnitType type, BWAPI::Position position, bool completed, bool burrowed, bool immobile);

    void unitWeaponDamageUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerDamage, int newDamage);

    void unitWeaponRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerRange, int newRange);

    void unitSightRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, int formerRange, int newRange);

    long collision(BWAPI::Position position) const { return _collision[position]; };

    long collision(BWAPI::WalkPosition position) const { return _collision[position]; };

    long collision(int walkX, int walkY) const { return _collision.at(walkX, walkY); };

    long groundThreat(BWAPI::Position position) const { return _groundThreat[position]; };

    long groundThreat(BWAPI::WalkPosition position) const { return _groundThreat[position]; };

    long groundThreat(int walkX, int walkY) const { return _groundThreat.at(walkX, walkY); };

    long staticGroundThreat(BWAPI::Position position) const { return _staticGroundThreat[position]; };

    long staticGroundThreat(BWAPI::WalkPosition position) const { return _staticGroundThreat[position]; };

    long staticGroundThreat(int walkX, int walkY) const { return _staticGroundThreat.at(walkX, walkY); };

    long airThreat(BWAPI::Position position) const { return _airThreat[position]; };

    long airThreat(BWAPI::WalkPosition position) const { return _airThreat[position]; };

    long airThreat(int walkX, int walkY) const { return _airThreat.at(walkX, walkY); };

    long detection(BWAPI::Position position) const { return _detection[position]; };

    long detection(BWAPI::WalkPosition position) const { return _detection[position]; };

    long detection(int walkX, int walkY) const { return _detection.at(walkX, walkY); };

    long stasisRange(BWAPI::Position position) const { return _stasisRange[position]; };

    long stasisRange(BWAPI::WalkPosition position) const { return _stasisRange[position]; };

    long stasisRange(int walkX, int walkY) const { return _stasisRange.at(walkX, walkY); };

    void dumpCollisionHeatmapIfChanged(const std::string &heatmapName) const { dumpHeatmapIfChanged(heatmapName, _collision); };

    void dumpGroundThreatHeatmapIfChanged(const std::string &heatmapName) const { dumpHeatmapIfChanged(heatmapName, _groundThreat); };

    void dumpStaticGroundThreatHeatmapIfChanged(const std::string &heatmapName) const { dumpHeatmapIfChanged(heatmapName, _staticGroundThreat); };

    void dumpAirThreatHeatmapIfChanged(const std::string &heatmapName) const { dumpHeatmapIfChanged(heatmapName, _airThreat); };

    void dumpDetectionHeatmapIfChanged(const std::string &heatmapName) const { dumpHeatmapIfChanged(heatmapName, _detection); };

    void dumpStasisRangeHeatmapIfChanged(const std::string &heatmapName) const { dumpHeatmapIfChanged(heatmapName, _stasisRange); };

private:
    std::shared_ptr<UpgradeTracker> upgradeTracker;

    GridData _collision;
    GridData _groundThreat;
    GridData _staticGroundThreat;
    GridData _airThreat;
    GridData _detection;
    GridData _stasisRange;

    static void dumpHeatmapIfChanged(const std::string &heatmapName, const GridData &data);
};
