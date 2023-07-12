#include "..\McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Spy {

    namespace {
        StrategySpy theSpy; // TODO: Impl multiple races and players
    }

    bool finishedSooner(UnitType t1, UnitType t2)
    {
        if (theSpy.enemyTimings.find(t1) == theSpy.enemyTimings.end())
            return false;
        return (theSpy.enemyTimings.find(t2) == theSpy.enemyTimings.end()) || (theSpy.enemyTimings[t1].firstCompletedWhen < theSpy.enemyTimings[t2].firstCompletedWhen);
    }

    bool startedEarlier(UnitType t1, UnitType t2)
    {
        if (theSpy.enemyTimings.find(t1) == theSpy.enemyTimings.end())
            return false;
        return (theSpy.enemyTimings.find(t2) == theSpy.enemyTimings.end()) || (theSpy.enemyTimings[t1].firstStartedWhen < theSpy.enemyTimings[t2].firstStartedWhen);
    }

    bool completesBy(int count, UnitType type, Time beforeThis)
    {
        int current = 0;
        for (auto &time : theSpy.enemyTimings[type].countCompletedWhen) {
            if (time <= beforeThis)
                current++;
        }
        return current >= count;
    }

    bool arrivesBy(int count, UnitType type, Time beforeThis)
    {
        int current = 0;
        for (auto &time : theSpy.enemyTimings[type].countArrivesWhen) {
            if (time <= beforeThis)
                current++;
        }
        return current >= count;
    }

    Time whenArrival(int count, UnitType type)
    {
        auto timeCount = Time(0, 00);
        auto times = theSpy.enemyTimings[type].countArrivesWhen;
        if (times.size() >= count) {
            for (auto &time : times) {
                if (time > timeCount)
                    timeCount = time;
            }
        }
        return timeCount;
    }
    
    void onFrame()
    {
        if (Players::vFFA())
            return;

        General::updateGeneral(theSpy);
        Protoss::updateProtoss(theSpy);
        Terran::updateTerran(theSpy);
        Zerg::updateZerg(theSpy);

        // Set a timestamp for when we detected a piece of the enemy build order
        if (theSpy.build.confirmed && theSpy.buildTime == Time(999, 00))
            theSpy.buildTime = Util::getTime();
        if (theSpy.opener.confirmed && theSpy.openerTime == Time(999, 00))
            theSpy.openerTime = Util::getTime();
        if (theSpy.transition.confirmed && theSpy.transitionTime == Time(999, 00))
            theSpy.transitionTime = Util::getTime();
    }

    string getEnemyBuild() { return theSpy.build.name; }
    string getEnemyOpener() { return theSpy.opener.name; }
    string getEnemyTransition() { return theSpy.transition.name; }
    Time getEnemyBuildTime() { return theSpy.buildTime; }
    Time getEnemyOpenerTime() { return theSpy.openerTime; }
    Time getEnemyTransitionTime() { return theSpy.transitionTime; }
    bool enemyFastExpand() { return theSpy.expand.confirmed; }
    bool enemyRush() { return theSpy.rush.confirmed; }
    bool enemyInvis() { return theSpy.invis.confirmed; }
    bool enemyPossibleProxy() { return theSpy.early.confirmed; }
    bool enemyProxy() { return theSpy.proxy.confirmed; }
    bool enemyGasSteal() { return theSpy.steal.confirmed; }
    bool enemyPressure() { return theSpy.pressure.confirmed; }
    bool enemyWalled() { return theSpy.wall.confirmed; }
    bool enemyGreedy() { return theSpy.greedy.confirmed; }
    bool enemyBust() { return theSpy.transition.name.find("HatchHydra") != string::npos; }
    int getWorkersNearUs() { return theSpy.workersNearUs; }
    int getEnemyGasMined() { return theSpy.gasMined; }
}