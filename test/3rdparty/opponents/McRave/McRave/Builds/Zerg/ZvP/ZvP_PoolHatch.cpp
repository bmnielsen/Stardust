#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    namespace {

        void ZvP_PH_Overpool()
        {
            // 'https://liquipedia.net/starcraft/Overpool_(vs._Protoss)'
            transitionReady =                               hatchCount() >= 2;
            unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
            gasLimit =                                      0;
            scout =                                         scout || (hatchCount() >= 2);
            wantNatural =                                   !Spy::enemyProxy();
            playPassive =                                   false;

            if (Spy::enemyFastExpand())
                unitLimits[Zerg_Drone] =                    INT_MAX;
            else
                unitLimits[Zerg_Drone] =                    12 - vis(Zerg_Hatchery);

            buildQueue[Zerg_Hatchery] =                     1 + (s >= 22 && vis(Zerg_Spawning_Pool) > 0 && (!Spy::enemyProxy() || com(Zerg_Sunken_Colony) >= 2));
            buildQueue[Zerg_Spawning_Pool] =                (vis(Zerg_Overlord) >= 2);
            buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 30);
        }

        void ZvP_PH_4Pool()
        {
            // 'https://liquipedia.net/starcraft/5_Pool_(vs._Protoss)'
            transitionReady =                               total(Zerg_Zergling) >= 24;
            unitLimits[Zerg_Drone] =                        4;
            unitLimits[Zerg_Zergling] =                     INT_MAX;
            gasLimit =                                      0;
            wantNatural =                                   !Spy::enemyProxy();
            scout =                                         scout || (vis(Zerg_Spawning_Pool) > 0 && com(Zerg_Drone) >= 4 && !Terrain::getEnemyStartingPosition().isValid());
            rush =                                          true;

            buildQueue[Zerg_Hatchery] =                     1;
            buildQueue[Zerg_Spawning_Pool] =                s >= 8;
            buildQueue[Zerg_Overlord] =                     1 + (s >= 18);
        }

        void ZvP_PH_9Pool()
        {
            // 'https://liquipedia.net/starcraft/9_Pool_(vs._Protoss)'
            transitionReady =                               hatchCount() >= 2;
            unitLimits[Zerg_Drone] =                        Spy::enemyFastExpand() ? INT_MAX : (12 - hatchCount());
            unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
            gasLimit =                                      0;
            gasTrick =                                      vis(Zerg_Spawning_Pool) > 0 && total(Zerg_Overlord) < 2;
            scout =                                         scout || (vis(Zerg_Spawning_Pool) > 0 && s >= 22);
            wantNatural =                                   !Spy::enemyProxy();
            playPassive =                                   false;

            buildQueue[Zerg_Hatchery] =                     1 + (s >= 20 && vis(Zerg_Spawning_Pool) > 0 && atPercent(Zerg_Spawning_Pool, 0.8 && total(Zerg_Zergling) >= 6) && vis(Zerg_Overlord) >= 2 && (!Spy::enemyProxy() || vis(Zerg_Sunken_Colony) >= 2));
            buildQueue[Zerg_Spawning_Pool] =                s >= 18;
            buildQueue[Zerg_Overlord] =                     1 + (s >= 20 && vis(Zerg_Spawning_Pool) > 0) + (s >= 30);
        }
    }

    void ZvP_PH()
    {
        // Openers
        if (currentOpener == "Overpool")
            ZvP_PH_Overpool();
        if (currentOpener == "4Pool")
            ZvP_PH_4Pool();
        if (currentOpener == "9Pool")
            ZvP_PH_9Pool();
    }
}