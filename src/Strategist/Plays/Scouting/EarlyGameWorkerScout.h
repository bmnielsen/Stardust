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
            , lastForewardMotionFrame(0) {}

    void update() override;

    virtual void disband(const std::function<void(const MyUnit&)> &removedUnitCallback,
                         const std::function<void(const MyUnit&)> &movableUnitCallback) override;

private:
    MyUnit scout;
    Base *targetBase;
    int closestDistanceToTargetBase;
    int lastForewardMotionFrame;
    std::map<int, std::set<BWAPI::TilePosition>> scoutTiles;
    std::set<const BWEM::Area *> scoutAreas;

    bool reserveScout();

    bool pickInitialTargetBase();

    void updateTargetBase();

    bool isScoutBlocked();

    BWAPI::TilePosition getHighestPriorityScoutTile();
};
