#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    void defaultPvFFA() {
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
        transitionReady =                               false;

        gasLimit =                                      INT_MAX;
        unitLimits[Protoss_Zealot] =                    1;
        unitLimits[Protoss_Dragoon] =                   INT_MAX;
        unitLimits[Protoss_Probe] =                     INT_MAX;

        desiredDetection =                              Protoss_Observer;
        firstUpgrade =                                  vis(Protoss_Dragoon) > 0 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
        firstTech =                                     TechTypes::None;
        firstUnit =                                     None;

        armyComposition[Protoss_Probe] =                1.00;
        armyComposition[Protoss_Zealot] =               0.10;
        armyComposition[Protoss_Dragoon] =              0.90;
    }

    void PvFFA()
    {
        defaultPvFFA();

        // Builds
        if (currentBuild == "1GateCore")
            PvFFA_1GC();
    }
}