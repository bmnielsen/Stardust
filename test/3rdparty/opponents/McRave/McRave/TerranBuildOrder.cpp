#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

namespace McRave::BuildOrder::Terran {

    void opener()
    {
        if (currentBuild == "RaxFact")
            RaxFact();

        if (Broodwar->getGameType() == GameTypes::Team_Free_For_All || Broodwar->getGameType() == GameTypes::Team_Melee) {
            buildQueue[Terran_Command_Center] = Players::getSupply(PlayerState::Self, Races::None) >= 30;
            currentBuild = "RaxFact";
            currentOpener = "10Rax";
            currentTransition = "2Fact";
            RaxFact();
        }
    }

    void tech()
    {
        getNewTech();
        getTechBuildings();
    }

    void situational()
    {
        auto satVal = 2;
        auto prodVal = vis(Terran_Barracks) + vis(Terran_Factory) + vis(Terran_Starport);
        auto baseVal = vis(Terran_Command_Center);
        auto techVal = int(techList.size());

        productionSat = (prodVal >= satVal * baseVal);
        techSat = (techVal > baseVal);

        if (techComplete())
            techUnit = None; // If we have our tech unit, set to none    
        if (Strategy::enemyInvis() || (!inOpeningBook && !getTech && !techSat /*&& productionSat*/ && techUnit == None))
            getTech = true; // If production is saturated and none are idle or we need detection, choose a tech

        productionSat = (com(Terran_Factory) >= min(12, (2 * vis(Terran_Command_Center))));
        unlockedType.insert(Terran_Goliath);
        unlockedType.insert(Terran_Vulture);

        if (s >= 80)
            unlockedType.insert(Terran_Siege_Tank_Tank_Mode);

        /*if (!inOpeningBook || com(Terran_Marine) >= 4)
            unlockedType.erase(Terran_Marine);
        else*/
        unlockedType.insert(Terran_Marine);
        unlockedType.insert(Terran_Vulture);
        unlockedType.insert(Terran_Siege_Tank_Tank_Mode);
        unlockedType.insert(Terran_Siege_Tank_Siege_Mode);

        armyComposition[Terran_Marine] = 0.05;
        armyComposition[Terran_Vulture] = 0.75;
        armyComposition[Terran_Siege_Tank_Tank_Mode] = 0.20;


        // Control Tower
        if (com(Terran_Starport) >= 1)
            buildQueue[Terran_Control_Tower] = com(Terran_Starport);

        // Machine Shop
        if (com(Terran_Factory) >= 3)
            buildQueue[Terran_Machine_Shop] = max(1, com(Terran_Factory) - com(Terran_Command_Center) - 1);

        // Supply Depot logic
        if (vis(Terran_Supply_Depot) > 0) {
            int count = min(22, s / 14) - (com(Terran_Command_Center) - 1);
            buildQueue[Terran_Supply_Depot] = count;
        }

        // Bunker logic
        if (Strategy::enemyRush() && !wallMain)
            buildQueue[Terran_Bunker] = 1;

        if (!inOpeningBook)
        {
            // Expansion logic
            if (shouldExpand())
                buildQueue[Terran_Command_Center] = com(Terran_Command_Center) + 1;

            // Refinery logic
            if (shouldAddGas())
                buildQueue[Terran_Refinery] = Resources::getGasCount();

            // Armory logic TODO
            buildQueue[Terran_Armory] = (s > 160) + (s > 200);

            // Academy logic
            if (Strategy::enemyInvis()) {
                buildQueue[Terran_Academy] = 1;
                buildQueue[Terran_Comsat_Station] = 2;
            }

            if (com(Terran_Science_Facility) > 0)
                buildQueue[Terran_Physics_Lab] = 1;

            // Engineering Bay logic
            if (s > 200)
                buildQueue[Terran_Engineering_Bay] = 1;

            // Missle Turret logic
            if (Players::vZ() && com(Terran_Engineering_Bay) > 0)
                buildQueue[Terran_Missile_Turret] = com(Terran_Command_Center) * 2;
        }
    }
}