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

        int capGas(int value) {
            auto onTheWay = 0;
            for (auto &w : Units::getUnits(PlayerState::Self)) {
                auto &worker = *w;
                if (worker.unit()->isCarryingGas() || (worker.hasResource() && worker.getResource().getType().isRefinery()))
                    onTheWay+=8;
            }

            return int(round(double(value - Broodwar->self()->gas() - onTheWay) / 8.0));
        }

        int hatchCount() {
            return vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive);
        }

        int colonyCount() {
            return vis(Zerg_Creep_Colony) + vis(Zerg_Sunken_Colony);
        }

        int lingsNeeded() {
            auto maxValue = currentTransition.find("3Hatch") != string::npos ? 48 : 24;
            if (Strategy::getEnemyBuild() == "2Gate") {
                if (total(Zerg_Zergling) < 14 && Strategy::getEnemyOpener() == "9/9")
                    return 14;
                if (total(Zerg_Zergling) < 10 && (Strategy::getEnemyOpener() == "10/12" || Strategy::getEnemyOpener() == "Unknown"))
                    return 10;
                if (total(Zerg_Zergling) < 6 && Strategy::getEnemyOpener() == "10/17")
                    return 6;
            }

            if (Strategy::enemyProxy())
                return 24;

            if (currentTransition.find("3Hatch") != string::npos) {
                auto armyMatchValue = (Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) * 2) + (Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) * 2) - (Strategy::enemyFastExpand() * 8);

                if (Strategy::getEnemyBuild() == "2Gate") {
                    auto minValue = max(6, 2 * (Util::getTime().minutes - 3));
                    return clamp(armyMatchValue, minValue, 48) - max(0, ((colonyCount() - 1) * 4));
                }

                if (Strategy::getEnemyBuild() == "1GateCore") {
                    auto minValue = max(6, 2 * (Util::getTime().minutes - 3));
                    return clamp(armyMatchValue, minValue, 48) - max(0, ((colonyCount() - 1) * 4));
                }
            }
            else {
                auto armyMatchValue = (Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) * 2) + (Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon)) - (Strategy::enemyFastExpand() * 8);

                if (Strategy::getEnemyBuild() == "2Gate") {
                    auto minValue = max(6, 2 * (Util::getTime().minutes - 3));
                    return clamp(armyMatchValue, minValue, 24) - max(0, ((colonyCount() - 1) * 4));
                }

                if (Strategy::getEnemyBuild() == "1GateCore") {
                    auto minValue = max(6, 2 * (Util::getTime().minutes - 3));
                    return clamp(armyMatchValue, minValue, 12) - max(0, ((colonyCount() - 1) * 4));
                }
            }

            if (Strategy::getEnemyBuild() == "FFE")
                return 6 * vis(Zerg_Lair);
            return 6;
        }

        void defaultZvP() {
            inOpeningBook =                                 true;
            inBookSupply =                                  true;
            wallNat =                                       hatchCount() >= 4 || Terrain::isShitMap();
            wallMain =                                      false;
            scout =                                         false;
            wantNatural =                                   true;
            wantThird =                                     Strategy::getEnemyBuild() == "FFE";
            proxy =                                         false;
            hideTech =                                      false;
            playPassive =                                   false;
            rush =                                          false;
            pressure =                                      false;
            cutWorkers =                                    false;
            transitionReady =                               false;
            planEarly =                                     false;

            gasLimit =                                      gasMax();
            unitLimits[Zerg_Zergling] =                     lingsNeeded();
            unitLimits[Zerg_Drone] =                        INT_MAX;

            desiredDetection =                              Zerg_Overlord;
            firstUpgrade =                                  vis(Zerg_Zergling) >= 6 ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
            firstTech =                                     TechTypes::None;
            firstUnit =                                     None;

            armyComposition[Zerg_Drone] =                   0.60;
            armyComposition[Zerg_Zergling] =                0.40;
        }
    }

    void ZvP2HatchMuta()
    {
        // 'https://liquipedia.net/starcraft/2_Hatch_Muta_(vs._Protoss)'
        lockedTransition =                              total(Zerg_Mutalisk) > 0;
        inOpeningBook =                                 total(Zerg_Mutalisk) < 9;
        inBookSupply =                                  vis(Zerg_Overlord) < 5 || total(Zerg_Mutalisk) < 6;
        firstUpgrade =                                  UpgradeTypes::None;
        firstUnit =                                     Zerg_Mutalisk;
        hideTech =                                      true;
        unitLimits[Zerg_Drone] =                        com(Zerg_Spawning_Pool) > 0 ? 26 : 13;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        playPassive =                                   (Strategy::getEnemyBuild() == "1GateCore" || (Strategy::getEnemyBuild() == "2Gate" && Strategy::getEnemyOpener() != "Unknown")) && (com(Zerg_Mutalisk) == 0 || Util::getTime() < Time(6, 00));
        wantThird =                                     Strategy::enemyFastExpand() || hatchCount() >= 3;
        gasLimit =                                      (vis(Zerg_Drone) >= 11) ? gasMax() : 0;

        auto thirdHatch = total(Zerg_Mutalisk) >= 6;
        if (Strategy::enemyPressure()) {
            thirdHatch = false;
            planEarly = false;
        }
        else if (Strategy::enemyFastExpand() && vis(Zerg_Drone) >= 20 && s >= 48) {
            thirdHatch = true;
            planEarly = atPercent(Zerg_Lair, 0.5) && int(Stations::getMyStations().size()) < 3;
        }

        // Build
        buildQueue[Zerg_Hatchery] =                     2 + thirdHatch;
        buildQueue[Zerg_Extractor] =                    (hatchCount() >= 2 && vis(Zerg_Drone) >= 10) + (vis(Zerg_Spire) > 0 && vis(Zerg_Drone) >= 19);
        buildQueue[Zerg_Lair] =                         (vis(Zerg_Drone) >= 12 && gas(80));
        buildQueue[Zerg_Spire] =                        (s >= 32 && atPercent(Zerg_Lair, 0.95));
        buildQueue[Zerg_Overlord] =                     (Strategy::getEnemyBuild() == "FFE" && atPercent(Zerg_Spire, 0.5)) ? 5 : 1 + (s >= 18) + (s >= 32) + (2 * (s >= 50)) + (s >= 80);

        // Composition
        if (com(Zerg_Spire) == 0 && lingsNeeded() > vis(Zerg_Zergling)) {
            armyComposition[Zerg_Drone] =               0.00;
            armyComposition[Zerg_Zergling] =            1.00;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            com(Zerg_Spire) > 0 ? 0.00 : 0.40;
            armyComposition[Zerg_Mutalisk] =            com(Zerg_Spire) > 0 ? 0.40 : 0.00;
        }
    }

    void ZvP2pt5HatchMuta()
    {

    }

    void ZvP3HatchMuta()
    {
        // 'https://liquipedia.net/starcraft/3_Hatch_Spire_(vs._Protoss)'
        lockedTransition =                              hatchCount() >= 3 || total(Zerg_Mutalisk) > 0;
        inOpeningBook =                                 total(Zerg_Mutalisk) < 9;
        inBookSupply =                                  vis(Zerg_Overlord) < 8 || total(Zerg_Mutalisk) < 9;
        firstUpgrade =                                  UpgradeTypes::None;
        firstUnit =                                     Zerg_Mutalisk;
        hideTech =                                      true;
        unitLimits[Zerg_Drone] =                        33;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        playPassive =                                   (Strategy::getEnemyBuild() == "1GateCore" || (Strategy::getEnemyBuild() == "2Gate" && Strategy::getEnemyOpener() != "Unknown")) && (com(Zerg_Mutalisk) == 0 || Util::getTime() < Time(6, 00));
        wantThird =                                     Strategy::enemyFastExpand() || hatchCount() >= 3;
        gasLimit =                                      (vis(Zerg_Drone) >= 11) ? gasMax() : 0;

        auto fourthHatch = Strategy::getEnemyBuild() == "FFE" && com(Zerg_Lair) > 0 && vis(Zerg_Drone) >= 16;

        // Build
        buildQueue[Zerg_Hatchery] =                     2 + (s >= 26 && vis(Zerg_Drone) >= 13) + fourthHatch;
        buildQueue[Zerg_Extractor] =                    (s >= 32 && vis(Zerg_Drone) >= 15) + (vis(Zerg_Lair) > 0 && vis(Zerg_Drone) >= 16);
        buildQueue[Zerg_Lair] =                         (s >= 32 && vis(Zerg_Drone) >= 15 && gas(100));
        buildQueue[Zerg_Spire] =                        (s >= 32 && atPercent(Zerg_Lair, 0.95) && vis(Zerg_Drone) >= 16);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 32 && vis(Zerg_Extractor) > 0) + (s >= 46) + (2 * (s >= 66)) + (s >= 68) + (s >= 82);

        // Composition
        if (com(Zerg_Spire) == 0 && lingsNeeded() > vis(Zerg_Zergling)) {
            armyComposition[Zerg_Drone] =               0.00;
            armyComposition[Zerg_Zergling] =            1.00;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            com(Zerg_Spire) > 0 ? 0.00 : 0.40;
            armyComposition[Zerg_Mutalisk] =            com(Zerg_Spire) > 0 ? 0.40 : 0.00;
        }
    }

    void ZvP4HatchMuta()
    {
        // 'https://liquipedia.net/starcraft/4_Hatch_Lair_(vs._Protoss)'
        lockedTransition =                              atPercent(Zerg_Lair, 0.25) || total(Zerg_Mutalisk) > 0;
        inOpeningBook =                                 total(Zerg_Mutalisk) < 9;
        inBookSupply =                                  vis(Zerg_Overlord) < 6 || total(Zerg_Mutalisk) < 6;
        firstUpgrade =                                  (com(Zerg_Lair) > 0 && vis(Zerg_Overlord) >= 5) ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        firstUnit =                                     Zerg_Mutalisk;
        hideTech =                                      true;
        pressure =                                      vis(Zerg_Zergling) >= 36;
        unitLimits[Zerg_Drone] =                        32;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        playPassive =                                   (Strategy::getEnemyBuild() == "1GateCore" || Strategy::getEnemyBuild() == "2Gate") && vis(Zerg_Spire) == 0;
        wantThird =                                     Strategy::getEnemyBuild() == "FFE";

        // Build
        buildQueue[Zerg_Hatchery] =                     2 + (s >= 28) + (vis(Zerg_Drone) >= 18 && vis(Zerg_Overlord) >= 3);
        buildQueue[Zerg_Extractor] =                    (vis(Zerg_Drone) >= 18) + (vis(Zerg_Lair) > 0 && vis(Zerg_Drone) >= 18) + (total(Zerg_Mutalisk) >= 9);
        buildQueue[Zerg_Lair] =                         (vis(Zerg_Drone) >= 10 && gas(80));
        buildQueue[Zerg_Spire] =                        (s >= 36 && atPercent(Zerg_Lair, 0.95));
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 36) + (s >= 46) + (s >= 54) + (s >= 62) + (2 * (s >= 88));

        // Composition
        if (Strategy::getEnemyBuild() == "2Gate" && Strategy::getEnemyTransition() != "Corsair" && (com(Zerg_Sunken_Colony) == 0 || com(Zerg_Drone) < 9)) {
            armyComposition[Zerg_Drone] =               0.20;
            armyComposition[Zerg_Zergling] =            0.80;
            gasLimit =                                  vis(Zerg_Drone) >= 12 ? gasMax() : 0;
        }
        else {
            armyComposition[Zerg_Drone] =               com(Zerg_Spire) > 0 ? 0.10 : 0.60;
            armyComposition[Zerg_Zergling] =            com(Zerg_Spire) > 0 ? 0.50 : 0.40;
            armyComposition[Zerg_Mutalisk] =            com(Zerg_Spire) > 0 ? 0.40 : 0.00;
            gasLimit =                                  vis(Zerg_Drone) >= 12 ? gasMax() : 0;
        }
    }

    void ZvP2HatchSpeedling()
    {
        lockedTransition =                              true;
        inOpeningBook =                                 total(Zerg_Zergling) < 90;
        inBookSupply =                                  vis(Zerg_Overlord) < 3;
        unitLimits[Zerg_Drone] =                        10 - hatchCount() - vis(Zerg_Extractor);
        unitLimits[Zerg_Zergling] =                     INT_MAX;
        gasLimit =                                      !lingSpeed() ? capGas(100) : 0;
        wallNat =                                       true;
        pressure =                                      !Strategy::enemyProxy();
        playPassive =                                   Broodwar->self()->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) == 0 && Strategy::getEnemyOpener() != "Proxy";
        wantNatural =                                   !Strategy::enemyProxy();
        firstUpgrade =                                  gas(100) ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;

        // Build
        buildQueue[Zerg_Hatchery] =                     1 + (s >= 20 && vis(Zerg_Spawning_Pool) > 0 && (!Strategy::enemyProxy() || vis(Zerg_Zergling) >= 6));
        buildQueue[Zerg_Extractor] =                    (hatchCount() >= 2 && com(Zerg_Drone) >= 8 && vis(Zerg_Zergling) >= 6 && (!Strategy::enemyProxy() || vis(Zerg_Sunken_Colony) > 0));
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 30);
        
        // Composition
        if (vis(Zerg_Drone) < 6) {
            armyComposition[Zerg_Drone] =                   0.60;
            armyComposition[Zerg_Zergling] =                0.40;
        }
        else {
            armyComposition[Zerg_Drone] =                   0.00;
            armyComposition[Zerg_Zergling] =                1.00;
        }
    }

    void ZvP3HatchSpeedling()
    {
        // 'https://liquipedia.net/starcraft/3_Hatch_Zergling_(vs._Protoss)'
        lockedTransition =                              true;
        inOpeningBook =                                 total(Zerg_Zergling) < 56 && Players::getTotalCount(PlayerState::Enemy, Protoss_Dark_Templar) == 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Reaver) == 0;
        inBookSupply =                                  vis(Zerg_Overlord) < 3;
        firstUpgrade =                                  hatchCount() >= 3 ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        unitLimits[Zerg_Drone] =                        13;
        unitLimits[Zerg_Zergling] =                     INT_MAX;
        gasLimit =                                      !lingSpeed() ? capGas(100) : 0;
        wallNat =                                       true;
        pressure =                                      com(Zerg_Hatchery) >= 3 && inOpeningBook;
        wantThird =                                     false;
        playPassive =                                   Broodwar->self()->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) == 0 && Strategy::getEnemyOpener() != "Proxy";

        // Build
        buildQueue[Zerg_Hatchery] =                     1 + (s >= 22 && vis(Zerg_Spawning_Pool) > 0) + (s >= 26);
        buildQueue[Zerg_Extractor] =                    (s >= 24);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 26);

        // Composition
        armyComposition[Zerg_Drone] =                   0.20;
        armyComposition[Zerg_Zergling] =                0.80;
    }

    void ZvP3HatchHydra()
    {

    }

    void ZvP6HatchHydra()
    {
        //`https://liquipedia.net/starcraft/6_hatch_(vs._Protoss)`
        lockedTransition =                              hatchCount() >= 3 || total(Zerg_Mutalisk) > 0;
        inOpeningBook =                                 total(Zerg_Hydralisk) < 24;
        inBookSupply =                                  vis(Zerg_Overlord) < 5;
        firstUpgrade =                                  UpgradeTypes::Muscular_Augments;
        firstUnit =                                     Zerg_Hydralisk;
        hideTech =                                      true;
        unitLimits[Zerg_Drone] =                        INT_MAX;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        unitLimits[Zerg_Scourge] =                      6;
        playPassive =                                   (Strategy::getEnemyBuild() == "1GateCore" || (Strategy::getEnemyBuild() == "2Gate" && Strategy::getEnemyOpener() != "Unknown")) && Util::getTime() < Time(5, 00);
        wantThird =                                     true;
        gasLimit =                                      (vis(Zerg_Drone) >= 14) ? gasMax() : 0;
        planEarly =                                     hatchCount() < 3 && s == 28;

        buildQueue[Zerg_Hatchery] =                     2 + (s >= 30 && vis(Zerg_Extractor) > 0) + (s >= 64) + (s >= 70) + (s >= 76);
        buildQueue[Zerg_Extractor] =                    (s >= 30) + (s >= 84) + (vis(Zerg_Evolution_Chamber) > 0);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 32) + (s >= 46) + (s >= 62);
        buildQueue[Zerg_Lair] =                         (s >= 36);
        buildQueue[Zerg_Spire] =                        (s >= 40 && atPercent(Zerg_Lair, 0.95) && vis(Zerg_Drone) >= 16);
        buildQueue[Zerg_Hydralisk_Den] =                (s >= 80);
        buildQueue[Zerg_Evolution_Chamber] =            (s >= 84);

        // Composition
        if (com(Zerg_Spire) == 0 && lingsNeeded() > vis(Zerg_Zergling)) {
            armyComposition[Zerg_Drone] =               0.00;
            armyComposition[Zerg_Zergling] =            1.00;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            0.10;
            armyComposition[Zerg_Hydralisk] =           0.30;
            armyComposition[Zerg_Scourge] =             0.05;
        }
    }

    void ZvP2HatchLurker()
    {
        // 'https://liquipedia.net/starcraft/2_Hatch_Lurker_(vs._Terran)'
        lockedTransition =                              true;
        inOpeningBook =                                 total(Zerg_Lurker) < 3;
        inBookSupply =                                  vis(Zerg_Overlord) < 3;
        unitLimits[Zerg_Drone] =                        20;
        unitLimits[Zerg_Zergling] =                     12;
        unitLimits[Zerg_Hydralisk] =                    3;
        firstUpgrade =                                  UpgradeTypes::None;
        firstTech =                                     TechTypes::Lurker_Aspect;
        firstUnit =                                     Zerg_Lurker;
        playPassive =                                   !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect);
        gasLimit =                                      gasMax();

        buildQueue[Zerg_Extractor] =                    (s >= 24) + (atPercent(Zerg_Lair, 0.90));
        buildQueue[Zerg_Lair] =                         (vis(Zerg_Drone) >= 10 && gas(80));
        buildQueue[Zerg_Hydralisk_Den] =                atPercent(Zerg_Lair, 0.5);

        // Composition
        armyComposition[Zerg_Drone] =                   0.60;
        armyComposition[Zerg_Zergling] =                0.20;
        armyComposition[Zerg_Hydralisk] =               0.20;
        armyComposition[Zerg_Lurker] =                  1.00;
    }

    void ZvPOverpool()
    {
        // 'https://liquipedia.net/starcraft/Overpool_(vs._Protoss)'
        transitionReady =                               hatchCount() >= 2;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        gasLimit =                                      0;
        scout =                                         scout || (hatchCount() >= 2) || (Terrain::isShitMap() && vis(Zerg_Spawning_Pool) > 0);
        wantNatural =                                   !Strategy::enemyProxy();
        playPassive =                                   false;

        if (Strategy::enemyFastExpand())
            unitLimits[Zerg_Drone] =                        INT_MAX;
        else
            unitLimits[Zerg_Drone] =                        12 - vis(Zerg_Hatchery);

        buildQueue[Zerg_Hatchery] =                     1 + (s >= 22 && vis(Zerg_Spawning_Pool) > 0 && (!Strategy::enemyProxy() || vis(Zerg_Sunken_Colony) >= 2));
        buildQueue[Zerg_Spawning_Pool] =                (vis(Zerg_Overlord) >= 2);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 30);
    }

    void ZvP4Pool()
    {
        // 'https://liquipedia.net/starcraft/5_Pool_(vs._Protoss)'
        transitionReady =                               total(Zerg_Zergling) >= 24;
        unitLimits[Zerg_Drone] =                        4;
        unitLimits[Zerg_Zergling] =                     INT_MAX;
        gasLimit =                                      0;
        wantNatural =                                   !Strategy::enemyProxy();
        scout =                                         scout || (vis(Zerg_Spawning_Pool) > 0 && com(Zerg_Drone) >= 4 && !Terrain::getEnemyStartingPosition().isValid());
        rush =                                          true;

        buildQueue[Zerg_Hatchery] =                     1;
        buildQueue[Zerg_Spawning_Pool] =                s >= 8;
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18);
    }

    void ZvP9Pool()
    {
        // 'https://liquipedia.net/starcraft/9_Pool_(vs._Protoss)'
        transitionReady =                               hatchCount() >= 2;
        unitLimits[Zerg_Drone] =                        11 - hatchCount();
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        gasLimit =                                      0;
        gasTrick =                                      vis(Zerg_Spawning_Pool) > 0 && total(Zerg_Overlord) < 2;
        scout =                                         scout || (vis(Zerg_Spawning_Pool) > 0 && s >= 22) || (Terrain::isShitMap() && vis(Zerg_Spawning_Pool) > 0);
        wantNatural =                                   !Strategy::enemyProxy();
        playPassive =                                   false;

        if (Strategy::enemyFastExpand())
            unitLimits[Zerg_Drone] =                        INT_MAX;
        else
            unitLimits[Zerg_Drone] =                        12 - vis(Zerg_Hatchery);

        buildQueue[Zerg_Hatchery] =                     1 + (s >= 20 && vis(Zerg_Spawning_Pool) > 0 && atPercent(Zerg_Spawning_Pool, 0.8) && vis(Zerg_Overlord) >= 2 && (!Strategy::enemyProxy() || vis(Zerg_Sunken_Colony) >= 2));
        buildQueue[Zerg_Spawning_Pool] =                s >= 18;
        buildQueue[Zerg_Overlord] =                     1 + (s >= 20 && vis(Zerg_Spawning_Pool) > 0) + (s >= 30);
    }

    void ZvP10Hatch()
    {
        // 'https://liquipedia.net/starcraft/10_Hatch'
        transitionReady =                               total(Zerg_Overlord) >= 2;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        gasLimit =                                      0;
        scout =                                         false;
        wantNatural =                                   !Strategy::enemyProxy();
        playPassive =                                   false;
        unitLimits[Zerg_Drone] =                        10;
        planEarly =                                     hatchCount() == 1 && s == 20 && Broodwar->self()->minerals() >= 150;
        gasTrick =                                      s >= 18 && hatchCount() < 2 && total(Zerg_Spawning_Pool) == 0;

        buildQueue[Zerg_Hatchery] =                     1 + (s >= 20);
        buildQueue[Zerg_Spawning_Pool] =                hatchCount() >= 2;
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18 && vis(Zerg_Spawning_Pool) > 0) + (s >= 36);
    }

    void ZvP12Hatch()
    {
        // 'https://liquipedia.net/starcraft/12_Hatch_(vs._Protoss)'
        transitionReady =                               vis(Zerg_Spawning_Pool) > 0;
        unitLimits[Zerg_Zergling] =                     lingsNeeded();
        gasLimit =                                      0;
        scout =                                         scout || (hatchCount() >= 2) || (Terrain::isShitMap() && vis(Zerg_Overlord) >= 2);
        wantNatural =                                   !Strategy::enemyProxy();
        playPassive =                                   false;
        unitLimits[Zerg_Drone] =                        13 - vis(Zerg_Hatchery) - vis(Zerg_Spawning_Pool);
        planEarly =                                     hatchCount() == 1 && s == 22;

        buildQueue[Zerg_Hatchery] =                     1 + (s >= 24);
        buildQueue[Zerg_Spawning_Pool] =                hatchCount() >= 2;
        buildQueue[Zerg_Overlord] =                     1 + (s >= 16) + (s >= 30);
    }

    void ZvP()
    {
        defaultZvP();

        // Openers
        if (currentOpener == "Overpool")
            ZvPOverpool();
        if (currentOpener == "4Pool")
            ZvP4Pool();
        if (currentOpener == "9Pool")
            ZvP9Pool();
        if (currentOpener == "10Hatch")
            ZvP10Hatch();
        if (currentOpener == "12Hatch")
            ZvP12Hatch();

        // Reactions
        if (!lockedTransition) {
            if (Strategy::enemyProxy())
                currentTransition = "2HatchSpeedling";
            if (Strategy::getEnemyBuild() == "2Gate" && currentTransition == "6HatchHydra")
                currentTransition = "3HatchMuta";
            if (Strategy::enemyProxy() && Util::getTime() < Time(2, 00))
                currentOpener = "9Pool";
            if (Strategy::getEnemyOpener() == "9/9")
                currentTransition = "2HatchMuta";
        }

        // Transitions
        if (transitionReady) {
            if (currentTransition == "2HatchMuta")
                ZvP2HatchMuta();
            if (currentTransition == "2.5HatchMuta")
                ZvP2pt5HatchMuta();
            if (currentTransition == "3HatchMuta")
                ZvP3HatchMuta();
            if (currentTransition == "4HatchMuta")
                ZvP4HatchMuta();
            if (currentTransition == "2HatchSpeedling")
                ZvP2HatchSpeedling();
            if (currentTransition == "3HatchSpeedling")
                ZvP3HatchSpeedling();
            if (currentTransition == "6HatchHydra")
                ZvP6HatchHydra();
            if (currentTransition == "2HatchLurker")
                ZvP2HatchLurker();
        }
    }
}