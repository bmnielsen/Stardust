#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    void defaultZvP() {
        inOpeningBook =                             true;
        inBookSupply =                              true;
        wallNat =                                   hatchCount() >= 4;
        wallMain =                                  false;
        wantNatural =                               true;
        wantThird =                                 Spy::getEnemyBuild() == "FFE";
        proxy =                                     false;
        hideTech =                                  false;
        playPassive =                               false;
        rush =                                      false;
        pressure =                                  false;
        transitionReady =                           false;
        planEarly =                                 false;
        gasTrick =                                  false;

        gasLimit =                                  gasMax();
        unitLimits[Zerg_Zergling] =                 lingsNeeded_ZvP();
        unitLimits[Zerg_Drone] =                    INT_MAX;

        desiredDetection =                          Zerg_Overlord;
        firstUpgrade =                              vis(Zerg_Zergling) >= 6 ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        firstTech =                                 TechTypes::None;
        firstUnit =                                 None;

        armyComposition[Zerg_Drone] =               0.60;
        armyComposition[Zerg_Zergling] =            0.40;
    }

    bool mutaPlayPassive()
    {
        if (com(Zerg_Mutalisk) == 0 || Util::getTime() < Time(6, 00)) {
            if (Spy::getEnemyBuild() == "1GateCore")
                return Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0;
            if (Spy::getEnemyBuild() == "2Gate")
                return Spy::getEnemyOpener() != "10/17" || Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0;
        }
        return false;
    }

    int lingsNeeded_ZvP() {
        auto lings = 0;
        auto timingValue = 0;
        auto initialValue = 6;

        // 2H/3H: Initial lings when pool completes
        // 2H/3H: Extra lings if enemy will arrive before defenses are ready

        if (com(Zerg_Spawning_Pool) == 0)
            return 0;

        if (Spy::getEnemyTransition() == "4Gate") {
            return 6 + (6 * vis(Zerg_Spire));
        }

        // 2Gate
        if (Spy::getEnemyBuild() == "2Gate") {
            if (Spy::getEnemyOpener() == "10/17")
                initialValue = 10;
            else if (Spy::getEnemyOpener() == "Proxy" || Spy::getEnemyOpener() == "9/9")
                initialValue = 16;
            else
                initialValue = 12;
        }

        // FFE
        if (Spy::getEnemyBuild() == "FFE") {
            if (Spy::getEnemyOpener() == "Forge" || Spy::getEnemyOpener() == "Nexus")
                initialValue = 0;
        }

        // 1GC
        if (Spy::getEnemyBuild() == "1GateCore") {
            initialValue = 10;
        }

        if (currentTransition.find("2Hatch") == string::npos)
            initialValue += 6;

        if (total(Zerg_Zergling) < initialValue)
            return initialValue;

        if (currentTransition.find("2Hatch") == string::npos && (Spy::getEnemyBuild() == "1GateCore" || Spy::getEnemyBuild() == "2Gate")) {
            if (Spy::getEnemyTransition() == "4Gate" || Spy::getEnemyTransition() == "Speedlot")
                timingValue = 5;
            else if (Spy::getEnemyTransition().find("3Gate") != string::npos || Spy::getEnemyTransition() == "DT")
                timingValue = 4;
            else if (Spy::getEnemyTransition() == "Corsair" || Players::ZvFFA() || Spy::getEnemyTransition() == "Unknown")
                timingValue = 2;
            else
                timingValue = 1;
        }

        // Specifically for proxy defense, we can allow pretty much any amount of lings after we've droned
        if (vis(Zerg_Drone) >= 18 && Spy::getEnemyOpener() == "Proxy" && Spy::getEnemyBuild() == "2Gate")
            return 24;

        auto time = double((Util::getTime().minutes - 1) * 60 + (Util::getTime().seconds)) / 60.0;
        return int(time * timingValue);
    }

    void ZvP2HatchMuta()
    {
        // 'https://liquipedia.net/starcraft/2_Hatch_Muta_(vs._Protoss)'
        lockedTransition =                              total(Zerg_Mutalisk) > 0;
        inOpeningBook =                                 Spy::getEnemyOpener() == "Proxy" ? total(Zerg_Mutalisk) < 3 : total(Zerg_Mutalisk) < 9;
        inBookSupply =                                  vis(Zerg_Overlord) < 5 || total(Zerg_Mutalisk) < 6;
        firstUpgrade =                                  (Spy::getEnemyOpener() == "Proxy" && hatchCount() >= 4) ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        firstUnit =                                     Zerg_Mutalisk;
        hideTech =                                      true;
        unitLimits[Zerg_Drone] =                        com(Zerg_Spawning_Pool) > 0 ? 26 : 13;
        unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
        playPassive =                                   mutaPlayPassive();
        wantThird =                                     Spy::enemyFastExpand() || hatchCount() >= 3 || Spy::getEnemyTransition() == "Corsair";
        gasLimit =                                      (vis(Zerg_Drone) >= 11) ? gasMax() : 0;
        planEarly =                                     Spy::enemyFastExpand() && atPercent(Zerg_Lair, 0.5) && com(Zerg_Lair) == 0 && int(Stations::getStations(PlayerState::Self).size()) < 3;

        auto thirdHatch = total(Zerg_Mutalisk) >= 6;
        if (Spy::getEnemyTransition() == "4Gate") {
            wantThird = false;
            thirdHatch = false;
            planEarly = false;
        }
        else if (Spy::getEnemyOpener() == "Proxy") {
            thirdHatch = total(Zerg_Mutalisk) >= 3;
            planEarly = false;
        }
        else if (Spy::enemyFastExpand() && vis(Zerg_Drone) >= 20 && vis(Zerg_Spire) == 0) {
            thirdHatch = true;
        }

        // Build
        buildQueue[Zerg_Hatchery] =                     2 + thirdHatch;
        buildQueue[Zerg_Extractor] =                    (s >= 24 && hatchCount() >= 2 && vis(Zerg_Drone) >= 10) + (vis(Zerg_Spire) > 0 && vis(Zerg_Drone) >= 16);
        buildQueue[Zerg_Lair] =                         (s >= 24 && gas(80));
        buildQueue[Zerg_Spire] =                        (s >= 32 && atPercent(Zerg_Lair, 0.95));
        buildQueue[Zerg_Overlord] =                     (com(Zerg_Spire) == 0 && atPercent(Zerg_Spire, 0.35)) ? 5 : 1 + (s >= 18) + (s >= 32) + (2 * (s >= 50)) + (s >= 80);

        // Composition
        if (com(Zerg_Spire) == 0 && lingsNeeded_ZvP() > vis(Zerg_Zergling)) {
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
        unitLimits[Zerg_Drone] =                        com(Zerg_Spawning_Pool) > 0 ? 33 : 15 - hatchCount();
        unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
        playPassive =                                   mutaPlayPassive();
        wantThird =                                     Spy::getEnemyBuild() != "1GateCore" && Spy::getEnemyBuild() != "2Gate";
        gasLimit =                                      (vis(Zerg_Drone) >= 11) ? gasMax() : 0;
        planEarly =                                     wantThird && hatchCount() >= 2 && Util::getTime() > Time(2, 30);

        auto spireOverlords = (Spy::getEnemyTransition() == "Corsair" || Spy::getEnemyTransition() == "NeoBisu") ? (4 * (s >= 66)) : (3 * (s >= 66)) + (s >= 82);

        // Build
        buildQueue[Zerg_Hatchery] =                     2 + (s >= 28 && vis(Zerg_Extractor) > 0);
        buildQueue[Zerg_Extractor] =                    (s >= 28 && vis(Zerg_Drone) >= 11) + (vis(Zerg_Lair) > 0 && vis(Zerg_Drone) >= 21);
        buildQueue[Zerg_Lair] =                         (s >= 32 && gas(100) && hatchCount() >= 3);
        buildQueue[Zerg_Spire] =                        (s >= 32 && atPercent(Zerg_Lair, 0.95) && vis(Zerg_Drone) >= 16);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 32 && vis(Zerg_Extractor) > 0) + (s >= 48) + spireOverlords;

        // Composition
        if (com(Zerg_Spire) == 0 && lingsNeeded_ZvP() > vis(Zerg_Zergling)) {
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
        unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
        playPassive =                                   (Spy::getEnemyBuild() == "1GateCore" || Spy::getEnemyBuild() == "2Gate") && vis(Zerg_Spire) == 0;
        wantThird =                                     Spy::getEnemyBuild() == "FFE";

        // Build
        buildQueue[Zerg_Hatchery] =                     2 + (s >= 28) + (vis(Zerg_Drone) >= 18 && vis(Zerg_Overlord) >= 3);
        buildQueue[Zerg_Extractor] =                    (vis(Zerg_Drone) >= 18) + (vis(Zerg_Lair) > 0 && vis(Zerg_Drone) >= 18) + (total(Zerg_Mutalisk) >= 9);
        buildQueue[Zerg_Lair] =                         (gas(80));
        buildQueue[Zerg_Spire] =                        (s >= 36 && atPercent(Zerg_Lair, 0.95));
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 36) + (s >= 46) + (s >= 54) + (s >= 62) + (2 * (s >= 88));

        // Composition
        if (Spy::getEnemyBuild() == "2Gate" && Spy::getEnemyTransition() != "Corsair" && (com(Zerg_Sunken_Colony) == 0 || com(Zerg_Drone) < 9)) {
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
        pressure =                                      !Spy::enemyProxy();
        playPassive =                                   Broodwar->self()->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) == 0 && Spy::getEnemyOpener() != "Proxy";
        wantNatural =                                   !Spy::enemyProxy();
        firstUpgrade =                                  gas(100) ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;

        // Build
        buildQueue[Zerg_Hatchery] =                     1 + (s >= 20 && vis(Zerg_Spawning_Pool) > 0 && (!Spy::enemyProxy() || vis(Zerg_Zergling) >= 6));
        buildQueue[Zerg_Extractor] =                    (hatchCount() >= 2 && com(Zerg_Drone) >= 8 && vis(Zerg_Zergling) >= 6 && (!Spy::enemyProxy() || vis(Zerg_Sunken_Colony) > 0));
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 30);

        // Composition
        if (vis(Zerg_Drone) < 6) {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            0.40;
        }
        else {
            armyComposition[Zerg_Drone] =               0.00;
            armyComposition[Zerg_Zergling] =            1.00;
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
        playPassive =                                   Broodwar->self()->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) == 0 && Spy::getEnemyOpener() != "Proxy";

        // Build
        buildQueue[Zerg_Hatchery] =                     1 + (s >= 22 && vis(Zerg_Spawning_Pool) > 0) + (s >= 26);
        buildQueue[Zerg_Extractor] =                    (s >= 24);
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 26);

        // Composition
        armyComposition[Zerg_Drone] =                   0.20;
        armyComposition[Zerg_Zergling] =                0.80;
    }

    void ZvP5HatchArmorSpeedling()
    {
        auto armor = Broodwar->self()->isUpgrading(UpgradeTypes::Zerg_Carapace) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Carapace) != 0;
        lockedTransition =                              true;
        inOpeningBook =                                 total(Zerg_Zergling) < 500;
        inBookSupply =                                  vis(Zerg_Overlord) < 3;
        firstUpgrade =                                  lingSpeed() ? UpgradeTypes::Zerg_Carapace : UpgradeTypes::Metabolic_Boost;
        firstUnit =                                     Zerg_Mutalisk;
        unitLimits[Zerg_Drone] =                        com(Zerg_Spawning_Pool) == 0 ? 15 - hatchCount() : 15;
        unitLimits[Zerg_Zergling] =                     200;
        playPassive =                                   Util::getTime() < Time(5, 30);
        gasLimit =                                      (vis(Zerg_Drone) >= 11 && !armor) ? gasMax() : 0;

        buildQueue[Zerg_Hatchery] =                     2 + (s >= 26) + 3 * (s >= 70);
        buildQueue[Zerg_Extractor] =                    (s >= 32 && vis(Zerg_Drone) >= 11 && hatchCount() >= 3);
        buildQueue[Zerg_Evolution_Chamber] =            (lingSpeed());
        buildQueue[Zerg_Overlord] =                     1 + (s >= 18) + (s >= 32 && vis(Zerg_Extractor) > 0);

        // Composition
        if (lingsNeeded_ZvP() > vis(Zerg_Zergling)) {
            armyComposition[Zerg_Drone] =               0.00;
            armyComposition[Zerg_Zergling] =            1.00;
        }
        else {
            armyComposition[Zerg_Drone] =               0.60;
            armyComposition[Zerg_Zergling] =            0.40;
        }
    }

    void ZvP3HatchHydra()
    {

    }

    void ZvP6HatchHydra()
    {
        //`https://liquipedia.net/starcraft/6_hatch_(vs._Protoss)`
        lockedTransition =                              Util::getTime() > Time(3, 15);
        inOpeningBook =                                 total(Zerg_Hydralisk) < 24;
        inBookSupply =                                  vis(Zerg_Overlord) < 5;
        firstUpgrade =                                  UpgradeTypes::Muscular_Augments;
        firstUnit =                                     Zerg_Hydralisk;
        hideTech =                                      true;
        unitLimits[Zerg_Drone] =                        INT_MAX;
        unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvP();
        unitLimits[Zerg_Scourge] =                      max(2, Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) * 2);
        playPassive =                                   (Spy::getEnemyBuild() == "1GateCore" || (Spy::getEnemyBuild() == "2Gate" && Spy::getEnemyOpener() != "Unknown")) && Util::getTime() < Time(5, 00);
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
        if (com(Zerg_Spire) == 0 && lingsNeeded_ZvP() > vis(Zerg_Zergling)) {
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

    void ZvP()
    {
        defaultZvP();

        // Builds
        if (currentBuild == "HatchPool")
            ZvP_HP();
        if (currentBuild == "PoolHatch")
            ZvP_PH();

        // Reactions
        if (!lockedTransition) {
            if (Spy::getEnemyBuild() != "FFE" && Util::getTime() > Time(3, 00) && currentTransition == "6HatchHydra")
                currentTransition = "2HatchMuta";
            if ((Spy::getEnemyBuild() == "2Gate" || Spy::getEnemyBuild() == "1GateCore") && currentTransition != "2HatchMuta")
                currentTransition = "2HatchMuta";
            if (Spy::enemyProxy() && Util::getTime() < Time(2, 00)) {
                currentBuild = "PoolHatch";
                currentOpener = "Overpool";
                currentTransition = "2HatchMuta";
            }
            if (Spy::getEnemyOpener() == "9/9")
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