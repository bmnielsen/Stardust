#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    namespace {

        void ZvP_HP_12Hatch()
        {
            // 'https://liquipedia.net/starcraft/12_Hatch_(vs._Protoss)'
            transitionReady =                               vis(Zerg_Spawning_Pool) > 0;
            unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
            gasLimit =                                      0;
            scout =                                         scout || (hatchCount() == 1 && s >= 22 && Util::getTime() > Time(1, 30));
            wantNatural =                                   !Spy::enemyProxy();
            playPassive =                                   false;
            unitLimits[Zerg_Drone] =                        13 - vis(Zerg_Hatchery);
            planEarly =                                     !Spy::enemyProxy() && (hatchCount() == 1 && s >= 22 && Util::getTime() > Time(1, 30));

            buildQueue[Zerg_Hatchery] =                     1 + (s >= 24);
            buildQueue[Zerg_Spawning_Pool] =                hatchCount() >= 2;
            buildQueue[Zerg_Overlord] =                     1 + (s >= 16) + (s >= 30);
        }

        void ZvP_HP_10Hatch()
        {
            // 'https://liquipedia.net/starcraft/10_Hatch'
            transitionReady =                               com(Zerg_Spawning_Pool) > 0;
            unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
            gasLimit =                                      0;
            scout =                                         scout || (hatchCount() == 1 && s == 20 && Broodwar->self()->minerals() >= 150);
            wantNatural =                                   !Spy::enemyProxy();
            playPassive =                                   false;
            unitLimits[Zerg_Drone] =                        10;
            planEarly =                                     !Spy::enemyProxy() && hatchCount() == 1 && s == 20 && Broodwar->self()->minerals() >= 150;
            gasTrick =                                      s >= 18 && hatchCount() < 2 && total(Zerg_Spawning_Pool) == 0;

            buildQueue[Zerg_Hatchery] =                     1 + (s >= 20);
            buildQueue[Zerg_Spawning_Pool] =                hatchCount() >= 2;
            buildQueue[Zerg_Overlord] =                     1 + (s >= 18 && vis(Zerg_Spawning_Pool) > 0) + (s >= 36);
        }
    }

    void ZvP_HP()
    {
        // Openers
        if (currentOpener == "10Hatch")
            ZvP_HP_10Hatch();
        if (currentOpener == "12Hatch")
            ZvP_HP_12Hatch();
    }
}