#pragma once

#include "Play.h"

// Takes the next expansion.
class TakeExpansion : public Play
{
public:
    explicit TakeExpansion(BWAPI::TilePosition depotPosition);

    const char *label() const override { return "TakeExpansion"; }

    void update() override;

    void cancel();

    BWAPI::TilePosition depotPosition;

private:
    BWAPI::Unit builder;
};
