#include "McRave.h"
#include "BuildOrder.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "../TerranBuildOrder.h"

namespace McRave::BuildOrder::Terran
{
    void TvA()
    {
        if (currentTransition == "2Fact") {
            firstUpgrade =  UpgradeTypes::Ion_Thrusters;
            firstTech = TechTypes::None;
            inOpeningBook = s < 70;
            scout = s >= 20 && vis(Terran_Supply_Depot) > 0;
            wallMain = true;
            gasLimit = INT_MAX;

            buildQueue[Terran_Supply_Depot] =        s >= 18;
            buildQueue[Terran_Barracks] =            s >= 20;
            buildQueue[Terran_Refinery] =            s >= 24;
            buildQueue[Terran_Factory] =            (s >= 30) + (s >= 36) + (s >= 46);
            buildQueue[Terran_Machine_Shop] =       (s >= 30) + (com(Terran_Factory) >= 2);

            armyComposition[Terran_SCV] = 1.00;
            armyComposition[Terran_Marine] = 0.05;
            armyComposition[Terran_Vulture] = 0.75;
            armyComposition[Terran_Siege_Tank_Tank_Mode] = 0.20;
        }
    }
}