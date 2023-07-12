#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss {

    namespace {

        void PvT_2B_12Nexus()
        {
            // "http://liquipedia.net/starcraft/12_Nexus"
            playPassive =                                       Spy::enemyPressure() ? vis(Protoss_Dragoon) < 12 : !firstReady() && vis(Protoss_Dragoon) < 4;
            firstUpgrade =                                      vis(Protoss_Dragoon) >= 1 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
            gasLimit =                                          goonRange() && com(Protoss_Nexus) < 2 ? 2 : INT_MAX;
            unitLimits[Protoss_Zealot] =                        1;
            scout =                                             vis(Protoss_Pylon) > 0;
            transitionReady =                                   vis(Protoss_Gateway) >= 2;

            buildQueue[Protoss_Nexus] =                         1 + (s >= 24);
            buildQueue[Protoss_Pylon] =                         (s >= 16) + (s >= 44);
            buildQueue[Protoss_Assimilator] =                   s >= 30;
            buildQueue[Protoss_Gateway] =                       (s >= 28) + (s >= 34);
            buildQueue[Protoss_Cybernetics_Core] =              vis(Protoss_Gateway) >= 2;
        }

        void PvT_2B_21Nexus()
        {
            // "http://liquipedia.net/starcraft/21_Nexus"
            playPassive =                                       (Spy::enemyPressure() ? vis(Protoss_Dragoon) < 16 : !firstReady()) || (com(Protoss_Dragoon) < 4 && !Spy::enemyFastExpand());
            scout =                                             Broodwar->getStartLocations().size() == 4 ? vis(Protoss_Pylon) > 0 : vis(Protoss_Pylon) > 0;
            gasLimit =                                          goonRange() && vis(Protoss_Nexus) < 2 ? 2 : INT_MAX;
            unitLimits[Protoss_Dragoon] =                       Util::getTime() > Time(4, 0) || vis(Protoss_Nexus) >= 2 ? INT_MAX : 1;
            unitLimits[Protoss_Probe] =                         20;
            transitionReady =                                   vis(Protoss_Gateway) >= 3;

            buildQueue[Protoss_Nexus] =                         1 + (s >= 42);
            buildQueue[Protoss_Pylon] =                         (s >= 16) + (s >= 30);
            buildQueue[Protoss_Assimilator] =                   s >= 30;
            buildQueue[Protoss_Gateway] =                       (s >= 20) + (vis(Protoss_Nexus) >= 2) + (s >= 76);
            buildQueue[Protoss_Cybernetics_Core] =              s >= 26;
        }

        void PvT_2B_20Nexus()
        {
            // "https://liquipedia.net/starcraft/2_Gate_Range_Expand"
            scout =                                             vis(Protoss_Cybernetics_Core) > 0;
            inBookSupply =                                      vis(Protoss_Pylon) < 3;
            gasLimit =                                          goonRange() && vis(Protoss_Pylon) < 3 ? 2 : INT_MAX;
            unitLimits[Protoss_Dragoon] =                       Util::getTime() > Time(4, 0) || vis(Protoss_Nexus) >= 2 || s >= 40 ? INT_MAX : 0;
            unitLimits[Protoss_Probe] =                         20;
            transitionReady =                                   vis(Protoss_Gateway) >= 3;

            buildQueue[Protoss_Nexus] =                         1 + (s >= 40);
            buildQueue[Protoss_Pylon] =                         (s >= 16) + (s >= 30) + (s >= 48);
            buildQueue[Protoss_Assimilator] =                   s >= 30;
            buildQueue[Protoss_Gateway] =                       (s >= 20) + (s >= 34) + (s >= 76);
            buildQueue[Protoss_Cybernetics_Core] =              s >= 26;
        }

        void PvT_2B_Obs()
        {
            inOpeningBook =                                 s < 80;
            firstUnit =                                     Protoss_Observer;
            lockedTransition =                              total(Protoss_Nexus) >= 2;

            buildQueue[Protoss_Assimilator] =               s >= 24;
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
            buildQueue[Protoss_Robotics_Facility] =         s >= 62;
        }

        void PvT_2B_Carrier()
        {
            inOpeningBook =                                 s < 160;
            firstUnit =                                     Protoss_Carrier;
            lockedTransition =                              total(Protoss_Stargate) > 0;

            buildQueue[Protoss_Assimilator] =               (s >= 24) + (s >= 60);
            buildQueue[Protoss_Cybernetics_Core] =          s >= 26;
            buildQueue[Protoss_Stargate] =                  vis(Protoss_Dragoon) >= 6;
            buildQueue[Protoss_Fleet_Beacon] =              atPercent(Protoss_Stargate, 1.00);
        }

        void PvT_2B_ReaverCarrier()
        {
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

    void PvT_2B()
    {
        // Reactions
        if (!lockedTransition) {
            if (currentBuild == "2Base") {
                if (Spy::enemyFastExpand() || Spy::getEnemyTransition() == "SiegeExpand")
                    currentTransition = "ReaverCarrier";
                else if (Terrain::getEnemyStartingPosition().isValid() && !Spy::enemyFastExpand() && currentTransition == "DoubleExpand")
                    currentTransition = "Obs";
                else if (Spy::enemyPressure())
                    currentTransition = "Obs";
            }
        }

        // Openers
        if (currentOpener == "12Nexus")
            PvT_2B_12Nexus();
        if (currentOpener == "20Nexus")
            PvT_2B_20Nexus();
        if (currentOpener == "21Nexus")
            PvT_2B_21Nexus();

        // Transitions
        if (transitionReady) {
            if (currentTransition == "Obs")
                PvT_2B_Obs();
            if (currentTransition == "Carrer")
                PvT_2B_Carrier();
            if (currentTransition == "ReaverCarrier")
                PvT_2B_ReaverCarrier();
        }
    }
}