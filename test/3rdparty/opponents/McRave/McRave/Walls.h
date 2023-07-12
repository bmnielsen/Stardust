#pragma once
#include <BWAPI.h>

namespace McRave::Walls {

    void onStart();
    void onFrame();
    
    int needGroundDefenses(BWEB::Wall&);
    int needAirDefenses(BWEB::Wall&);
    BWEB::Wall* getMainWall();
    BWEB::Wall* getNaturalWall();
}
