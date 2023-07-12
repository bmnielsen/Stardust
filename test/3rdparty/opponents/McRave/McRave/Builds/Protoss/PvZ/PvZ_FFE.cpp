#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    namespace {

        void PvZ_FFE_Forge()
        {
            wantNatural =                                   true;
            wallNat =                                       true;
            scout =                                         vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Gateway) >= 1;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 28);
            buildQueue[Protoss_Pylon] =                     (s >= 14) + (s >= 30), (s >= 16) + (s >= 30);
            buildQueue[Protoss_Assimilator] =               !Spy::enemyRush() && s >= 34;
            buildQueue[Protoss_Gateway] =                   s >= 32;
            buildQueue[Protoss_Forge] =                     s >= 20;
        }

        void PvZ_FFE_Gate()
        {
            wantNatural =                                   true;
            wallNat =                                       true;
            scout =                                         vis(Protoss_Gateway) > 0;
            transitionReady =                               vis(Protoss_Nexus) >= 2;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 42);
            buildQueue[Protoss_Pylon] =                     (s >= 14) + (s >= 30), (s >= 16) + (s >= 30);
            buildQueue[Protoss_Gateway] =                   vis(Protoss_Pylon) > 0;
            buildQueue[Protoss_Forge] =                     s >= 60;
        }

        void PvZ_FFE_Nexus()
        {
            wantNatural =                                   true;
            wallNat =                                       true;
            scout =                                         vis(Protoss_Pylon) > 0;
            transitionReady =                               vis(Protoss_Assimilator) >= 1;

            buildQueue[Protoss_Nexus] =                     1 + (s >= 24);
            buildQueue[Protoss_Pylon] =                     (s >= 14) + (s >= 30), (s >= 16) + (s >= 30);
            buildQueue[Protoss_Assimilator] =               !Spy::enemyRush() && vis(Protoss_Gateway) >= 1;
            buildQueue[Protoss_Gateway] =                   vis(Protoss_Forge) > 0;
            buildQueue[Protoss_Forge] =                     vis(Protoss_Nexus) >= 2;
        }

        void PvZ_StormRush()
        {
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

        void PvZ_2Stargate()
        {
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

        void PvZ_5GateGoon()
        {
            // "https://liquipedia.net/starcraft/5_Gate_Ranged_Goons_(vs._Zerg)"
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

        void PvZ_NeoBisu()
        {
            // "https://liquipedia.net/starcraft/%2B1_Sair/Speedlot_(vs._Zerg)"
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

    void PvZ_FFE()
    {
        // Reactions
        if (!lockedTransition) {
            if (Spy::getEnemyOpener() == "4Pool" && currentOpener != "Forge")
                currentOpener = "Panic";
            if (Spy::getEnemyTransition() == "2HatchHydra" || Spy::getEnemyTransition() == "3HatchHydra")
                currentTransition = "StormRush";
            else if (Spy::getEnemyTransition() == "2HatchMuta" || Spy::getEnemyTransition() == "3HatchMuta")
                currentTransition = "2Stargate";
        }

        // Openers
        if (currentOpener == "Forge")
            PvZ_FFE_Forge();
        if (currentOpener == "Gate")
            PvZ_FFE_Gate();
        if (currentOpener == "Nexus")
            PvZ_FFE_Nexus();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "StormRush")
                PvZ_StormRush();
            if (currentTransition == "2Stargate")
                PvZ_2Stargate();
            if (currentTransition == "5GateGoon")
                PvZ_5GateGoon();
            if (currentTransition == "NeoBisu")
                PvZ_NeoBisu();
        }
    }
}