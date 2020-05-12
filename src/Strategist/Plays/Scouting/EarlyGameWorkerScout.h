#pragma once

#include "Play.h"

class EarlyGameWorkerScout : public Play
{
public:
    EarlyGameWorkerScout() : Play("EarlyGameWorkerScout"), scout(nullptr), targetBase(nullptr) {}

    void update() override;

private:
    MyUnit scout;
    Base *targetBase;
    std::map<int, std::set<BWAPI::TilePosition>> scoutTiles;
    std::set<const BWEM::Area *> scoutAreas;

    bool reserveScout();

    bool pickInitialTargetBase();

    void updateTargetBase();

    BWAPI::TilePosition getHighestPriorityScoutTile();
};
