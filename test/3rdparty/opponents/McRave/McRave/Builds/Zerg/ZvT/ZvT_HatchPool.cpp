#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    namespace {

        void ZvT_HP_12Hatch()
        {
            transitionReady =                               vis(Zerg_Spawning_Pool) > 0;
            unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvT();
            unitLimits[Zerg_Drone] =                        12;
            gasLimit =                                      ((!Spy::enemyProxy() || com(Zerg_Zergling) >= 6) && !lingSpeed()) ? capGas(100) : 0;
            scout =                                         scout || (hatchCount() == 1 && s >= 22 && Util::getTime() > Time(1, 30));
            planEarly =                                     (hatchCount() == 1 && s >= 22 && Util::getTime() > Time(1, 30));

            buildQueue[Zerg_Hatchery] =                     1 + (s >= 24);
            buildQueue[Zerg_Spawning_Pool] =                (hatchCount() >= 2 && s >= 24);
            buildQueue[Zerg_Overlord] =                     1 + (s >= 16) + (s >= 32);

            // Composition
            armyComposition[Zerg_Drone] =                   0.60;
            armyComposition[Zerg_Zergling] =                0.40;
        }
    }

    void ZvT_HP()
    {
        // Openers
        if (currentOpener == "12Hatch")
            ZvT_HP_12Hatch();
    }
}