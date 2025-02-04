#pragma once

#include "MapSpecificOverride.h"

class NeoSylphid : public MapSpecificOverride
{
public:
    std::vector<BWAPI::Position> startingWorkerPositions(BWAPI::TilePosition startPosition) override
    {
        if (startPosition != BWAPI::TilePosition(62, 6)) return MapSpecificOverride::startingWorkerPositions(startPosition);

        return {
            BWAPI::Position(2038,184),
            BWAPI::Position(2062,184),
            BWAPI::Position(2090,184),
            BWAPI::Position(2120,192)
        };
    }
};
