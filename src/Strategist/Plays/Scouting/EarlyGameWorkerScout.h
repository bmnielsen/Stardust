#pragma once

#include "Play.h"

class EarlyGameWorkerScout : public Play
{
public:
    EarlyGameWorkerScout()
            : Play("EarlyGameWorkerScout")
            , scout(nullptr)
            , targetBase(nullptr)
            , closestDistanceToTargetBase(INT_MAX)
            , lastForewardMotionFrame(0)
            , fixedPosition(BWAPI::TilePositions::Invalid)
            , hidingUntil(0) {}

    void update() override;

    void disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                 const std::function<void(const MyUnit &)> &movableUnitCallback) override;

    // Instructs the scout to hide in a corner of the enemy main until the given frame.
    void hideUntil(int frame);

private:
    MyUnit scout;
    Base *targetBase;
    int closestDistanceToTargetBase;
    int lastForewardMotionFrame;
    std::map<int, std::set<BWAPI::TilePosition>> scoutTiles;
    std::set<const BWEM::Area *> scoutAreas;
    int hidingUntil;

    bool reserveScout();

    bool pickInitialTargetBase();

    void updateTargetBase();

    bool isScoutBlocked();

    BWAPI::TilePosition getHighestPriorityScoutTile();
};
