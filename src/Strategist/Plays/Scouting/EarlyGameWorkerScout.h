#pragma once

#include "Play.h"

class EarlyGameWorkerScout : public Play
{
public:
    [[nodiscard]] const char *label() const override { return "EarlyGameWorkerScout"; }

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
