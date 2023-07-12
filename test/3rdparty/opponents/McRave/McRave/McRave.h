#pragma once
#include <BWAPI.h>
#include "bwem.h"
#include "BWEB.h"

namespace McRave
{
    class UnitInfo;
    class ResourceInfo;
    class PlayerInfo;

    /// Writes to a file, very slow function!
    static void easyWrite(std::string& stuff)
    {
        std::ofstream writeFile;
        writeFile.open("bwapi-data/write/McRave_Debug_Log.txt", std::ios::app);
        writeFile << stuff << std::endl;
    }

    struct Time {
        int minutes;
        int seconds;
        int frames;
        Time(int f) {
            frames = f;
            minutes = int(double(f) / 1428.6);
            seconds = int(double(f) / 23.81) % 60;
        }
        Time(int m, int s) {
            minutes = m, seconds = s;
            frames = int(23.81 * ((double(m) * 60.0) + double(s)));
        }
        Time() {
            minutes = 999;
            seconds = 0;
        }
        std::string toString() {
            return std::to_string(minutes) + ":" + (seconds < 10 ? "0" + std::to_string(seconds) : std::to_string(seconds));
        }

        bool operator< (const Time t2) {
            return (minutes < t2.minutes)
                || (minutes == t2.minutes && seconds < t2.seconds);
        }

        bool operator<= (const Time t2) {
            return (minutes < t2.minutes)
                || (minutes == t2.minutes && seconds <= t2.seconds);
        }

        bool operator> (const Time t2) {
            return (minutes > t2.minutes)
                || (minutes == t2.minutes && seconds > t2.seconds);
        }

        bool operator>= (const Time t2) {
            return (minutes > t2.minutes)
                || (minutes == t2.minutes && seconds >= t2.seconds);
        }

        bool operator== (const Time t2) {
            return minutes == t2.minutes && seconds == t2.seconds;
        }

        Time operator- (const Time t2) {
            auto secondsDiff = (minutes * 60 + seconds) - (t2.minutes * 60 + t2.seconds);
            return Time(secondsDiff / 60, secondsDiff % 60);
        }
    };

    inline BWAPI::GameWrapper& operator<<(BWAPI::GameWrapper& bw, const Time& t) {
        bw << t.minutes << ":";
        t.seconds < 10 ? bw << "0" << t.seconds : bw << t.seconds;
        bw << std::endl;
        return bw;
    }

    struct Strength {
        double airToAir = 0.0;
        double airToGround = 0.0;
        double groundToAir = 0.0;
        double groundToGround = 0.0;
        double airDefense = 0.0;
        double groundDefense = 0.0;

        void operator+= (const Strength &second) {
            airToAir += second.airToAir;
            airToGround += second.airToGround;
            groundToAir += second.groundToAir;
            groundToGround += second.groundToGround;
            airDefense += second.airDefense;
            groundDefense += second.groundDefense;
        }

        void clear() {
            airToAir = 0.0;
            airToGround = 0.0;
            groundToAir = 0.0;
            groundToGround = 0.0;
            airDefense = 0.0;
            groundDefense = 0.0;
        }
    };

    enum class ZoneType {
        None, Engage, Retreat, Defend
    };

    enum class GoalType {
        None, Attack, Contain, Explore, Escort, Defend
    };

    enum class Role {
        None, Worker, Combat, Transport, Scout, Production, Defender, Support
    };

    enum class TransportState {
        None, Loading, Unloading, Monitoring, Engaging, Retreating
    };

    enum class ResourceState {
        None, Assignable, Mineable, Stealable
    };

    enum class GlobalState {
        None, Attack, Retreat, Hold
    };

    enum class LocalState {
        None, Attack, Retreat, Hold
    };

    enum class SimState {
        None, Win, Loss
    };

    enum class PlayerState {
        None, Self, Ally, Enemy, Neutral
    };


    inline double logLookup16[1024]={};
}

#include "Horizon.h"
#include "Actions.h"
#include "Buildings.h"
#include "BuildOrder.h"
#include "Commands.h"
#include "Micro/Combat/Combat.h"
#include "Defender.h"
#include "Expansions.h"
#include "Goals.h"
#include "Grids.h"
#include "Stations.h"
#include "Workers.h"
#include "Learning.h"
#include "Pathing.h"
#include "Planning.h"
#include "PlayerInfo.h"
#include "Players.h"
#include "Production.h"
#include "Pylons.h"
#include "ResourceInfo.h"
#include "Resources.h"
#include "Roles.h"
#include "Scouts.h"
#include "Spy/Spy.h"
#include "Support.h"
#include "Targeting.h"
#include "Terrain.h"
#include "Transports.h"
#include "Units.h"
#include "UnitMath.h"
#include "UnitInfo.h"
#include "Util.h"
#include "Visuals.h"
#include "Walls.h"
#include "Strategy/Zones/Zones.h"

namespace
{
    auto &mapBWEM = BWEM::Map::Instance();
}