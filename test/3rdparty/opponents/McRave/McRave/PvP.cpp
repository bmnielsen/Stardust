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

        bool enemyMoreZealots() {
            return com(Protoss_Zealot) <= Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) || Strategy::enemyProxy();
        }

        bool enemyMaybeDT() {
            return (Players::getVisibleCount(PlayerState::Enemy, Protoss_Citadel_of_Adun) > 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) <= 2)
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) < 3
                || Strategy::enemyInvis()
                || Strategy::getEnemyTransition() == "DT";
        }

        void defaultPvP() {
            inOpeningBook =                                 true;
            inBookSupply =                                  vis(Protoss_Pylon) < 2;
            wallNat =                                       vis(Protoss_Nexus) >= 2;
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
            unitLimits[Protoss_Zealot] =                    1;
            unitLimits[Protoss_Dragoon] =                   INT_MAX;

            desiredDetection =                              Protoss_Observer;
            firstUpgrade =                                  vis(Protoss_Dragoon) > 0 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
            firstTech =                                     TechTypes::None;
            firstUnit =                                     None;

            armyComposition[Protoss_Zealot] =               0.10;
            armyComposition[Protoss_Dragoon] =              0.90;
        }
    }

    void PvP2GateDefensive() {

        lockedTransition =                                  true;

        // Make a tech decision before 3:30
        if (Util::getTime() < Time(3, 30))
            firstUnit =                                     (Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) > 0 || Players::getVisibleCount(PlayerState::Enemy, Protoss_Assimilator) > 0) ? Protoss_Reaver : None;

        gasLimit =                                          (2 * (vis(Protoss_Cybernetics_Core) > 0 && s >= 46)) + (com(Protoss_Cybernetics_Core) > 0);
        inOpeningBook =                                     s < 100;
        playPassive    =                                    enemyMoreZealots() && s < 100 && com(Protoss_Dragoon) < 4;
        firstUpgrade =                                      com(Protoss_Dragoon) >= 2 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
        firstTech =                                         TechTypes::None;
        wantNatural =                                       false;
        wantThird =                                         false;
        rush =                                              false;

        unitLimits[Protoss_Zealot] =                        s > 80 ? 0 : INT_MAX;
        unitLimits[Protoss_Dragoon] =                       s > 60 ? INT_MAX : 0;

        desiredDetection =                                  Protoss_Forge;
        cutWorkers =                                        Util::getTime() < Time(3, 30) && enemyMoreZealots() && Production::hasIdleProduction();

        buildQueue[Protoss_Nexus] =                         1;
        buildQueue[Protoss_Pylon] =                         (s >= 14) + (s >= 30), (s >= 16) + (s >= 30);
        buildQueue[Protoss_Gateway] =                       (s >= 20) + (s >= 24) + (s >= 66);
        buildQueue[Protoss_Assimilator] =                   vis(Protoss_Zealot) >= 5;
        buildQueue[Protoss_Cybernetics_Core] =              vis(Protoss_Assimilator) >= 1;
        buildQueue[Protoss_Shield_Battery] =                vis(Protoss_Zealot) >= 2 && vis(Protoss_Pylon) >= 2;

        if (firstUnit == Protoss_Reaver)
            buildQueue[Protoss_Robotics_Facility] =         com(Protoss_Dragoon) >= 2;

        // Army Composition
        armyComposition[Protoss_Zealot] =                   0.05;
        armyComposition[Protoss_Reaver] =                   0.10;
        armyComposition[Protoss_Observer] =                 0.05;
        armyComposition[Protoss_Shuttle] =                  0.05;
        armyComposition[Protoss_Dragoon] =                  0.75;
    }

    void PvP2Gate()
    {
        // "https://liquipedia.net/starcraft/2_Gate_(vs._Protoss)"
        defaultPvP();
        unitLimits[Protoss_Zealot] =                        s <= 80 ? 7 : 0;
        proxy =                                             currentOpener == "Proxy" && vis(Protoss_Gateway) < 2 && Broodwar->getFrameCount() < 5000;
        scout =                                             currentOpener != "Proxy" && startCount >= 3 ? vis(Protoss_Gateway) >= 1 : vis(Protoss_Gateway) >= 2;
        transitionReady =                                   vis(Protoss_Gateway) >= 2;
        desiredDetection =                                  Protoss_Forge;
        gasLimit =                                          total(Protoss_Zealot) >= 3 ? INT_MAX : 0;

        if (Strategy::enemyRush())
            buildQueue[Protoss_Shield_Battery] =            vis(Protoss_Zealot) >= 2 && vis(Protoss_Pylon) >= 2;

        // Reactions
        if (!lockedTransition) {
            if (Strategy::getEnemyBuild() == "FFE" || Strategy::getEnemyTransition() == "DT")
                currentTransition = "Robo";
            else if (Strategy::getEnemyBuild() == "CannonRush")
                currentTransition = "Robo";
            else if (Strategy::enemyPressure())
                currentTransition = "DT";
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
            if (currentTransition == "DT") {
                lockedTransition =                          total(Protoss_Citadel_of_Adun) > 0;
                inOpeningBook =                             s < 80;
                firstUpgrade =                              UpgradeTypes::None;
                firstUnit =                                 Protoss_Dark_Templar;
                wallNat =                                   (com(Protoss_Forge) > 0 && com(Protoss_Dark_Templar) > 0);
                hideTech =                                  com(Protoss_Dark_Templar) <= 0;

                // Build
                buildQueue[Protoss_Gateway] =               2 + (com(Protoss_Cybernetics_Core) > 0);
                buildQueue[Protoss_Nexus] =                 1;
                buildQueue[Protoss_Assimilator] =           total(Protoss_Zealot) >= 3;
                buildQueue[Protoss_Cybernetics_Core] =      (total(Protoss_Zealot) >= 5 && vis(Protoss_Assimilator) >= 1);
                buildQueue[Protoss_Citadel_of_Adun] =       atPercent(Protoss_Cybernetics_Core, 1.00);
                buildQueue[Protoss_Templar_Archives] =      atPercent(Protoss_Citadel_of_Adun, 1.00);
                buildQueue[Protoss_Forge] =                 s >= 70;

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.90;
                armyComposition[Protoss_Dark_Templar] =     0.10;
                armyComposition[Protoss_Dragoon] =          0.00;
            }
            else if (currentTransition == "Expand") {       // "https://liquipedia.net/starcraft/2_Gate_(vs._Protoss)#10.2F12_Gateway_Expand"            
                lockedTransition =                          total(Protoss_Nexus) >= 2;
                inOpeningBook =                             s < 80;
                firstUnit =                                 None;
                wallNat =                                   currentOpener == "Natural" || s >= 56;

                // Build
                buildQueue[Protoss_Gateway] =               2 + (com(Protoss_Cybernetics_Core) > 0);
                buildQueue[Protoss_Assimilator] =           total(Protoss_Zealot) >= 3;
                buildQueue[Protoss_Cybernetics_Core] =      (total(Protoss_Zealot) >= 5 && vis(Protoss_Assimilator) >= 1);
                buildQueue[Protoss_Forge] =                 s >= 70;
                buildQueue[Protoss_Nexus] =                 1 + (vis(Protoss_Zealot) >= 3);

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.10;
                armyComposition[Protoss_Dragoon] =          0.90;
            }
            else if (currentTransition == "Robo") {         // "https://liquipedia.net/starcraft/2_Gate_Reaver_(vs._Protoss)"            
                lockedTransition =                          total(Protoss_Robotics_Facility) > 0;
                inOpeningBook =                             s < 80;
                firstUnit =                                 enemyMaybeDT() ? Protoss_Observer : Protoss_Reaver;

                // Build
                buildQueue[Protoss_Gateway] =               2 + (com(Protoss_Cybernetics_Core) > 0);
                buildQueue[Protoss_Nexus] =                 1;
                buildQueue[Protoss_Assimilator] =           total(Protoss_Zealot) >= 3;
                buildQueue[Protoss_Cybernetics_Core] =      (total(Protoss_Zealot) >= 5 && vis(Protoss_Assimilator) >= 1);
                buildQueue[Protoss_Robotics_Facility] =     com(Protoss_Dragoon) >= 2;

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.05;
                armyComposition[Protoss_Reaver] =           0.10;
                armyComposition[Protoss_Observer] =         0.05;
                armyComposition[Protoss_Shuttle] =          0.05;
                armyComposition[Protoss_Dragoon] =          0.75;
            }
        }
    }

    void PvP1GateCore()
    {
        // "https://liquipedia.net/starcraft/1_Gate_Core_(vs._Protoss)"
        defaultPvP();

        // Reactions
        if (!lockedTransition) {

            // If enemy is rushing us
            if (Strategy::getEnemyBuild() == "2Gate" && vis(Protoss_Cybernetics_Core) == 0) {
                currentBuild = "2Gate";
                currentOpener = "Main";
                currentTransition = "Robo";
            }
            else if (Strategy::getEnemyBuild() == "2Gate" && vis(Protoss_Cybernetics_Core) > 0)
                currentTransition = "3Gate";

            // If our 4Gate would likely kill us
            else if (Scouts::enemyDeniedScout() && currentTransition == "4Gate")
                currentTransition = "Robo";

            // If we didn't see enemy info by 3:30
            else if (!Scouts::gotFullScout() && Util::getTime() > Time(3, 30))
                currentTransition = "Robo";

            // If we're not doing Robo vs potential DT, switch
            else if (currentTransition != "Robo" && Players::getVisibleCount(PlayerState::Enemy, Protoss_Citadel_of_Adun) > 0)
                currentTransition = "Robo";

            // If we see a FFE, 3Gate with an expansion
            else if (Strategy::getEnemyBuild() == "FFE")
                currentTransition = "3Gate";
        }

        // Openers
        if (currentOpener == "0Zealot") {                   // NZCore
            unitLimits[Protoss_Zealot] =                    0;
            scout =                                         vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Cybernetics_Core) > 0;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
        }
        else if (currentOpener == "1Zealot") {              // ZCore
            unitLimits[Protoss_Zealot] =                    s >= 60 ? 0 : 1;
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Cybernetics_Core) > 0;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 34;
        }
        else if (currentOpener == "2Zealot") {              // ZCoreZ
            unitLimits[Protoss_Zealot] =                    s >= 60 ? 0 : 1 + (vis(Protoss_Cybernetics_Core) > 0);
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Cybernetics_Core) > 0;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 34;
        }

        // Transitions
        if (transitionReady) {
            if (currentTransition == "Robo") {              // "https://liquipedia.net/starcraft/2_Gate_Reaver_(vs._Protoss)"   
                firstUnit =                                 enemyMaybeDT() ? Protoss_Observer : Protoss_Reaver;
                lockedTransition =                          total(Protoss_Robotics_Facility) > 0;
                inOpeningBook =                             com(firstUnit) == 0;
                playPassive =                               com(firstUnit) == 0;

                // Build
                buildQueue[Protoss_Gateway] =               (s >= 20) + (vis(Protoss_Robotics_Facility) > 0);
                buildQueue[Protoss_Robotics_Facility] =     s >= 50;

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.05;
                armyComposition[Protoss_Reaver] =           0.10;
                armyComposition[Protoss_Observer] =         0.05;
                armyComposition[Protoss_Shuttle] =          0.05;
                armyComposition[Protoss_Dragoon] =          0.75;
            }
            else if (currentTransition == "3Gate") {        // -nolink-
                firstUnit =                                 None;
                lockedTransition =                          total(Protoss_Gateway) >= 3;
                inOpeningBook =                             Strategy::enemyPressure() ? Broodwar->getFrameCount() < 9000 : Broodwar->getFrameCount() < 8000;
                playPassive =                               Strategy::enemyPressure() ? Broodwar->getFrameCount() < 13000 : false;
                gasLimit =                                  vis(Protoss_Gateway) >= 2 && com(Protoss_Gateway) < 3 ? 2 : INT_MAX;
                wallNat =                                   Util::getTime() > Time(4, 30);

                // Build
                buildQueue[Protoss_Gateway] =               (s >= 20) + (s >= 38) + (s >= 40);

                // Army Composition
                armyComposition[Protoss_Zealot] = 0.05;
                armyComposition[Protoss_Dragoon] = 0.95;
            }
            else if (currentTransition == "4Gate") {        // "https://liquipedia.net/starcraft/4_Gate_Goon_(vs._Protoss)" 
                firstUnit =                                 None;
                lockedTransition =                          total(Protoss_Gateway) >= 3;
                inOpeningBook =                             s < 140;
                playPassive =                               !firstReady();
                desiredDetection =                          Protoss_Forge;

                // Build
                if (Strategy::getEnemyBuild() == "2Gate") {
                    unitLimits[Protoss_Zealot] =            s < 60 ? 4 : 0;
                    gasLimit =                              vis(Protoss_Dragoon) > 0 ? 3 : 1;
                    playPassive =                           com(Protoss_Dragoon) < 2;
                    buildQueue[Protoss_Shield_Battery] =    enemyMoreZealots() && vis(Protoss_Zealot) >= 2 && vis(Protoss_Pylon) >= 2;
                    buildQueue[Protoss_Gateway] =           (s >= 20) + (vis(Protoss_Pylon) >= 3) + (2 * (s >= 62));
                    buildQueue[Protoss_Cybernetics_Core] =  s >= 34;
                }
                else {
                    buildQueue[Protoss_Shield_Battery] =    0;
                    buildQueue[Protoss_Gateway] =           (s >= 20) + (s >= 40) + (2 * (s >= 62));
                    buildQueue[Protoss_Cybernetics_Core] =  s >= 34;
                }

                // Army Composition
                armyComposition[Protoss_Zealot] = 0.05;
                armyComposition[Protoss_Dragoon] = 0.95;
            }
            else if (currentTransition == "DT") {           // "https://liquipedia.net/starcraft/2_Gate_DT_(vs._Protoss)"
                firstUnit =                                 Protoss_Dark_Templar;
                lockedTransition =                          total(Protoss_Citadel_of_Adun) > 0;
                inOpeningBook =                             s < 90;
                playPassive =                               Broodwar->getFrameCount() < 13500;
                wallNat =                                   (com(Protoss_Forge) > 0 && com(Protoss_Dark_Templar) > 0);
                desiredDetection =                          Protoss_Forge;
                firstUpgrade =                              UpgradeTypes::None;
                hideTech =                                  com(Protoss_Dark_Templar) <= 0;
                unitLimits[Protoss_Zealot] =                vis(Protoss_Photon_Cannon) >= 2 && s < 60 ? INT_MAX : unitLimits[Protoss_Zealot];

                // Build
                buildQueue[Protoss_Gateway] =               (s >= 20) + (vis(Protoss_Templar_Archives) > 0);
                buildQueue[Protoss_Forge] =                 s >= 70;
                buildQueue[Protoss_Photon_Cannon] =         2 * (com(Protoss_Forge) > 0);
                buildQueue[Protoss_Citadel_of_Adun] =       vis(Protoss_Dragoon) > 0;
                buildQueue[Protoss_Templar_Archives] =      atPercent(Protoss_Citadel_of_Adun, 1.00);

                // Army Composition
                armyComposition[Protoss_Zealot] =           0.05;
                armyComposition[Protoss_Dark_Templar] =     0.05;
                armyComposition[Protoss_Dragoon] =          0.90;
            }
        }
    }
}