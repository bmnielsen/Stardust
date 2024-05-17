#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    namespace {

        void PvZ_2G_4Gate()
        {
            firstUnit =                                 None;
            inOpeningBook =                             s < 120;
            lockedTransition =                          true;
            firstUpgrade =                              UpgradeTypes::Singularity_Charge;
            unitLimits[Protoss_Zealot] =                5;
            unitLimits[Protoss_Dragoon] =               INT_MAX;
            wallNat =                                   vis(Protoss_Nexus) >= 2 || currentOpener == "Natural";
            playPassive =                               !firstReady() && (!Terrain::foundEnemy() || Spy::enemyPressure());

            buildQueue[Protoss_Gateway] =               2 + (s >= 62) + (s >= 70);
            buildQueue[Protoss_Assimilator] =           s >= 52;
            buildQueue[Protoss_Cybernetics_Core] =      vis(Protoss_Zealot) >= 5;

            // Army Composition
            armyComposition[Protoss_Zealot] =           0.25;
            armyComposition[Protoss_Dragoon] =          0.75;
        }

        void PvZ_2G_Expand()
        {
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

        void PvZ_2G_Main()
        {
            // "https://liquipedia.net/starcraft/2_Gate_(vs._Protoss)" - 10/12
            wallNat =                                       vis(Protoss_Nexus) >= 2;
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) >= 1 : vis(Protoss_Gateway) >= 2;
            transitionReady =                               vis(Protoss_Gateway) >= 2;

            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   (s >= 20) + (s >= 24);
            buildQueue[Protoss_Shield_Battery] =            Spy::enemyRush() && vis(Protoss_Zealot) >= 2 && vis(Protoss_Pylon) >= 2;
        }
    }

    void PvZ_2G()
    {
        // Reactions
        if (!lockedTransition) {
            if (Players::getVisibleCount(PlayerState::Enemy, UnitTypes::Zerg_Sunken_Colony) >= 4)
                currentTransition = "Expand";
        }

        // Openers
        if (currentOpener == "Main")
            PvZ_2G_Main();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "4Gate")
                PvZ_2G_4Gate();
            if (currentTransition == "Expand")
                PvZ_2G_Expand();
        }
    }
}