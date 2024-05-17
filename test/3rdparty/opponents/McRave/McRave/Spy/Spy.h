#pragma once
#include <BWAPI.h>

namespace McRave::Spy {

    struct Strat {
        bool possible = false;
        bool confirmed = false;
        bool changeable = false;
        int framesTrue = 0;
        int framesRequired = 50;
        int framesChangeable = 200;
        std::string name = "Unknown";

        void update() {
            (possible || name != "Unknown") ? framesTrue++ : framesTrue = 0;
            if (framesTrue > framesRequired)
                confirmed = true;
            else {
                possible = false;
                name = "Unknown";
            }
            changeable = framesTrue < framesChangeable;
        }
    };

    struct UnitTimings {
        std::vector<Time> countStartedWhen;
        std::vector<Time> countCompletedWhen;
        std::vector<Time> countArrivesWhen;
        Time firstStartedWhen;
        Time firstCompletedWhen;
        Time firstArrivesWhen;
    };

    struct StrategySpy { // TODO: Impl multiple players
        Strat build, opener, transition, expand, rush, wall, proxy, early, steal, pressure, greedy, invis;
        Time buildTime, openerTime, transitionTime, rushArrivalTime;
        std::vector<Strat*> listOfStrats;
        std::map<BWAPI::UnitType, UnitTimings> enemyTimings;
        std::map<BWAPI::UpgradeType, int> upgradeLevel;
        std::map<BWAPI::TechType, bool> techResearched; // TODO: Impl
        int workersNearUs = 0;
        int gasMined = 0;
        std::set<BWAPI::UnitType> typeUpgrading; // TODO: Better impl (doesn't look at current state)

        StrategySpy() {
            listOfStrats ={ &build, &opener, &transition, &expand, &rush, &wall, &proxy, &early, &steal, &pressure, &greedy, &invis };
            build.framesRequired = 100;
            build.framesChangeable = 200;
            opener.framesRequired = 100;
            opener.framesChangeable = 200;
            transition.framesRequired = 100;
            transition.framesChangeable = 200;
        }
    };

    namespace Protoss {
        void updateProtoss(StrategySpy&);
    }
    namespace Terran {
        void updateTerran(StrategySpy&);
    }
    namespace Zerg {
        void updateZerg(StrategySpy&);        
    }
    namespace General {
        void updateGeneral(StrategySpy&);
    }

    bool finishedSooner(BWAPI::UnitType, BWAPI::UnitType);
    bool startedEarlier(BWAPI::UnitType, BWAPI::UnitType);
    bool completesBy(int, BWAPI::UnitType, Time);
    bool arrivesBy(int, BWAPI::UnitType, Time);
    void onFrame();

    std::string getEnemyBuild();
    std::string getEnemyOpener();
    std::string getEnemyTransition();
    Time getEnemyBuildTime();
    Time getEnemyOpenerTime();
    Time getEnemyTransitionTime();
    bool enemyFastExpand();
    bool enemyRush();
    bool enemyInvis();
    bool enemyPossibleProxy();
    bool enemyProxy();
    bool enemyGasSteal();
    bool enemyPressure();
    bool enemyWalled();
    bool enemyGreedy();
    bool enemyBust();
    int getWorkersNearUs();
    int getEnemyGasMined();
}
