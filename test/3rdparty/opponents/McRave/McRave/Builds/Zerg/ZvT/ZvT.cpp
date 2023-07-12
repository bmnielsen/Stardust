#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    void defaultZvT() {
        inOpeningBook =                             true;
        inBookSupply =                              true;
        wallNat =                                   hatchCount() >= 4;
        wallMain =                                  false;
        wantNatural =                               true;
        wantThird =                                 true;
        proxy =                                     false;
        hideTech =                                  false;
        playPassive =                               Util::getTime() > Time(3, 30) && !Spy::enemyFastExpand() && com(Zerg_Mutalisk) < 5;
        rush =                                      false;
        pressure =                                  false;
        transitionReady =                           false;
        planEarly =                                 false;
        gasTrick =                                  false;

        gasLimit =                                  gasMax();
        unitLimits[Zerg_Zergling] =                 lingsNeeded_ZvT();
        unitLimits[Zerg_Drone] =                    INT_MAX;

        desiredDetection =                          Zerg_Overlord;
        firstUpgrade =                              vis(Zerg_Zergling) >= 8 ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        firstTech =                                 TechTypes::None;
        firstUnit =                                 None;

        armyComposition[Zerg_Drone] =               0.60;
        armyComposition[Zerg_Zergling] =            0.40;
    }

    int lingsNeeded_ZvT() {
        if (com(Zerg_Spawning_Pool) == 0)
            return 0;
        if (Spy::getEnemyBuild() == "2Rax") {
            if (vis(Zerg_Sunken_Colony) == 0)
                return int(max(6.0, 1.5 * Players::getVisibleCount(PlayerState::Enemy, Terran_Marine)));
            return 6;
        }
        if (Spy::enemyRush())
            return 18;
        if (Spy::enemyProxy() || Spy::getEnemyOpener() == "8Rax" || Spy::getEnemyTransition() == "WorkerRush")
            return 10;
        if (Spy::getEnemyBuild() == "RaxFact" || Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0)
            return 4 + 6 * (Util::getTime() > Time(4, 00));
        if (Spy::enemyPressure() || Spy::getEnemyBuild() == "2Rax")
            return 6;
        return 2 + (4 * (Util::getTime() > Time(4, 00)));
    }

    void ZvT2HatchMuta()
    {
        lockedTransition =                              vis(Zerg_Lair) > 0;
        unitLimits[Zerg_Drone] =                        com(Zerg_Spawning_Pool) > 0 ? 28 : 15 - hatchCount();
        unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvT();
        gasLimit =                                      gasMax();

        inOpeningBook =                                 total(Zerg_Mutalisk) <= 9;
        firstUpgrade =                                  UpgradeTypes::None;
        firstUnit =                                     Zerg_Mutalisk;
        inBookSupply =                                  total(Zerg_Mutalisk) < 6;
        wantThird =                                     !Spy::enemyPressure() && !Spy::enemyRush() && Spy::getEnemyOpener() != "8Rax" && Spy::getEnemyBuild() != "RaxFact";
        planEarly =                                     wantThird && atPercent(Zerg_Lair, 0.5) && int(Stations::getStations(PlayerState::Self).size()) < 3;

        auto thirdHatch =  (total(Zerg_Mutalisk) >= 6) || (vis(Zerg_Drone) >= 20 && s >= 48 && vis(Zerg_Spire) > 0);

        buildQueue[Zerg_Hatchery] =                     2 + thirdHatch;
        buildQueue[Zerg_Extractor] =                    (hatchCount() >= 2 && vis(Zerg_Drone) >= 10) + (vis(Zerg_Spire) > 0);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 32) + (2 * atPercent(Zerg_Spire, 0.25));
        buildQueue[Zerg_Lair] =                         (s >= 24 && gas(80));
        buildQueue[Zerg_Spire] =                        atPercent(Zerg_Lair, 0.80);

        // Composition
        if (com(Zerg_Spire) == 0 && lingsNeeded_ZvT() > vis(Zerg_Zergling)) {
            armyComposition[Zerg_Drone] =               0.00;
            armyComposition[Zerg_Zergling] =            1.00;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            com(Zerg_Spire) > 0 ? 0.00 : 0.40;
            armyComposition[Zerg_Mutalisk] =            com(Zerg_Spire) > 0 ? 0.40 : 0.00;
        }
    }

    void ZvT3HatchMuta()
    {
        lockedTransition =                              vis(Zerg_Lair) > 0;
        unitLimits[Zerg_Drone] =                        com(Zerg_Spawning_Pool) > 0 ? 33 : 15 - hatchCount();
        unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvT();
        gasLimit =                                      gasMax();

        inOpeningBook =                                 total(Zerg_Mutalisk) <= 9;
        firstUpgrade =                                  (vis(Zerg_Extractor) >= 2 && gas(100)) ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        firstUnit =                                     Zerg_Mutalisk;
        inBookSupply =                                  vis(Zerg_Overlord) < 7 || total(Zerg_Mutalisk) < 9;
        wantThird =                                     true;

        auto fourthHatch =                              total(Zerg_Mutalisk) >= 9;
        planEarly =                                     ((Spy::enemyFastExpand() && s >= 60) || atPercent(Zerg_Lair, 0.6)) && wantThird && int(Stations::getStations(PlayerState::Self).size()) < 3;

        buildQueue[Zerg_Hatchery] =                     2 + (s >= 26) + fourthHatch;
        buildQueue[Zerg_Extractor] =                    (hatchCount() >= 3) + (s >= 44);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 32) + (s >= 48) + (atPercent(Zerg_Spire, 0.5) * 3);
        buildQueue[Zerg_Lair] =                         (s >= 24 && gas(80));
        buildQueue[Zerg_Spire] =                        (s >= 42 && atPercent(Zerg_Lair, 0.80));

        // Composition
        if (com(Zerg_Spire) == 0 && lingsNeeded_ZvT() > vis(Zerg_Zergling)) {
            armyComposition[Zerg_Drone] =               0.00;
            armyComposition[Zerg_Zergling] =            1.00;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            vis(Zerg_Spire) > 0 ? 0.10 : 0.40;
            armyComposition[Zerg_Mutalisk] =            vis(Zerg_Spire) > 0 ? 0.30 : 0.00;
        }
    }

    void ZvT2HatchSpeedling()
    {
        unitLimits[Zerg_Drone] =                        total(Zerg_Zergling) >= 12 ? 11 : 9;
        unitLimits[Zerg_Zergling] =                     INT_MAX;
        gasLimit =                                      ((!Spy::enemyProxy() || com(Zerg_Zergling) >= 6) && !lingSpeed()) ? capGas(100) : 0;

        wallNat =                                       false;
        inOpeningBook =                                 total(Zerg_Zergling) < 36;
        firstUpgrade =                                  UpgradeTypes::Metabolic_Boost;
        firstUnit =                                     UnitTypes::None;
        inBookSupply =                                  vis(Zerg_Overlord) < 3;
        rush =                                          true;

        // Build
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 26);
        buildQueue[Zerg_Extractor] =                    (hatchCount() >= 2 && vis(Zerg_Drone) >= 9);

        // Composition
        armyComposition[Zerg_Drone] =                   0.00;
        armyComposition[Zerg_Zergling] =                1.00;
    }

    void ZvT3HatchSpeedling()
    {
        unitLimits[Zerg_Drone] =                        13;
        unitLimits[Zerg_Zergling] =                     hatchCount() >= 3 ? INT_MAX : 0;
        gasLimit =                                      !lingSpeed() ? capGas(100) : 0;

        wallNat =                                       true;
        inOpeningBook =                                 total(Zerg_Zergling) < 80;
        firstUpgrade =                                  UpgradeTypes::Metabolic_Boost;
        firstUnit =                                     UnitTypes::None;
        inBookSupply =                                  vis(Zerg_Overlord) < 3;
        rush =                                          true;

        // Build
        buildQueue[Zerg_Hatchery] =                     1 + (s >= 22 && vis(Zerg_Spawning_Pool) > 0) + (s >= 26);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 26);
        buildQueue[Zerg_Extractor] =                    (s >= 24);

        // Composition
        armyComposition[Zerg_Drone] =                   0.20;
        armyComposition[Zerg_Zergling] =                0.80;
    }

    void ZvT()
    {
        defaultZvT();

        // Builds
        if (currentBuild == "HatchPool")
            ZvT_HP();
        if (currentBuild == "PoolHatch")
            ZvT_PH();

        // Reactions
        if (!lockedTransition) {
            if (Spy::enemyRush() || Spy::enemyProxy())
                currentTransition = "2HatchSpeedling";
            if (Spy::getEnemyOpener() == "8Rax") {
                currentBuild = "PoolHatch";
                currentOpener = "12Pool";
                currentTransition = "2HatchMuta";
            }
            if (Spy::getEnemyTransition() == "WorkerRush") {
                currentBuild = "PoolHatch";
                currentOpener = "Overpool";
                currentTransition = "2HatchSpeedling";
            }
        }

        // Transitions
        if (transitionReady) {
            if (currentTransition == "2HatchMuta")
                ZvT2HatchMuta();
            if (currentTransition == "3HatchMuta")
                ZvT3HatchMuta();
            if (currentTransition == "2HatchSpeedling")
                ZvT2HatchSpeedling();
            if (currentTransition == "3HatchSpeedling")
                ZvT3HatchSpeedling();
        }
    }
}