#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    namespace {

        void PvP_2G_DT()
        {
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

        void PvP_2G_Robo()
        {
            // "https://liquipedia.net/starcraft/2_Gate_Reaver_(vs._Protoss)"
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

        void PvP_2G_Main()
        {
            // "https://liquipedia.net/starcraft/2_Gate_(vs._Protoss)" - 10/12
            unitLimits[Protoss_Zealot] =                    s <= 80 ? 7 : 0;
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) >= 1 : vis(Protoss_Gateway) >= 2;
            transitionReady =                               vis(Protoss_Gateway) >= 2;
            desiredDetection =                              Protoss_Forge;
            gasLimit =                                      total(Protoss_Zealot) >= 3 ? INT_MAX : 0;

            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 24);
            buildQueue[Protoss_Shield_Battery] =            Spy::enemyRush() && vis(Protoss_Zealot) >= 2 && vis(Protoss_Pylon) >= 2;
        }
    }

    void PvP_2G()
    {
        // Reactions
        if (!lockedTransition) {
            if (Spy::getEnemyBuild() == "FFE" || Spy::getEnemyTransition() == "DT")
                currentTransition = "Robo";
            else if (Spy::getEnemyBuild() == "CannonRush")
                currentTransition = "Robo";
            else if (Spy::enemyPressure())
                currentTransition = "DT";
        }

        // Openers
        if (currentOpener == "Main")
            PvP_2G_Main();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "Robo")
                PvP_2G_Robo();
            if (currentTransition == "DT")
                PvP_2G_DT();
        }
    }
}