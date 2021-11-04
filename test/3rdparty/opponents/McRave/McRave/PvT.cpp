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

        void defaultPvT() {
            inOpeningBook =                                 true;
            inBookSupply =                                  vis(Protoss_Pylon) < 2;
            wallNat =                                       com(Protoss_Pylon) >= 6 || currentOpener == "Natural";
            wallMain =                                      false;
            scout =                                         vis(Protoss_Cybernetics_Core) > 0;
            wantNatural =                                   true;
            wantThird =                                     true;
            proxy =                                         false;
            hideTech =                                      false;
            playPassive =                                   false;
            rush =                                          false;
            cutWorkers =                                    false;
            transitionReady =                               false;

            gasLimit =                                      INT_MAX;
            unitLimits[Protoss_Zealot] =                    0;
            unitLimits[Protoss_Dragoon] =                   INT_MAX;

            desiredDetection =                              Protoss_Observer;
            firstUpgrade =                                  UpgradeTypes::Singularity_Charge;
            firstTech =                                     TechTypes::None;
            firstUnit =                                     None;

            armyComposition[Protoss_Dragoon] =              1.00;
        }
    }

    void PvT2GateDefensive() {
        gasLimit =                                          com(Protoss_Cybernetics_Core) > 0 && s >= 50 ? INT_MAX : 0;
        inOpeningBook =                                     s < 80;
        playPassive =                                       false;
        firstUpgrade =                                      UpgradeTypes::None;
        firstTech =                                         TechTypes::None;
        firstUnit =                                         None;
        wantNatural =                                       false;
        wantThird =                                         false;

        unitLimits[Protoss_Zealot] =                        s >= 50 ? 0 : INT_MAX;
        unitLimits[Protoss_Dragoon] =                       s >= 50 ? INT_MAX : 0;

        buildQueue[Protoss_Nexus] =                         1;
        buildQueue[Protoss_Pylon] =                         (s >= 16) + (s >= 30);
        buildQueue[Protoss_Gateway] =                       (s >= 20) + (s >= 24) + (s >= 66);
        buildQueue[Protoss_Assimilator] =                   s >= 40;
        buildQueue[Protoss_Shield_Battery] =                vis(Protoss_Zealot) >= 2 && vis(Protoss_Pylon) >= 2;
        buildQueue[Protoss_Cybernetics_Core] =              s >= 58;
    }

    void PvT2Gate()
    {
        // "https://liquipedia.net/starcraft/10/15_Gates_(vs._Terran)"
        defaultPvT();
        proxy =                                             currentOpener == "Proxy" && vis(Protoss_Gateway) < 2 && Broodwar->getFrameCount() < 5000;
        scout =                                             currentOpener != "Proxy" && Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) >= 1 : vis(Protoss_Gateway) >= 2;
        rush =                                              currentOpener == "Proxy";
        playPassive =                                       Strategy::enemyPressure() && Util::getTime() < Time(5, 0);

        // Reactions
        if (!lockedTransition) {
            if (Strategy::enemyFastExpand())
                currentTransition = "DT";
        }

        // Openers
        if (currentOpener == "Proxy") {
            buildQueue[Protoss_Pylon] =                     (s >= 12) + (s >= 30), (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   (vis(Protoss_Pylon) > 0 && s >= 18) + (vis(Protoss_Gateway) > 0), 2 * (s >= 18);
        }
        else if (currentOpener == "Main") {
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 30);
        }

        // Transitions
        if (currentTransition == "DT") {
            playPassive =                                   Strategy::getEnemyBuild() == "2Rax" && vis(Protoss_Dark_Templar) == 0;
            unitLimits[Protoss_Zealot] =                                   Strategy::getEnemyBuild() == "2Rax" ? 2 : 0;
            gasLimit =                                      Strategy::getEnemyBuild() == "2Rax" && total(Protoss_Gateway) < 2 ? 0 : 3;
            lockedTransition =                              total(Protoss_Citadel_of_Adun) > 0;
            inOpeningBook =                                 s < 70;
            firstUnit =                                     Protoss_Dark_Templar;
            hideTech =                                      true;

            // Build
            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Assimilator] =               s >= 22;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
            buildQueue[Protoss_Citadel_of_Adun] =           vis(Protoss_Dragoon) >= 3;
            buildQueue[Protoss_Templar_Archives] =          atPercent(Protoss_Citadel_of_Adun, 1.00);

            // Army composition
            armyComposition[Protoss_Dragoon] =              1.00;
        }
        else if (currentTransition == "Robo") {
            lockedTransition =                              total(Protoss_Robotics_Facility) > 0;
            inOpeningBook =                                 s < 70;
            firstUnit =                                     Protoss_Reaver;

            // Build
            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Assimilator] =               s >= 22;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
            buildQueue[Protoss_Robotics_Facility] =         vis(Protoss_Dragoon) >= 3;

            // Army Composition
            armyComposition[Protoss_Zealot] =               0.05;
            armyComposition[Protoss_Reaver] =               0.10;
            armyComposition[Protoss_Observer] =             0.05;
            armyComposition[Protoss_Shuttle] =              0.05;
            armyComposition[Protoss_Dragoon] =              0.75;

        }
        else if (currentTransition == "Expand") {
            lockedTransition =                              total(Protoss_Nexus) >= 2;
            inOpeningBook =                                 s < 100;
            firstUnit =                                     None;

            // Build
            buildQueue[Protoss_Nexus] =                     1 + (s >= 50);
            buildQueue[Protoss_Assimilator] =               s >= 22;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;

            // Army composition
            armyComposition[Protoss_Dragoon] =              1.00;
        }
        else if (currentTransition == "DoubleExpand") {
            lockedTransition =                              total(Protoss_Nexus) >= 3;
            inOpeningBook =                                 s < 120;
            gasLimit =                                      3;
            firstUnit =                                     None;

            // Build
            buildQueue[Protoss_Nexus] =                     1 + (s >= 50) + (s >= 50);
            buildQueue[Protoss_Assimilator] =               s >= 22;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;

            // Army composition
            armyComposition[Protoss_Dragoon] =              1.00;
        }
    }

    void PvT1GateCore()
    {
        // "https://liquipedia.net/starcraft/1_Gate_Core_(vs._Terran)"
        defaultPvT();
        firstUpgrade =                                      UpgradeTypes::Singularity_Charge;
        firstTech =                                         TechTypes::None;
        scout =                                             Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;

        // Reactions
        if (!lockedTransition) {
            if (Strategy::enemyRush()) {
                currentBuild = "2Gate";
                currentOpener = "Main";
                currentTransition = "DT";
            }
            else if (Strategy::enemyFastExpand())
                currentTransition = "DT";
        }

        // Openers
        if (currentOpener == "0Zealot") {                   // NZCore            
            unitLimits[Protoss_Zealot] = 0;
            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
        }
        else if (currentOpener == "1Zealot") {              // ZCore            
            unitLimits[Protoss_Zealot] = 1;
            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 36;
        }

        // Transitions
        if (currentTransition == "Robo") {                  // "http://liquipedia.net/starcraft/1_Gate_Reaver"            
            lockedTransition =                              total(Protoss_Robotics_Facility) > 0;
            inOpeningBook =                                 s < 60;
            hideTech =                                      com(Protoss_Reaver) <= 0;
            firstUnit =                                     Strategy::enemyPressure() ? Protoss_Reaver : Protoss_Observer;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 74);
            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 60) + (s >= 62);
            buildQueue[Protoss_Robotics_Facility] =         s >= 52;

            // Army Composition
            armyComposition[Protoss_Zealot] =               0.05;
            armyComposition[Protoss_Reaver] =               0.10;
            armyComposition[Protoss_Observer] =             0.05;
            armyComposition[Protoss_Shuttle] =              0.05;
            armyComposition[Protoss_Dragoon] =              0.75;
        }
        else if (currentTransition == "4Gate") {            // "https://liquipedia.net/starcraft/4_Gate_Goon_(vs._Protoss)"            
            lockedTransition =                              total(Protoss_Gateway) >= 4;
            inOpeningBook =                                 s < 80;
            firstUnit =                                     None;

            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 30) + (2 * (s >= 62));
            buildQueue[Protoss_Assimilator] =               s >= 22;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;

            // Army composition
            armyComposition[Protoss_Dragoon] =              1.00;
        }
        else if (currentTransition == "DT") {               // "https://liquipedia.net/starcraft/DT_Fast_Expand_(vs._Terran)"            
            lockedTransition =                              total(Protoss_Citadel_of_Adun) > 0;
            inOpeningBook =                                 s <= 80;
            hideTech =                                      com(Protoss_Dark_Templar) <= 0;
            firstUnit =                                     Protoss_Dark_Templar;
            firstUpgrade =                                  vis(Protoss_Dark_Templar) >= 2 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
            playPassive =                                   Strategy::enemyPressure() ? vis(Protoss_Observer) == 0 : false;

            buildQueue[Protoss_Gateway] =                   (s >= 20) + (vis(Protoss_Templar_Archives) > 0);
            buildQueue[Protoss_Nexus] =                     1 + (vis(Protoss_Dark_Templar) > 0);
            buildQueue[Protoss_Assimilator] =               (s >= 24) + (vis(Protoss_Nexus) >= 2);
            buildQueue[Protoss_Citadel_of_Adun] =           s >= 36;
            buildQueue[Protoss_Templar_Archives] =          s >= 48;

            // Army composition
            armyComposition[Protoss_Dragoon] =              1.00;
        }
    }

    void PvTNexusGate()
    {
        // "http://liquipedia.net/starcraft/12_Nexus"
        defaultPvT();
        playPassive =                                       Strategy::enemyPressure() ? vis(Protoss_Dragoon) < 12 : !firstReady() && vis(Protoss_Dragoon) < 4;
        firstUpgrade =                                      vis(Protoss_Dragoon) >= 1 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
        cutWorkers =                                        s >= 44 && s < 48;
        gasLimit =                                          goonRange() && com(Protoss_Nexus) < 2 ? 2 : INT_MAX;
        unitLimits[Protoss_Zealot] =                        currentOpener == "Zealot" ? 1 : 0;
        scout =                                             vis(Protoss_Pylon) > 0;

        // Reactions
        if (!lockedTransition) {
            if (Strategy::enemyFastExpand() || Strategy::getEnemyTransition() == "SiegeExpand")
                currentTransition = s < 50 ? "DoubleExpand" : "ReaverCarrier";
            else if (Terrain::getEnemyStartingPosition().isValid() && !Strategy::enemyFastExpand() && currentTransition == "DoubleExpand")
                currentTransition = "Standard";
            else if (Strategy::enemyPressure())
                currentTransition = "Standard";
        }

        // Openers
        if (currentOpener == "Zealot") {
            transitionReady =                               vis(Protoss_Gateway) >= 2;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 24);
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 44);
            buildQueue[Protoss_Assimilator] =               s >= 30;
            buildQueue[Protoss_Gateway] =                   (s >= 28) + (s >= 34);
            buildQueue[Protoss_Cybernetics_Core] =          vis(Protoss_Gateway) >= 2;
        }
        else if (currentOpener == "Dragoon") {
            transitionReady =                               vis(Protoss_Gateway) >= 2;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 24);
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 44);
            buildQueue[Protoss_Assimilator] =               vis(Protoss_Gateway) >= 1;
            buildQueue[Protoss_Gateway] =                   (s >= 26) + (s >= 32);
            buildQueue[Protoss_Cybernetics_Core] =          s >= 30;
        }

        // Transitions
        if (transitionReady) {
            if (currentTransition == "DoubleExpand") {
                lockedTransition =                          total(Protoss_Nexus) >= 3;
                inOpeningBook =                             s < 120;
                firstUnit =                                 None;

                // Build
                buildQueue[Protoss_Nexus] =                 1 + (s >= 24) + (com(Protoss_Cybernetics_Core) > 0);
                buildQueue[Protoss_Assimilator] =           s >= 26;

                // Army composition
                armyComposition[Protoss_Dragoon] =          1.00;

            }
            else if (currentTransition == "Standard") {
                inOpeningBook =                             s < 100;
                firstUnit =                                 None;

                // Build
                buildQueue[Protoss_Assimilator] =           1 + (s >= 86);
                buildQueue[Protoss_Gateway] =               2 + (s >= 80);

                // Army composition
                armyComposition[Protoss_Dragoon] =          1.00;
            }
            else if (currentTransition == "ReaverCarrier") {
                inOpeningBook =                             s < 120;
                lockedTransition =                          total(Protoss_Stargate) > 0;
                firstUnit =                                 Players::getVisibleCount(PlayerState::Enemy, Terran_Medic) > 0 && vis(Protoss_Reaver) == 0 ? Protoss_Reaver : Protoss_Carrier;

                // Build
                buildQueue[Protoss_Assimilator] =           1 + (s >= 56);
                if (firstUnit == Protoss_Carrier) {
                    buildQueue[Protoss_Stargate] =          (s >= 80) + (vis(Protoss_Carrier) > 0);
                    buildQueue[Protoss_Fleet_Beacon] =      atPercent(Protoss_Stargate, 1.00);
                }

                if (firstUnit == Protoss_Reaver)
                    buildQueue[Protoss_Robotics_Facility] = s >= 58;

                // Army composition
                armyComposition[Protoss_Dragoon] =          1.00;
            }
        }
    }

    void PvTGateNexus()
    {
        defaultPvT();
        firstUpgrade =                                      UpgradeTypes::Singularity_Charge;
        firstTech =                                         TechTypes::None;
        unitLimits[Protoss_Zealot] =                        0;

        // Reactions
        if (!lockedTransition) {
            if (Strategy::enemyFastExpand() || Strategy::getEnemyTransition() == "SiegeExpand")
                currentTransition = "Carrier";
            else if ((!Strategy::enemyFastExpand() && Terrain::foundEnemy() && currentTransition == "DoubleExpand") || Strategy::enemyPressure())
                currentTransition = "Standard";

            if (s < 42 && Strategy::getEnemyBuild() == "2Rax") {
                currentBuild = "2Gate";
                currentOpener = "Main";
                currentTransition = "DT";
            }
        }

        // Openers
        if (currentOpener == "1Gate") {                     // "http://liquipedia.net/starcraft/21_Nexus"            
            buildQueue[Protoss_Nexus] =                     1 + (s >= 42);
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   (s >= 20) + (vis(Protoss_Nexus) >= 2) + (s >= 76);

            playPassive =                                   (Strategy::enemyPressure() ? vis(Protoss_Dragoon) < 16 : !firstReady()) || (com(Protoss_Dragoon) < 4 && !Strategy::enemyFastExpand());
            scout =                                         Broodwar->getStartLocations().size() == 4 ? vis(Protoss_Pylon) > 0 : vis(Protoss_Pylon) > 0;
            gasLimit =                                      goonRange() && vis(Protoss_Nexus) < 2 ? 2 : INT_MAX;
            unitLimits[Protoss_Dragoon] =                   Util::getTime() > Time(4, 0) || vis(Protoss_Nexus) >= 2 ? INT_MAX : 1;
            cutWorkers =                                    vis(Protoss_Probe) >= 19 && s < 46;
        }
        else if (currentOpener == "2Gate") {                // "https://liquipedia.net/starcraft/2_Gate_Range_Expand"

            buildQueue[Protoss_Nexus] =                     1 + (s >= 40);
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 30) + (s >= 48);
            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 34) + (s >= 76);

            scout =                                         vis(Protoss_Cybernetics_Core) > 0;
            inBookSupply =                                  vis(Protoss_Pylon) < 3;
            gasLimit =                                      goonRange() && vis(Protoss_Pylon) < 3 ? 2 : INT_MAX;
            unitLimits[Protoss_Dragoon] =                   Util::getTime() > Time(4, 0) || vis(Protoss_Nexus) >= 2 || s >= 40 ? INT_MAX : 0;
            cutWorkers =                                    vis(Protoss_Probe) >= 20 && s < 48;
        }

        // Transitions
        if (currentTransition == "DoubleExpand") {
            firstUnit =                                     None;
            inOpeningBook =                                 s < 140;
            playPassive =                                   s < 100;
            lockedTransition =                              total(Protoss_Nexus) >= 3;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 42) + (s >= 70);
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
        }
        else if (currentTransition == "Standard") {
            inOpeningBook =                                 s < 80;
            firstUnit =                                     Protoss_Observer;
            lockedTransition =                              total(Protoss_Nexus) >= 2;

            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
            buildQueue[Protoss_Robotics_Facility] =         s >= 62;
        }
        else if (currentTransition == "Carrier") {
            inOpeningBook =                                 s < 160;
            firstUnit =                                     Protoss_Carrier;
            lockedTransition =                              total(Protoss_Stargate) > 0;

            buildQueue[Protoss_Assimilator] =               (s >= 24) + (s >= 60);
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
            buildQueue[Protoss_Stargate] =                  vis(Protoss_Dragoon) >= 6;
            buildQueue[Protoss_Fleet_Beacon] =              atPercent(Protoss_Stargate, 1.00);
        }
    }
}