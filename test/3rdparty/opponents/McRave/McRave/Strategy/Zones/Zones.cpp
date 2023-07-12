#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Zones
{
    namespace {

        vector<Zone> zones;

        void updateZones()
        {
            for (auto &zone : zones)
                zone.duration--;

            if (!zones.empty()) {
                zones.erase(remove_if(zones.begin(), zones.end(), [&](auto &zone) {
                    return zone.duration <= 0;
                }), zones.end());
            }
        }

        void drawZones()
        {
            return;
            for (auto &zone : zones) {
                if (zone.type == ZoneType::Engage)
                    Broodwar->drawCircleMap(zone.position, zone.radius, Colors::Green);
                if (zone.type == ZoneType::Defend)
                    Broodwar->drawCircleMap(zone.position, zone.radius, Colors::Blue);
                if (zone.type == ZoneType::Retreat)
                    Broodwar->drawCircleMap(zone.position, zone.radius, Colors::Red);
            }
        }
    }

    void onFrame()
    {
        updateZones();
        drawZones();
    }

    void addZone(Position position, ZoneType zone, int frames, int radius)
    {
        if (!zones.empty()) {
            zones.erase(remove_if(zones.begin(), zones.end(), [&](auto &zone) {
                return zone.position == position;
            }), zones.end());
        }

        auto newZone = Zone(position, zone, frames, radius);
        zones.push_back(newZone);
    }

    ZoneType getZone(Position p)
    {
        for (auto &zone : zones) {
            if (p.getDistance(zone.position) < zone.radius)
                return zone.type;
        }
        return ZoneType::None;
    }
}
