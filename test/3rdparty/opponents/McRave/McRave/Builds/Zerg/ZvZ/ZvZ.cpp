#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ZergBuildOrder.h"

namespace McRave::BuildOrder::Zerg {

    void defaultZvZ() {
        inOpeningBook =                                 true;
        inBookSupply =                                  true;
        wallNat =                                       hatchCount() >= 3;
        wallMain =                                      false;
        wantNatural =                                   false;
        wantThird =                                     false;
        proxy =                                         false;
        hideTech =                                      false;
        playPassive =                                   false;
        rush =                                          false;
        pressure =                                      false;
        transitionReady =                               false;
        gasTrick =                                      false;

        gasLimit =                                      gasMax();
        unitLimits[Zerg_Zergling] =                     lingsNeeded_ZvZ();
        unitLimits[Zerg_Drone] =                        INT_MAX;

        desiredDetection =                              Zerg_Overlord;
        firstUpgrade =                                  vis(Zerg_Zergling) >= 6 ? UpgradeTypes::Metabolic_Boost : UpgradeTypes::None;
        firstTech =                                     TechTypes::None;
        firstUnit =                                     None;

        armyComposition[Zerg_Drone] =                   0.60;
        armyComposition[Zerg_Zergling] =                0.40;
    }

    int lingsNeeded_ZvZ() {
        if (Players::getTotalCount(PlayerState::Enemy, Zerg_Sunken_Colony) >= 1 && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) < 6)
            return 24;
        else if (Spy::getEnemyTransition() == "2HatchSpeedling")
            return 6;
        else if (vis(Zerg_Spire) > 0)
            return 12;
        else if (vis(Zerg_Lair) > 0)
            return 18;
        return 16;
    }

    void ZvZ()
    {
        defaultZvZ();

        // Builds
        if (currentBuild == "PoolHatch")
            ZvZ_PH();
        if (currentBuild == "PoolLair")
            ZvZ_PL();
    }
}