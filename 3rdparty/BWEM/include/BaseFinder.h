// License: "I have a pretty solid one-file C++ basefinder if someone needs one" - jaj22 on SSCAIT Discord

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
    void DrawStuff(BWAPI::TilePosition mintile, BWAPI::TilePosition maxtile);
}