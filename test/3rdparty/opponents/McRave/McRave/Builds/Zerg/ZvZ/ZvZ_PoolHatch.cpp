#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    namespace {

        void ZvZ_PH_2HatchMuta()
        {
            inOpeningBook =                                 total(Zerg_Mutalisk) < 3;
            lockedTransition =                              vis(Zerg_Lair) > 0;
            unitLimits[Zerg_Drone] =                        20;
            gasLimit =                                      (lingSpeed() && com(Zerg_Lair) == 0) ? 1 : gasMax();
            unitLimits[Zerg_Zergling] =                     18;

            firstUnit =                                     Zerg_Mutalisk;
            inBookSupply =                                  vis(Zerg_Overlord) < 3;
            playPassive =                                   com(Zerg_Mutalisk) == 0 && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) > total(Zerg_Zergling);

            // Build
            buildQueue[Zerg_Extractor] =                    (hatchCount() >= 2 && vis(Zerg_Drone) >= 9) + (atPercent(Zerg_Spire, 0.5));
            buildQueue[Zerg_Lair] =                         gas(100) && vis(Zerg_Spawning_Pool) > 0 && total(Zerg_Zergling) >= 12 && vis(Zerg_Drone) >= 8;
            buildQueue[Zerg_Spire] =                        lingSpeed() && atPercent(Zerg_Lair, 0.8) && vis(Zerg_Drone) >= 9;
            buildQueue[Zerg_Overlord] =                     1 + (vis(Zerg_Extractor) >= 1) + (s >= 32) + (atPercent(Zerg_Spire, 0.5) && s >= 38);

            // Army Composition
            if (com(Zerg_Spire)) {
                armyComposition[Zerg_Drone] =               0.40;
                armyComposition[Zerg_Zergling] =            0.10;
                armyComposition[Zerg_Mutalisk] =            0.50;
            }
            else {
                armyComposition[Zerg_Drone] =               0.50;
                armyComposition[Zerg_Zergling] =            0.50;
            }
        }

        void ZvZ_PH_Overpool()
        {
            transitionReady =                               total(Zerg_Zergling) >= 6 || (Spy::enemyFastExpand() && com(Zerg_Spawning_Pool) > 0);
            unitLimits[Zerg_Drone] =                        Spy::enemyFastExpand() ? 16 : 10;
            unitLimits[Zerg_Zergling] =                     6;
            gasLimit =                                      capGas(100);
            playPassive =                                   (com(Zerg_Mutalisk) == 0 && Spy::enemyRush()) || (Spy::getEnemyOpener() == "9Pool" && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 8 && !Spy::enemyRush() && !Spy::enemyPressure() && total(Zerg_Mutalisk) == 0);

            buildQueue[Zerg_Hatchery] =                     1 + (s >= 22);
            buildQueue[Zerg_Spawning_Pool] =                (vis(Zerg_Overlord) >= 2);
            buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 32);
        }

        void ZvZ_PH_12Pool()
        {
            transitionReady =                               total(Zerg_Zergling) >= 6;
            unitLimits[Zerg_Drone] =                        vis(Zerg_Extractor) > 0 ? 9 : 12;
            unitLimits[Zerg_Zergling] =                     12;
            gasLimit =                                      com(Zerg_Drone) >= 10 ? gasMax() : 0;
            firstUpgrade =                                  vis(Zerg_Zergling) >= 6 ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
            playPassive =                                   total(Zerg_Zergling) < 16;

            buildQueue[Zerg_Hatchery] =                     1 + (s >= 22 && vis(Zerg_Extractor) > 0);
            buildQueue[Zerg_Spawning_Pool] =                s >= 24;
            buildQueue[Zerg_Extractor] =                    (s >= 24 && vis(Zerg_Spawning_Pool) > 0);
            buildQueue[Zerg_Overlord] =                     1 + (s >= 18);
        }
    }

    void ZvZ_PH()
    {
        // Openers
        if (currentOpener == "Overpool")
            ZvZ_PH_Overpool();
        if (currentOpener == "12Pool")
            ZvZ_PH_12Pool();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "2HatchMuta")
                ZvZ_PH_2HatchMuta();
        }
    }
}