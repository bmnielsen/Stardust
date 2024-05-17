#pragma once
#include <BWAPI.h>

namespace McRave::Zones
{
    struct Zone {
        BWAPI::Position position;
        ZoneType type;
        int duration, radius;

        Zone(BWAPI::Position _p, ZoneType _t, int _d, int _r) {
            position = _p;
            type = _t;
            duration = _d;
            radius = _r;
        }
    };

    void onFrame();

    void addZone(BWAPI::Position, ZoneType, int, int);

    ZoneType getZone(BWAPI::Position);
}
