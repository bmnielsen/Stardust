// Written by jaj22

#pragma once
#include <BWAPI.h>

namespace BaseFinder
{
    struct Base
    {
        BWAPI::TilePosition tpos;
        BWAPI::Position pos;
        std::vector<BWAPI::Unit> minerals;
        std::vector<BWAPI::Unit> geysers;
        // also make list of depot-blocking units?
    };

    const std::vector<Base> &GetBases();

    void Init();
    void DrawStuff();
}
