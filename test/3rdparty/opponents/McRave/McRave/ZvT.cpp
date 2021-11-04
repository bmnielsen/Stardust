#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

namespace McRave::BuildOrder::Zerg {

    namespace {

        bool lingSpeed() {
            return Broodwar->self()->isUpgrading(UpgradeTypes::Metabolic_Boost) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Metabolic_Boost);
        }

        bool gas(int amount) {
            return Broodwar->self()->gas() >= amount;
        }

        int gasMax() {
            return Resources::getGasCount() * 3;
        }

        int hatchCount() {
            return vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive);
        }

        int capGas(int value) {
            auto onTheWay = 0;
            for (auto &w : Units::getUnits(PlayerState::Self)) {
                auto &worker = *w;
                if (worker.unit()->isCarryingGas() || (worker.hasResource() && worker.getResource().getType().isRefinery()))
                    onTheWay+=8;
            }

            return int(round(double(value - Broodwar->self()->gas() - onTheWay) / 8.0));
        }

        int colonyCount() {
            return vis(Zerg_Creep_Colony) + vis(Zerg_Sunken_Colony);
        }

        int lingsNeeded() {
            if (Strategy::getEnemyOpener() == "1RaxFE")
                return 0;
            if (Strategy::getEnemyBuild() == "2Rax") {
                if (vis(Zerg_Sunken_Colony) == 0)
                    return int(max(6.0, 1.5 * Players::getVisibleCount(PlayerState::Enemy, Terran_Marine)));
                return 6;
            }
            if (Strategy::enemyRush())
                return 18;
            if (Strategy::enemyProxy() || Strategy::getEnemyOpener() == "8Rax" || Strategy::getEnemyTransition() == "WorkerRush")
                return 10;
            if (Strategy::getEnemyBuild() == "RaxFact" || Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0)
                return Util::getTime() > Time(3, 00) ? 6 : 0;
            if (Strategy::enemyPressure() || Strategy::getEnemyBuild() == "2Rax")
                return 6;
            return 4;
        }

        void defaultZvT() {
            inOpeningBook =                                 true;
            inBookSupply =                                  true;
            wallNat =                                       hatchCount() >= 4;
            wallMain =                                      false;
            scout =                                         false;
            wantNatural =                                   true;
            wantThird =                                     true;
            proxy =                                         false;
            hideTech =                                      false;
            playPassive =                                   Util::getTime() > Time(3, 30) && !Strategy::enemyFastExpand() && com(Zerg_Mutalisk) < 5;
            rush =                                          false;
            pressure =                                      false;
            cutWorkers =                                    false;
            transitionReady =                               false;

            gasLimit =                                      gasMax();
            unitLimits[Zerg_Zergling] =                     lingsNeeded();
            unitLimits[Zerg_Drone] =                        INT_MAX;

            desiredDetection =                              Zerg_Overlord;
            firstUpgrade =                                  vis(Zerg_Zergling) >= 8 ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
            firstTech =                                     TechTypes::None;
            firstUnit =                                     None;

            armyComposition[Zerg_Drone] =                   0.60;
            armyComposition[Zerg_Zergling] =                0.40;
        }
    }

    void ZvT2HatchMuta()
    {
        lockedTransition =                          vis(Zerg_Lair) > 0;
        unitLimits[Zerg_Drone] =                    28;
        unitLimits[Zerg_Zergling] =                 lingsNeeded();
        gasLimit =                                  gasMax();

        inOpeningBook =                             total(Zerg_Mutalisk) <= 9;
        firstUpgrade =                              UpgradeTypes::None;
        firstUnit =                                 Zerg_Mutalisk;
        inBookSupply =                              total(Zerg_Mutalisk) < 6;
        planEarly =                                 atPercent(Zerg_Lair, 0.5) && int(Stations::getMyStations().size()) < 3;

        buildQueue[Zerg_Hatchery] =                 2 + (total(Zerg_Mutalisk) >= 6);
        buildQueue[Zerg_Extractor] =                (hatchCount() >= 2 && vis(Zerg_Drone) >= 10) + (atPercent(Zerg_Spire, 0.1 + (0.05 * colonyCount())));
        buildQueue[Zerg_Overlord] =                 1 + (s >= 18) + (s >= 32) + (2 * atPercent(Zerg_Spire, 0.25));
        buildQueue[Zerg_Lair] =                     (vis(Zerg_Drone) >= 10 && gas(80));
        buildQueue[Zerg_Spire] =                    atPercent(Zerg_Lair, 0.80);

        // Composition
        if (Strategy::getEnemyBuild() == "2Rax" && !Strategy::enemyFastExpand() && (com(Zerg_Sunken_Colony) == 0 || com(Zerg_Drone) < 9)) {
            armyComposition[Zerg_Drone] =               0.20;
            armyComposition[Zerg_Zergling] =            0.80;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            com(Zerg_Spire) > 0 ? 0.00 : 0.40;
            armyComposition[Zerg_Mutalisk] =            com(Zerg_Spire) > 0 ? 0.30 : 0.00;
        }
    }

    void ZvT3HatchMuta()
    {
        lockedTransition =                          vis(Zerg_Lair) > 0;
        unitLimits[Zerg_Drone] =                    33;
        unitLimits[Zerg_Zergling] =                 lingsNeeded();
        gasLimit =                                  com(Zerg_Drone) >= 11 ? gasMax() : 0;

        inOpeningBook =                             total(Zerg_Mutalisk) <= 9;
        firstUpgrade =                              (vis(Zerg_Extractor) >= 2 && gas(100)) ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        firstUnit =                                 Zerg_Mutalisk;
        inBookSupply =                              vis(Zerg_Overlord) < 7 || total(Zerg_Mutalisk) < 9;
        
        auto fourthHatch =                          (Strategy::enemyFastExpand() && s >= 66) || total(Zerg_Mutalisk) >= 9;
        auto planEarly =                            (Strategy::enemyFastExpand() && s >= 60) || atPercent(Zerg_Lair, 0.6);

        buildQueue[Zerg_Hatchery] =                 2 + (s >= 26) + fourthHatch;
        buildQueue[Zerg_Extractor] =                (hatchCount() >= 2 && vis(Zerg_Drone) >= 13) + (s >= 44);
        buildQueue[Zerg_Overlord] =                 1 + (s >= 18) + (s >= 32) + (s >= 48) + (atPercent(Zerg_Spire, 0.5) * 3);
        buildQueue[Zerg_Lair] =                     (vis(Zerg_Drone) >= 16 && gas(100));
        buildQueue[Zerg_Spire] =                    (s >= 42 && atPercent(Zerg_Lair, 0.80));

        // Composition
        if (Strategy::getEnemyBuild() == "2Rax" && !Strategy::enemyFastExpand() && (com(Zerg_Sunken_Colony) == 0 || com(Zerg_Drone) < 9)) {
            armyComposition[Zerg_Drone] =               0.20;
            armyComposition[Zerg_Zergling] =            0.80;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            vis(Zerg_Spire) > 0 ? 0.10 : 0.40;
            armyComposition[Zerg_Mutalisk] =            vis(Zerg_Spire) > 0 ? 0.30 : 0.00;
        }
    }

    void ZvT2HatchSpeedling()
    {
        unitLimits[Zerg_Drone] =                    total(Zerg_Zergling) >= 12 ? 11 : 9;
        unitLimits[Zerg_Zergling] =                 INT_MAX;
        gasLimit =                                  ((!Strategy::enemyProxy() || com(Zerg_Zergling) >= 6) && !lingSpeed()) ? capGas(100) : 0;

        wallNat =                                   false;
        inOpeningBook =                             total(Zerg_Zergling) < 36;
        firstUpgrade =                              UpgradeTypes::Metabolic_Boost;
        firstUnit =                                 UnitTypes::None;
        inBookSupply =                              vis(Zerg_Overlord) < 3;
        rush =                                      true;

        // Build
        buildQueue[Zerg_Overlord] =                 1 + (s >= 18) + (s >= 26);
        buildQueue[Zerg_Extractor] =                (hatchCount() >= 2 && vis(Zerg_Drone) >= 9);

        // Composition
        armyComposition[Zerg_Drone] =               0.00;
        armyComposition[Zerg_Zergling] =            1.00;
    }

    void ZvT3HatchSpeedling()
    {
        unitLimits[Zerg_Drone] =                    13;
        unitLimits[Zerg_Zergling] =                 hatchCount() >= 3 ? INT_MAX : 0;
        gasLimit =                                  !lingSpeed() ? capGas(100) : 0;

        wallNat =                                   true;
        inOpeningBook =                             total(Zerg_Zergling) < 80;
        firstUpgrade =                              UpgradeTypes::Metabolic_Boost;
        firstUnit =                                 UnitTypes::None;
        inBookSupply =                              vis(Zerg_Overlord) < 3;
        rush =                                      true;

        // Build
        buildQueue[Zerg_Hatchery] =                 1 + (s >= 22 && vis(Zerg_Spawning_Pool) > 0) + (s >= 26);
        buildQueue[Zerg_Overlord] =                 1 + (s >= 18) + (s >= 26);
        buildQueue[Zerg_Extractor] =                (s >= 24);

        // Composition
        armyComposition[Zerg_Drone] =               0.20;
        armyComposition[Zerg_Zergling] =            0.80;
    }

    void ZvT12Hatch()
    {
        transitionReady =                               vis(Zerg_Spawning_Pool) > 0;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        unitLimits[Zerg_Drone] =                        14;
        gasLimit =                                      ((!Strategy::enemyProxy() || com(Zerg_Zergling) >= 6) && !lingSpeed()) ? capGas(100) : 0;
        scout =                                         scout || vis(Zerg_Hatchery) >= 2 || (Terrain::isShitMap() && vis(Zerg_Overlord) >= 2);
        planEarly =                                     hatchCount() == 1 && s == 22;

        buildQueue[Zerg_Hatchery] =                     1 + (s >= 24);
        buildQueue[Zerg_Spawning_Pool] =                (hatchCount() >= 2 && s >= 24);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 16) + (s >= 32);

        // Composition
        armyComposition[Zerg_Drone] =                   0.60;
        armyComposition[Zerg_Zergling] =                0.40;
    }

    void ZvT4Pool()
    {
        scout =                                         scout || (vis(Zerg_Spawning_Pool) > 0 && com(Zerg_Drone) >= 4 && !Terrain::getEnemyStartingPosition().isValid());
        unitLimits[Zerg_Drone] =                        4;
        unitLimits[Zerg_Zergling] =                     INT_MAX;
        transitionReady =                               total(Zerg_Zergling) >= 24;
        firstUpgrade =                                  UpgradeTypes::Metabolic_Boost;
        rush =                                          true;

        buildQueue[Zerg_Spawning_Pool] =                s >= 8;
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18);
    }

    void ZvTOverpool()
    {
        transitionReady =                               hatchCount() >= 2;
        unitLimits[Zerg_Drone] =                        Strategy::enemyFastExpand() ? INT_MAX : 14;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        gasLimit =                                      com(Zerg_Drone) >= 11 ? gasMax() : 0;
        scout =                                         scout || vis(Zerg_Spawning_Pool) > 0 || (Terrain::isShitMap() && vis(Zerg_Spawning_Pool) > 0);
        proxy =                                         false;

        buildQueue[Zerg_Hatchery] =                     1 + (Strategy::enemyProxy() ? total(Zerg_Zergling) >= 6 : s >= 22 && vis(Zerg_Spawning_Pool) > 0);
        buildQueue[Zerg_Spawning_Pool] =                (vis(Zerg_Overlord) >= 2);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 30);
    }

    void ZvT12Pool()
    {
        transitionReady =                               hatchCount() >= 2;
        unitLimits[Zerg_Zergling] =                     4;
        gasLimit =                                      com(Zerg_Drone) >= 11 ? gasMax() : 0;
        unitLimits[Zerg_Drone] =                        13;
        scout =                                         scout || (Terrain::isShitMap() && vis(Zerg_Overlord) >= 2);

        buildQueue[Zerg_Hatchery] =                     1 + (s >= 24 && vis(Zerg_Extractor) > 0);
        buildQueue[Zerg_Extractor] =                    (s >= 24 && vis(Zerg_Spawning_Pool) > 0);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18);
        buildQueue[Zerg_Spawning_Pool] =                s >= 26;
    }

    void ZvT()
    {
        defaultZvT();

        // Reactions
        if (!lockedTransition) {
            if (Strategy::enemyRush() || Strategy::enemyProxy())
                currentTransition = "2HatchSpeedling";
            if (Strategy::getEnemyOpener() == "8Rax") {
                currentBuild = "PoolHatch";
                currentOpener = "12Pool";
                currentTransition = "2HatchMuta";
            }
            if (Strategy::getEnemyTransition() == "WorkerRush") {
                currentBuild = "PoolHatch";
                currentOpener = "Overpool";
                currentTransition = "2HatchSpeedling";
            }
        }

        // Openers
        if (currentOpener == "12Hatch")
            ZvT12Hatch();
        if (currentOpener == "4Pool")
            ZvT4Pool();
        if (currentOpener == "Overpool")
            ZvTOverpool();
        if (currentOpener == "12Pool")
            ZvT12Pool();

        // Builds
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