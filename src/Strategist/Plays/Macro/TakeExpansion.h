#pragma once

#include "Play.h"

// Takes the next expansion.
class TakeExpansion : public Play
{
public:
    explicit TakeExpansion(BWAPI::TilePosition depotPosition);

    const char *label() const override { return "TakeExpansion"; }

    void update() override;

    void addMineralReservations(std::vector<std::pair<int, int>> &mineralReservations) override;

private:
    BWAPI::TilePosition depotPosition;
    BWAPI::Unit builder;
};
