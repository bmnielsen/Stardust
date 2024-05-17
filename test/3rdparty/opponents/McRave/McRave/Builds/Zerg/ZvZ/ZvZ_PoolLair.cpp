#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    namespace {

        void ZvZ_PL_1HatchMuta()
        {
            inOpeningBook =                                 total(Zerg_Mutalisk) < 4;
            lockedTransition =                              vis(Zerg_Lair) > 0;
            unitLimits[Zerg_Drone] =                        atPercent(Zerg_Lair, 0.50) ? 12 : 9;
            unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvZ();
            gasLimit =                                      (lingSpeed() && total(Zerg_Lair) == 0) ? 2 : gasMax();
            firstUnit =                                     Zerg_Mutalisk;
            inBookSupply =                                   total(Zerg_Mutalisk) < 3;

            auto secondHatch = (Spy::getEnemyTransition() == "1HatchMuta" && total(Zerg_Mutalisk) >= 4)
                || (Players::getVisibleCount(PlayerState::Enemy, Zerg_Sunken_Colony) >= 2 && Util::getTime() < Time(3, 30));
            wantNatural = secondHatch;

            // Build
            buildQueue[Zerg_Lair] =                         gas(100) && vis(Zerg_Spawning_Pool) > 0 && total(Zerg_Zergling) >= 6 && vis(Zerg_Drone) >= 8;
            buildQueue[Zerg_Spire] =                        lingSpeed() && atPercent(Zerg_Lair, 0.95) && vis(Zerg_Drone) >= 9;
            buildQueue[Zerg_Hatchery] =                     1 + secondHatch;
            buildQueue[Zerg_Overlord] =                     (com(Zerg_Spire) == 0 && atPercent(Zerg_Spire, 0.35) && (s >= 34)) ? 4 : 1 + (vis(Zerg_Extractor) >= 1) + (s >= 32) + (s >= 46);

            // Army Composition
            if (com(Zerg_Spire) > 0) {
                armyComposition[Zerg_Drone] =               0.40;
                armyComposition[Zerg_Zergling] =            0.00;
                armyComposition[Zerg_Mutalisk] =            0.60;
            }
            else {
                armyComposition[Zerg_Drone] =               0.50;
                armyComposition[Zerg_Zergling] =            0.50;
            }
        }

        void ZvZ_PL_9Pool()
        {
            transitionReady =                               lingSpeed();
            unitLimits[Zerg_Drone] =                        9 - (vis(Zerg_Extractor) > 0) + (vis(Zerg_Overlord) > 1);
            unitLimits[Zerg_Zergling] =                     Spy::enemyRush() ? INT_MAX : 10;
            gasLimit =                                      (Spy::enemyRush() && com(Zerg_Sunken_Colony) == 0) ? 0 : gasMax();
            playPassive =                                   (com(Zerg_Mutalisk) == 0 && Spy::enemyRush())
                                                            || (Spy::getEnemyOpener() == "9Pool" && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 8 && !Spy::enemyRush() && !Spy::enemyPressure() && total(Zerg_Mutalisk) == 0)
                                                            || (Broodwar->getStartLocations().size() >= 3 && Util::getTime() < Time(3,00) && !Terrain::getEnemyStartingPosition().isValid());

            buildQueue[Zerg_Spawning_Pool] =                s >= 18;
            buildQueue[Zerg_Extractor] =                    s >= 18 && vis(Zerg_Spawning_Pool) > 0;
            buildQueue[Zerg_Overlord] =                     1 + (vis(Zerg_Extractor) >= 1) + (s >= 32);
        }
    }

    void ZvZ_PL()
    {
        // Openers
        if (currentOpener == "9Pool")
            ZvZ_PL_9Pool();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "1HatchMuta")
                ZvZ_PL_1HatchMuta();
        }
    }
}