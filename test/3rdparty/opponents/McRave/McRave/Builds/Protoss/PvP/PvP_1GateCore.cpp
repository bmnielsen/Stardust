#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    namespace {

        void PvP_1GC_Robo()
        {
            // "https://liquipedia.net/starcraft/2_Gate_Reaver_(vs._Protoss)"
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

        void PvP_1GC_3Gate()
        {
            // -nolink-
            firstUnit =                                 None;
            lockedTransition =                          total(Protoss_Gateway) >= 3;
            inOpeningBook =                             Spy::enemyPressure() ? Broodwar->getFrameCount() < 9000 : Broodwar->getFrameCount() < 8000;
            playPassive =                               Spy::enemyPressure() ? Broodwar->getFrameCount() < 13000 : false;
            gasLimit =                                  vis(Protoss_Gateway) >= 2 && com(Protoss_Gateway) < 3 ? 2 : INT_MAX;
            wallNat =                                   Util::getTime() > Time(4, 30);

            // Build
            buildQueue[Protoss_Gateway] =               (s >= 20) + (s >= 38) + (s >= 40);

            // Army Composition
            armyComposition[Protoss_Zealot] = 0.05;
            armyComposition[Protoss_Dragoon] = 0.95;
        }

        void PvP_1GC_4Gate()
        {
            // "https://liquipedia.net/starcraft/4_Gate_Goon_(vs._Protoss)"
            firstUnit =                                 None;
            lockedTransition =                          total(Protoss_Gateway) >= 3;
            inOpeningBook =                             s < 140;
            playPassive =                               !firstReady();
            desiredDetection =                          Protoss_Forge;

            // Build
            if (Spy::getEnemyBuild() == "2Gate") {
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

        void PvP_1GC_DT()
        {
            // "https://liquipedia.net/starcraft/2_Gate_DT_(vs._Protoss)"
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

        void PvP_1GC_NZCore()
        {
            // "https://liquipedia.net/starcraft/1_Gate_Core_(vs._Protoss)"
            unitLimits[Protoss_Zealot] =                    0;
            scout =                                         vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Cybernetics_Core) > 0;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
        }

        void PvP_1GC_ZCore()
        {
            // "https://liquipedia.net/starcraft/1_Gate_Core_(vs._Protoss)"
            unitLimits[Protoss_Zealot] =                    s >= 60 ? 0 : 1;
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Cybernetics_Core) > 0;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 34;
        }

        void PvP_1GC_ZZCore()
        {
            unitLimits[Protoss_Zealot] =                    s >= 60 ? 0 : 1 + (vis(Protoss_Cybernetics_Core) > 0);
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Cybernetics_Core) > 0;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 34;
        }
    }

    void PvP_1GC()
    {
        // Reactions
        if (!lockedTransition) {

            // If enemy is 2gate, switch to some form of 2gate if we haven't gone core yet
            if (Spy::getEnemyBuild() == "2Gate" && vis(Protoss_Cybernetics_Core) == 0) {
                currentBuild = "2Gate";
                currentOpener = "Main";
                currentTransition = "Robo";
            }

            // Otherwise try to go 3gate after core
            else if (Spy::getEnemyBuild() == "2Gate" && vis(Protoss_Cybernetics_Core) > 0)
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
            else if (Spy::getEnemyBuild() == "FFE")
                currentTransition = "3Gate";
        }

        // Openers
        if (currentOpener == "0Zealot")
            PvP_1GC_NZCore();
        if (currentOpener == "1Zealot")
            PvP_1GC_ZCore();
        if (currentOpener == "2Zealot")
            PvP_1GC_ZZCore();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "Robo")
                PvP_1GC_Robo();
            if (currentTransition == "3Gate")
                PvP_1GC_3Gate();
            if (currentTransition == "4Gate")
                PvP_1GC_4Gate();
            if (currentTransition == "DT")
                PvP_1GC_DT();
        }
    }
}