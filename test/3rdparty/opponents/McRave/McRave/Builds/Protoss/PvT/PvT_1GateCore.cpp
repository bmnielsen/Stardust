#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    namespace {

        void PvT_1GC_Robo()
        {
            // "http://liquipedia.net/starcraft/1_Gate_Reaver" 
            lockedTransition =                              total(Protoss_Robotics_Facility) > 0;
            inOpeningBook =                                 s < 60;
            hideTech =                                      com(Protoss_Reaver) <= 0;
            firstUnit =                                     Spy::enemyPressure() ? Protoss_Reaver : Protoss_Observer;

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

        void PvT_1GC_DT()
        {
            // "https://liquipedia.net/starcraft/DT_Fast_Expand_(vs._Terran)"
            lockedTransition =                              total(Protoss_Citadel_of_Adun) > 0;
            inOpeningBook =                                 s <= 80;
            hideTech =                                      com(Protoss_Dark_Templar) <= 0;
            firstUnit =                                     Protoss_Dark_Templar;
            firstUpgrade =                                  vis(Protoss_Dark_Templar) >= 2 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
            playPassive =                                   Spy::enemyPressure() ? vis(Protoss_Observer) == 0 : false;

            buildQueue[Protoss_Gateway] =                   (s >= 20) + (vis(Protoss_Templar_Archives) > 0);
            buildQueue[Protoss_Nexus] =                     1 + (vis(Protoss_Dark_Templar) > 0);
            buildQueue[Protoss_Assimilator] =               (s >= 24) + (vis(Protoss_Nexus) >= 2);
            buildQueue[Protoss_Citadel_of_Adun] =           s >= 36;
            buildQueue[Protoss_Templar_Archives] =          s >= 48;

            // Army composition
            armyComposition[Protoss_Dragoon] =              0.95;
            armyComposition[Protoss_Zealot] =               0.05;
        }

        void PvT_1GC_4Gate()
        {
            // "https://liquipedia.net/starcraft/4_Gate_Goon_(vs._Protoss)"
            lockedTransition =                              total(Protoss_Gateway) >= 4;
            inOpeningBook =                                 s < 80;
            firstUnit =                                     None;

            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 30) + (2 * (s >= 62));
            buildQueue[Protoss_Assimilator] =               s >= 22;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;

            // Army composition
            armyComposition[Protoss_Dragoon] =              0.95;
            armyComposition[Protoss_Zealot] =               0.05;
        }

        void PvT_1GC_NZCore()
        {
            // "https://liquipedia.net/starcraft/1_Gate_Core_(vs._Terran)"
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            unitLimits[Protoss_Zealot] =                    0;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;

            // Army composition
            armyComposition[Protoss_Dragoon] =              0.95;
            armyComposition[Protoss_Zealot] =               0.05;
        }

        void PvT_1GC_ZCore()
        {
            // "https://liquipedia.net/starcraft/1_Gate_Core_(vs._Terran)"
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            unitLimits[Protoss_Zealot] =                    1;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 36;

            // Army composition
            armyComposition[Protoss_Dragoon] =              0.95;
            armyComposition[Protoss_Zealot] =               0.05;
        }
    }

    void PvT_1GC()
    {
        // Reactions
        if (!lockedTransition) {
            if (currentBuild == "1GateCore") {
                if (Spy::enemyRush()) {
                    currentBuild = "2Gate";
                    currentOpener = "Main";
                    currentTransition = "DT";
                }
                else if (Spy::enemyFastExpand())
                    currentTransition = "DT";
            }
            if (currentBuild == "2Gate") {
                if (Spy::enemyFastExpand())
                    currentTransition = "DT";
            }
        }

        // Openers
        if (currentOpener == "0Zealot")
            PvT_1GC_NZCore();
        if (currentOpener == "1Zealot")
            PvT_1GC_ZCore();

        // Transitions
        if (currentTransition == "Robo")
            PvT_1GC_Robo();
        if (currentTransition == "DT")
            PvT_1GC_DT();
        if (currentTransition == "4Gate")
            PvT_1GC_4Gate();
    }
}