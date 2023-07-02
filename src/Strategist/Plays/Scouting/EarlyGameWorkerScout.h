#pragma once

#include "Play.h"

class EarlyGameWorkerScout : public Play
{
public:
    struct ScoutLookingForEnemyBase
    {
        MyUnit unit = nullptr;
        bool reserved = false;
        Base *targetBase = nullptr;
        int closestDistanceToTargetBase = INT_MAX;
        int lastDistanceToTargetBase = INT_MAX;
        int lastForwardMotionFrame = 0;
    };

    EarlyGameWorkerScout()
            : Play("EarlyGameWorkerScout")
            , fixedPosition(BWAPI::TilePositions::Invalid)
            , hidingUntil(0) {}

    void update() override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

    // Instructs the scout to hide in a corner of the enemy main until the given frame.
    void hideUntil(int frame);

    // Instructs the scout to take a position where it can keep an eye on units leaving the enemy main.
    void monitorEnemyChoke();

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
