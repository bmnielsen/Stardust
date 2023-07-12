#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "TerranBuildOrder.h"

namespace McRave::BuildOrder::Terran {

    namespace {
        bool againstRandom = false;

        void queueWallDefenses()
        {

        }

        void queueStationDefenses()
        {
            // Adding Station Defenses
            for (auto &station : Stations::getStations(PlayerState::Self)) {
                if (vis(Terran_Engineering_Bay) > 0 && Stations::needAirDefenses(station) > 0)
                    buildQueue[Terran_Missile_Turret] = vis(Terran_Missile_Turret) + 1;
            }
        }

        void queueSupply()
        {
            if (!inBookSupply) {
                if (vis(Terran_Supply_Depot) > 0) {
                    int count = min(22, s / 14) - (com(Terran_Command_Center) - 1);
                    buildQueue[Terran_Supply_Depot] = count;
                }
            }
        }

        void queueGeysers()
        {
            if (!inOpeningBook) {
                gasDesired = ((Broodwar->self()->minerals() > 600 && Broodwar->self()->gas() < 200) || Resources::isMineralSaturated()) && com(Terran_SCV) >= 30;
                buildQueue[Terran_Refinery] = min(vis(Terran_Refinery) + gasDesired, Resources::getGasCount());
            }
        }

        void queueUpgradeStructures()
        {
            // Control Tower
            if (com(Terran_Starport) >= 1)
                buildQueue[Terran_Control_Tower] = com(Terran_Starport);

            // Machine Shop
            if (com(Terran_Factory) >= 3)
                buildQueue[Terran_Machine_Shop] = Stations::getGasingStationsCount();

            // If we're not in our opener
            if (!inOpeningBook) {

                // Armory
                buildQueue[Terran_Armory] = (s > 160) + (s > 200);

                // Academy
                if (Spy::enemyInvis()) {
                    buildQueue[Terran_Academy] = 1;
                    buildQueue[Terran_Comsat_Station] = 2;
                }

                if (com(Terran_Science_Facility) > 0)
                    buildQueue[Terran_Physics_Lab] = 1;

                // Engineering Bay
                if (s > 200)
                    buildQueue[Terran_Engineering_Bay] = 1;
            }
        }

        void queueExpansions()
        {
            // If we're not in our opener
            if (!inOpeningBook) {
                const auto availableMinerals = Broodwar->self()->minerals() - BuildOrder::getMinQueued();
                expandDesired = (techUnit == None && Resources::isGasSaturated() && (Resources::isMineralSaturated() || com(Terran_Command_Center) >= 3) && (techSat || com(Terran_Command_Center) >= 3) && productionSat)
                    || (com(Terran_Command_Center) >= 2 && availableMinerals >= 800 && (Resources::isMineralSaturated() || Resources::isGasSaturated()))
                    || (Stations::getStations(PlayerState::Self).size() >= 4 && Stations::getMiningStationsCount() <= 2)
                    || (Stations::getStations(PlayerState::Self).size() >= 4 && Stations::getGasingStationsCount() <= 1);

                buildQueue[Terran_Command_Center] = com(Terran_Command_Center) + expandDesired;
            }
        }

        void queueProduction()
        {
            // If we're not in our opener
            if (!inOpeningBook) {
                const auto availableMinerals = Broodwar->self()->minerals() - BuildOrder::getMinQueued();
                rampDesired = !productionSat && ((techUnit == None && availableMinerals >= 150 && (techSat || com(Terran_Command_Center) >= 3)) || availableMinerals >= 300);

                // Adding production
                auto maxFacts = 8;
                auto factsPerBase = 3;
                productionSat = (vis(Terran_Factory) >= int(factsPerBase * vis(Terran_Command_Center)) || vis(Terran_Command_Center) >= maxFacts);
                if (rampDesired) {
                    auto factCount = min({ maxFacts, int(round(com(Terran_Command_Center) * factsPerBase)), vis(Terran_Factory) + 1 });
                    buildQueue[Terran_Factory] = factCount;
                }
            }
        }

        void calculateGasLimit()
        {
            // Gas limits
            if ((buildCount(Terran_Refinery) == 0 && com(Terran_SCV) <= 12) || com(Terran_SCV) <= 8)
                gasLimit = 0;
            else if (com(Terran_SCV) < 20)
                gasLimit = min(gasLimit, com(Terran_SCV) / 4);
            else if (!inOpeningBook && com(Terran_SCV) >= 20)
                gasLimit = INT_MAX;
        }

        void removeExcessGas()
        {

        }
    }

    void opener()
    {
        TvA();
    }

    void tech()
    {
        auto techVal = int(techList.size());
        techSat = (techVal > vis(Terran_Command_Center));

        if (techComplete())
            techUnit = None;   
        if (Spy::enemyInvis() || (!inOpeningBook && !getTech && !techSat && techUnit == None))
            getTech = true;

        getNewTech();
        getTechBuildings();
    }

    void situational()
    {
        // Queue up defenses
        queueWallDefenses();
        queueStationDefenses();

        // Queue up supply, upgrade structures
        queueSupply();
        queueUpgradeStructures();
        queueGeysers();

        // Outside of opening book, book no longer is in control of queuing production, expansions or gas limits
        queueExpansions();
        queueProduction();
        calculateGasLimit();

        // Optimize our gas mining by dropping gas mining at specific excessive values
        removeExcessGas();

        // Main bunker defense
        if (Spy::enemyRush() && !wallMain)
            buildQueue[Terran_Bunker] = 1;
    }

    void composition()
    {
        armyComposition[Terran_SCV] = 1.00;
        armyComposition[Terran_Marine] = 0.05;
        armyComposition[Terran_Vulture] = 0.75;
        armyComposition[Terran_Siege_Tank_Tank_Mode] = 0.20;
    }

    void unlocks()
    {
        // Unlocking units
        unlockedType.clear();
        for (auto &[type, per] : armyComposition) {
            if (per > 0.0)
                unlockedType.insert(type);
        }

        // Unit limiting in opening book
        if (inOpeningBook) {
            for (auto &type : unitLimits) {
                if (type.second > vis(type.first))
                    unlockedType.insert(type.first);
                else
                    unlockedType.erase(type.first);
            }
        }

        // UMS Unlocking
        if (Broodwar->getGameType() == GameTypes::Use_Map_Settings) {
            for (auto &type : BWAPI::UnitTypes::allUnitTypes()) {
                if (!type.isBuilding() && type.getRace() == Races::Terran && vis(type) >= 2) {
                    unlockedType.insert(type);
                    if (!type.isWorker())
                        techList.insert(type);
                }
            }
        }
    }
}