#pragma once

#include "Play.h"

class EarlyGameWorkerScout : public PlayItem
{
public:
    struct ScoutLookingForEnemyBase
    {
        OwnedWorker gameUnit = nullptr;
        bool reserved = false;
        Base *targetBase = nullptr;
        int closestDistanceToTargetBase = INT_MAX;
        int lastDistanceToTargetBase = INT_MAX;
        int lastForwardMotionFrame = 0;
    };

    EarlyGameWorkerScout()
            : PlayItem("EarlyGameWorkerScout")
            , fixedPosition(BWAPI::TilePositions::Invalid)
            , hidingUntil(0) {}

    void update() override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    /* Instructs the scout to hide in a corner of the enemy main until the given frame. */
    void hideUntil(int tick);

    /* Instructs the scout to take a position where it can keep an eye on units leaving the enemy main. */
    void monitorEnemyChoke();

    /* Direction 2 (2026-09-24 plan): set by the engine while the DT-scout */
    /* window is active (enemy core done, no Citadel/Archives sighted yet). */
    /* The ranged-threat completion below is suppressed so the scout keeps */
    /* feeding handleDetection the enemy tech state - PW abandons a scouted */
    /* DT line with 0.65-0.85 probability, so the scout may pay for itself. */
    bool ignoreRangedThreatCompletion = false;

private:
    ScoutLookingForEnemyBase scout;
    ScoutLookingForEnemyBase secondScout;

    std::map<int, std::set<BWAPI::TilePosition>> scoutTiles;
    std::set<const BWEM::Area *> scoutAreas;
    BWAPI::TilePosition fixedPosition;
    int hidingUntil;

    void findEnemyBase();

    void scoutEnemyBase();

    bool selectScout();

    void selectSecondScout();

    BWAPI::TilePosition getHighestPriorityScoutTile();
};
