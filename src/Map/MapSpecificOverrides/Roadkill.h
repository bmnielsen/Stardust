#pragma once

#include "MapSpecificOverride.h"

class Roadkill : public MapSpecificOverride
{
public:
    std::vector<BWAPI::Position> startingWorkerPositions(BWAPI::TilePosition startPosition) override
    {
        if (startPosition != BWAPI::TilePosition(69, 6)) return MapSpecificOverride::startingWorkerPositions(startPosition);

        return {
                BWAPI::Position(2318, 184),
                BWAPI::Position(2294, 184),
                BWAPI::Position(2270, 184),
                BWAPI::Position(2246, 184)
        };
    }
};
