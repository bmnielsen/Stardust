#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    namespace {

        void PvZ_1GC_DT()
        {
            // Experimental build from Best
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

        void PvZ_1GC_ZCore()
        {
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            gasLimit =                                      INT_MAX;
            unitLimits[Protoss_Zealot] =                    vis(Protoss_Cybernetics_Core) > 0 ? INT_MAX : 1;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 32);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 34;
        }

        void PvZ_1GC_ZZCore()
        {
            scout =                                         Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
            gasLimit =                                      INT_MAX;
            unitLimits[Protoss_Zealot] =                    vis(Protoss_Cybernetics_Core) > 0 ? INT_MAX : 2;

            buildQueue[Protoss_Nexus] =                     1;
            buildQueue[Protoss_Pylon] =                     (s >= 16) + (s >= 24);
            buildQueue[Protoss_Gateway] =                   s >= 20;
            buildQueue[Protoss_Assimilator] =               s >= 32;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 40;
        }
    }

    void PvZ_1GC()
    {
        // Reactions
        if (!lockedTransition) {

            // If enemy is rushing, pressuring or stole gas
            if (Spy::enemyRush() || Spy::enemyPressure() || Spy::enemyGasSteal()) {
                currentBuild = "2Gate";
                currentOpener = "Main";
                currentTransition = "4Gate";
            }
        }

        // Openers
        if (currentOpener == "1Zealot")
            PvZ_1GC_ZCore();
        if (currentOpener == "2Zealot")
            PvZ_1GC_ZZCore();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "DT")
                PvZ_1GC_DT();
        }
    }
}