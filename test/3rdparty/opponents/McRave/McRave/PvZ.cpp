#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

namespace McRave::BuildOrder::Protoss {

    namespace {

        bool goonRange() {
            return Broodwar->self()->isUpgrading(UpgradeTypes::Singularity_Charge) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Singularity_Charge);
        }

        void defaultPvZ() {
            inOpeningBook =                                 true;
            inBookSupply =                                  vis(Protoss_Pylon) < 2;
            wallNat =                                       vis(Protoss_Nexus) >= 2 || currentOpener == "Natural";
            wallMain =                                      false;
            scout =                                         vis(Protoss_Gateway) > 0;
            wantNatural =                                   false;
            wantThird =                                     false;
            proxy =                                         false;
            hideTech =                                      false;
            playPassive =                                   false;
            rush =                                          false;
            cutWorkers =                                    false;
            transitionReady =                               false;

            gasLimit =                                      INT_MAX;
            unitLimits[Protoss_Zealot] =                    INT_MAX;
            unitLimits[Protoss_Dragoon] =                   0;

            desiredDetection =                              Protoss_Observer;
            firstUpgrade =                                  vis(Protoss_Dragoon) > 0 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
            firstTech =                                     TechTypes::None;
            firstUnit =                                     None;
        }
    }

    void PvZ2GateDefensive() {
        gasLimit =                                          (com(Protoss_Cybernetics_Core) && s >= 50) ? INT_MAX : 0;
        inOpeningBook =                                     vis(Protoss_Corsair) == 0;
        playPassive =                                       s < 60;
        firstUpgrade =                                      UpgradeTypes::None;
        firstTech =                                         TechTypes::None;
        cutWorkers =                                        Production::hasIdleProduction();

        unitLimits[Protoss_Zealot] =                        INT_MAX;
        unitLimits[Protoss_Dragoon] =                       vis(Protoss_Templar_Archives) > 0 ? INT_MAX : 0;
        firstUnit =                                         Protoss_Corsair;

        buildQueue[Protoss_Nexus] =                         1;
        buildQueue[Protoss_Pylon] =                         (s >= 16) + (s >= 30);
        buildQueue[Protoss_Gateway] =                       (s >= 20) + (vis(Protoss_Zealot) > 0) + (s >= 66);
        buildQueue[Protoss_Assimilator] =                   s >= 40;
        buildQueue[Protoss_Cybernetics_Core] =              s >= 58;
        buildQueue[Protoss_Stargate] =                      atPercent(Protoss_Cybernetics_Core, 1.00);
    }

    void PvZFFE()
    {
        defaultPvZ();
        wantNatural =                                       true;
        wallNat =                                           true;
        cutWorkers =                                        Strategy::enemyRush() && vis(Protoss_Photon_Cannon) < 2 && vis(Protoss_Forge) > 0;

        int cannonCount = 0;

        // Reactions
        if (!lockedTransition) {
            if (Strategy::getEnemyOpener() == "4Pool" && currentOpener != "Forge")
                currentOpener = "Panic";
            if (Strategy::getEnemyTransition() == "2HatchHydra" || Strategy::getEnemyTransition() == "3HatchHydra")
                currentTransition = "StormRush";
            else if (Strategy::getEnemyTransition() == "2HatchMuta" || Strategy::getEnemyTransition() == "3HatchMuta")
                currentTransition = "2Stargate";
        }

        // Openers
        if (currentOpener == "Forge") {
            scout =                                         vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Gateway) >= 1;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 28);
            buildQueue[Protoss_Pylon] =                     (s >= 14) + (s >= 30), (s >= 16) + (s >= 30);
            buildQueue[Protoss_Assimilator] =               s >= 34;
            buildQueue[Protoss_Gateway] =                   s >= 32;
            buildQueue[Protoss_Forge] =                     s >= 20;
        }
        else if (currentOpener == "Nexus") {
            scout =                                         vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Assimilator) >= 1;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 24);
            buildQueue[Protoss_Pylon] =                     (s >= 14) + (s >= 30), (s >= 16) + (s >= 30);
            buildQueue[Protoss_Assimilator] =               vis(Protoss_Gateway) >= 1;
            buildQueue[Protoss_Gateway] =                   vis(Protoss_Forge) > 0;
            buildQueue[Protoss_Forge] =                     vis(Protoss_Nexus) >= 2;
        }
        else if (currentOpener == "Gate") {
            scout =                                         vis(Protoss_Gateway) > 0;
            transitionReady =                               vis(Protoss_Nexus) >= 2;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 42);
            buildQueue[Protoss_Pylon] =                     (s >= 14) + (s >= 30), (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   vis(Protoss_Pylon) > 0;
            buildQueue[Protoss_Forge] =                     s >= 60;
        }
        else if (currentOpener == "Panic") {
            scout =                                         com(Protoss_Photon_Cannon) >= 3;
            transitionReady =                               vis(Protoss_Pylon) >= 2;
            cutWorkers =                                    vis(Protoss_Pylon) == 1 && vis(Protoss_Probe) >= 13;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     1 + (Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) < 6 || s >= 32);
            buildQueue[Protoss_Shield_Battery] =            vis(Protoss_Gateway) > 0;
            buildQueue[Protoss_Gateway] =                   0;
        }

        // Don't add an assimilator if we're being rushed
        if (Strategy::enemyRush())
            buildQueue[Protoss_Assimilator] =               0;

        // Don't mine gas if we need cannons
        if (cannonCount > vis(Protoss_Photon_Cannon) && Broodwar->getFrameCount() < 10000)
            gasLimit = 0;

        // If we want Cannons but have no Forge
        if (cannonCount > 0 && currentOpener != "Panic") {
            if (!atPercent(Protoss_Forge, 1.00)) {
                cannonCount =                               0;
                buildQueue[Protoss_Forge] =                 1;
            }
            else
                buildQueue[Protoss_Photon_Cannon] =         min(vis(Protoss_Photon_Cannon) + 1, cannonCount);
        }

        // Transitions
        if (transitionReady) {
            if (currentTransition == "StormRush") {
                inOpeningBook =                             s < 100;
                lockedTransition =                          total(Protoss_Citadel_of_Adun) > 0;
                firstUpgrade =                              UpgradeTypes::None;
                firstTech =                                 TechTypes::Psionic_Storm;
                firstUnit =                                 Protoss_High_Templar;

                // Build
                buildQueue[Protoss_Assimilator] =           (s >= 38) + (s >= 60);
                buildQueue[Protoss_Cybernetics_Core] =      s >= 42;
                buildQueue[Protoss_Citadel_of_Adun] =       atPercent(Protoss_Cybernetics_Core, 1.00);
                buildQueue[Protoss_Templar_Archives] =      atPercent(Protoss_Citadel_of_Adun, 1.00);
                buildQueue[Protoss_Stargate] =              0;

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.75;
                armyComposition[Protoss_High_Templar] =     0.15;
                armyComposition[Protoss_Dark_Templar] =     0.10;
            }
            else if (currentTransition == "2Stargate") {
                inOpeningBook =                             s < 100;
                lockedTransition =                          total(Protoss_Stargate) >= 2; 
                firstUpgrade =                              total(Protoss_Stargate) > 0 ? UpgradeTypes::Protoss_Air_Weapons : UpgradeTypes::None;
                firstTech =                                 TechTypes::None;
                firstUnit =                                 Protoss_Corsair;

                // Build
                buildQueue[Protoss_Assimilator] =           (s >= 38) + (atPercent(Protoss_Cybernetics_Core, 0.75));
                buildQueue[Protoss_Cybernetics_Core] =      s >= 36;
                buildQueue[Protoss_Citadel_of_Adun] =       0;
                buildQueue[Protoss_Templar_Archives] =      0;
                buildQueue[Protoss_Stargate] =              (vis(Protoss_Corsair) > 0) + (atPercent(Protoss_Cybernetics_Core, 1.00));

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.60;
                armyComposition[Protoss_Corsair] =          0.25;
                armyComposition[Protoss_High_Templar] =     0.05;
                armyComposition[Protoss_Dark_Templar] =     0.10;
            }
            else if (currentTransition == "5GateGoon") {    // "https://liquipedia.net/starcraft/5_Gate_Ranged_Goons_(vs._Zerg)"
                inOpeningBook =                             s < 160;
                lockedTransition =                          total(Protoss_Gateway) >= 3;
                unitLimits[Protoss_Zealot] =                2;
                unitLimits[Protoss_Dragoon] =               INT_MAX;
                firstUpgrade =                              UpgradeTypes::Singularity_Charge;
                firstTech =                                 TechTypes::None;
                firstUnit =                                 UnitTypes::None;

                // Build
                buildQueue[Protoss_Cybernetics_Core] =      s >= 40;
                buildQueue[Protoss_Gateway] =               (vis(Protoss_Cybernetics_Core) > 0) + (4 * (s >= 64));
                buildQueue[Protoss_Assimilator] =           1 + (s >= 116);

                // Army Composition
                armyComposition[Protoss_Dragoon] =          1.00;
            }
            else if (currentTransition == "NeoBisu") {      // "https://liquipedia.net/starcraft/%2B1_Sair/Speedlot_(vs._Zerg)"
                inOpeningBook =                             s < 100;
                lockedTransition =                          total(Protoss_Citadel_of_Adun) > 0 && total(Protoss_Stargate) > 0;
                firstUpgrade =                              total(Protoss_Stargate) > 0 ? UpgradeTypes::Protoss_Air_Weapons : UpgradeTypes::None;
                firstTech =                                 TechTypes::None;
                firstUnit =                                 Protoss_Corsair;

                // Build
                buildQueue[Protoss_Assimilator] =           (s >= 34) + (atPercent(Protoss_Cybernetics_Core, 0.75));
                buildQueue[Protoss_Gateway] =               1 + (vis(Protoss_Citadel_of_Adun) > 0);
                buildQueue[Protoss_Cybernetics_Core] =      s >= 36;
                buildQueue[Protoss_Citadel_of_Adun] =       vis(Protoss_Corsair) > 0;
                buildQueue[Protoss_Stargate] =              com(Protoss_Cybernetics_Core) >= 1;
                buildQueue[Protoss_Templar_Archives] =      Broodwar->self()->isUpgrading(UpgradeTypes::Leg_Enhancements) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Leg_Enhancements);

                if (vis(Protoss_Templar_Archives) > 0) {
                    techList.insert(Protoss_High_Templar);
                    unlockedType.insert(Protoss_High_Templar);
                    techList.insert(Protoss_Dark_Templar);
                    unlockedType.insert(Protoss_Dark_Templar);
                }

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.60;
                armyComposition[Protoss_Corsair] =          0.15;
                armyComposition[Protoss_High_Templar] =     0.10;
                armyComposition[Protoss_Dark_Templar] =     0.15;
            }
        }
    }

    void PvZ2Gate()
    {
        defaultPvZ();
        proxy =                                             currentOpener == "Proxy" && vis(Protoss_Gateway) < 2 && Broodwar->getFrameCount() < 5000;
        wallNat =                                           vis(Protoss_Nexus) >= 2 || currentOpener == "Natural";
        scout =                                             currentOpener != "Proxy" && Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) >= 1 : vis(Protoss_Gateway) >= 2;
        rush =                                              currentOpener == "Proxy";
        transitionReady =                                   vis(Protoss_Gateway) >= 2;

        // Reactions
        if (!lockedTransition) {
            if (Players::getVisibleCount(PlayerState::Enemy, UnitTypes::Zerg_Sunken_Colony) >= 4)
                currentTransition = "Expand";
        }

        // Openers
        if (currentOpener == "Proxy") {                     // 9/9            
            buildQueue[Protoss_Pylon] =                     (s >= 12) + (s >= 26);
            buildQueue[Protoss_Gateway] =                   (vis(Protoss_Pylon) > 0 && s >= 18) + (vis(Protoss_Gateway) > 0);
        }
        else if (currentOpener == "Natural") {
            if (startCount >= 3) {                          // 9/10
                buildQueue[Protoss_Pylon] =                 (s >= 14) + (s >= 26);
                buildQueue[Protoss_Gateway] =               (vis(Protoss_Pylon) > 0 && s >= 18) + (s >= 20);
            }
            else {                                          // 9/9
                buildQueue[Protoss_Pylon] =                 (s >= 14) + (s >= 26);
                buildQueue[Protoss_Gateway] =               (vis(Protoss_Pylon) > 0 && s >= 18) + (vis(Protoss_Gateway) > 0);
            }
        }
        else if (currentOpener == "Main") {
            if (startCount >= 3) {                          // 10/12
                buildQueue[Protoss_Pylon] =                 (s >= 16) + (s >= 32);
                buildQueue[Protoss_Gateway] =               (s >= 20) + (s >= 24);
            }
            else {                                          // 9/10
                buildQueue[Protoss_Pylon] =                 (s >= 16) + (s >= 26);
                buildQueue[Protoss_Gateway] =               (vis(Protoss_Pylon) > 0 && s >= 18) + (s >= 20);
            }
        }

        // Transitions
        if (transitionReady) {
            if (currentTransition == "Expand") {
                inOpeningBook =                             s < 90;
                lockedTransition =                          vis(Protoss_Nexus) >= 2;
                wallNat =                                   vis(Protoss_Nexus) >= 2 || currentOpener == "Natural";
                firstUnit =                                 None;

                buildQueue[Protoss_Nexus] =                 1 + (s >= 42);
                buildQueue[Protoss_Forge] =                 s >= 62;
                buildQueue[Protoss_Cybernetics_Core] =      vis(Protoss_Photon_Cannon) >= 2;

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.25;
                armyComposition[Protoss_Dragoon] =          0.75;
            }
            else if (currentTransition == "4Gate") {        // "https://liquipedia.net/starcraft/4_Gate_Goon_(vs._Protoss)"        
                firstUnit =                                 None;
                inOpeningBook =                             s < 120;
                lockedTransition =                          true;
                firstUpgrade =                              UpgradeTypes::Singularity_Charge;
                unitLimits[Protoss_Zealot] =                5;
                unitLimits[Protoss_Dragoon] =               INT_MAX;
                wallNat =                                   vis(Protoss_Nexus) >= 2 || currentOpener == "Natural";
                playPassive =                               !firstReady() && (!Terrain::foundEnemy() || Strategy::enemyPressure());

                buildQueue[Protoss_Gateway] =               2 + (s >= 62) + (s >= 70);
                buildQueue[Protoss_Assimilator] =           s >= 52;
                buildQueue[Protoss_Cybernetics_Core] =      vis(Protoss_Zealot) >= 5;

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.25;
                armyComposition[Protoss_Dragoon] =          0.75;
            }
        }
    }

    void PvZ1GateCore()
    {
        defaultPvZ();
        scout =                                             Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
        gasLimit =                                          INT_MAX;

        // Reactions
        if (!lockedTransition) {

            // If enemy is rushing, pressuring or stole gas
            if (Strategy::enemyRush() || Strategy::enemyPressure() || Strategy::enemyGasSteal()) {
                currentBuild = "2Gate";
                currentOpener = "Main";
                currentTransition = "4Gate";
            }
        }

        // Openers
        if (currentOpener == "1Zealot") {
            unitLimits[Protoss_Zealot] =                    vis(Protoss_Cybernetics_Core) > 0 ? INT_MAX : 1;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 34;
        }
        else if (currentOpener == "2Zealot") {
            unitLimits[Protoss_Zealot] =                    vis(Protoss_Cybernetics_Core) > 0 ? INT_MAX : 2;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 24);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 32;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 40;
        }

        // Transitions
        if (currentTransition == "DT") {                    // Experimental build from Best            
            firstUpgrade =                                  UpgradeTypes::None;
            firstTech =                                     vis(Protoss_Dark_Templar) >= 2 ? TechTypes::Psionic_Storm : TechTypes::None;
            inOpeningBook =                                 s < 70;
            unitLimits[Protoss_Dragoon] =                   1;
            lockedTransition =                              total(Protoss_Citadel_of_Adun) > 0;
            playPassive =                                   s < 70;
            firstUnit =                                     Protoss_Dark_Templar;

            // Build
            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 42);
            buildQueue[Protoss_Citadel_of_Adun] =           s >= 34;
            buildQueue[Protoss_Templar_Archives] =          vis(Protoss_Gateway) >= 2;

            // Army Composition
            armyComposition[Protoss_Zealot] =               0.80;
            armyComposition[Protoss_Dragoon] =              0.10;
            armyComposition[Protoss_Dark_Templar] =         0.10;
        }
    }
}