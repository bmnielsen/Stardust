#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

#include "Builds/Protoss/ProtossBuildOrder.h"

namespace McRave::BuildOrder::Protoss
{
    namespace {
        bool againstRandom = false;

        void queueWallDefenses()
        {
            // Adding Wall Defenses
            if (Walls::getNaturalWall()) {
                if (vis(Protoss_Forge) > 0 && (Walls::needAirDefenses(*Walls::getNaturalWall()) > 0 || Walls::needGroundDefenses(*Walls::getNaturalWall()) > 0))
                    buildQueue[Protoss_Photon_Cannon] = vis(Protoss_Photon_Cannon) + 1;
            }
        }

        void queueStationDefenses()
        {
            // Adding Station Defenses
            if (int(Stations::getStations(PlayerState::Self).size()) >= 2) {
                for (auto &station : Stations::getStations(PlayerState::Self)) {
                    if (vis(Protoss_Forge) > 0 && (Stations::needGroundDefenses(station) > 0 || Stations::needAirDefenses(station) > 0))
                        buildQueue[Protoss_Photon_Cannon] = vis(Protoss_Photon_Cannon) + 1;
                }
            }
        }

        void queueSupply()
        {
            if (!inBookSupply) {
                int count = min(22, s / 14) - (com(Protoss_Nexus) - 1);
                buildQueue[Protoss_Pylon] = count;

                for (auto &station : Stations::getStations(PlayerState::Self)) {
                    if (Stations::needPower(station)) {
                        buildQueue[Protoss_Pylon]++;
                        break;
                    }
                }
            }
        }

        void queueGeysers()
        {
            if (!inOpeningBook) {
                gasDesired = ((Broodwar->self()->minerals() > 600 && Broodwar->self()->gas() < 200) || Resources::isMineralSaturated()) && com(Protoss_Probe) >= 30;
                buildQueue[Protoss_Assimilator] = min(vis(Protoss_Assimilator) + gasDesired, Resources::getGasCount());
            }
        }

        void queueUpgradeStructures()
        {
            // If we're not in our opener
            if (!inOpeningBook) {

                // Adding upgrade buildings
                if (com(Protoss_Assimilator) >= 3) {
                    auto forgeCount = com(Protoss_Assimilator) >= 4 ? 2 - (int)Terrain::isIslandMap() : 1;
                    auto coreCount = com(Protoss_Assimilator) >= 4 ? 1 + (int)Terrain::isIslandMap() : 1;

                    buildQueue[Protoss_Cybernetics_Core] = 1 + (int)Terrain::isIslandMap();
                    buildQueue[Protoss_Forge] = 2 - (int)Terrain::isIslandMap();
                }

                // Important to have a forge vs Z
                if (com(Protoss_Nexus) >= 2 && Players::vZ())
                    buildQueue[Protoss_Forge] = 1;

                // Ensure we build a core outside our opening book
                if (com(Protoss_Gateway) >= 2)
                    buildQueue[Protoss_Cybernetics_Core] = 1;

                // Corsair/Scout upgrades
                if (s >= 300 && (isTechUnit(Protoss_Scout) || isTechUnit(Protoss_Corsair)))
                    buildQueue[Protoss_Fleet_Beacon] = 1;
            }
        }

        void queueExpansions()
        {
            // Against FFE add a Nexus
            if (Spy::getEnemyBuild() == "FFE" && Broodwar->getFrameCount() < 15000) {
                auto cannonCount = Players::getVisibleCount(PlayerState::Enemy, Protoss_Photon_Cannon);
                wantNatural = true;

                if (cannonCount < 6) {
                    buildQueue[Protoss_Nexus] = 2;
                    buildQueue[Protoss_Assimilator] = (vis(Protoss_Nexus) >= 2) + (s >= 120);
                    unitLimits[Protoss_Zealot] = 0;
                    gasLimit = vis(Protoss_Nexus) != buildCount(Protoss_Nexus) ? 0 : INT_MAX;
                }
                else {
                    buildQueue[Protoss_Nexus] = 3;
                    buildQueue[Protoss_Assimilator] = (vis(Protoss_Nexus) >= 2) + (s >= 120);
                    unitLimits[Protoss_Zealot] = 0;
                    gasLimit = vis(Protoss_Nexus) != buildCount(Protoss_Nexus) ? 0 : INT_MAX;
                }
            }

            // If we're not in our opener
            if (!inOpeningBook) {
                const auto availableMinerals = Broodwar->self()->minerals() - BuildOrder::getMinQueued();
                expandDesired = (techUnit == None && Resources::isGasSaturated() && (Resources::isMineralSaturated() || com(Protoss_Nexus) >= 3) && (techSat || com(Protoss_Nexus) >= 3) && productionSat)
                    || (availableMinerals >= 800 && (Resources::isMineralSaturated() || Resources::isGasSaturated()))
                    || (Stations::getStations(PlayerState::Self).size() >= 4 && Stations::getMiningStationsCount() <= 2)
                    || (Stations::getStations(PlayerState::Self).size() >= 4 && Stations::getGasingStationsCount() <= 1);

                buildQueue[Protoss_Nexus] = com(Protoss_Nexus) + expandDesired;
            }
        }

        void queueProduction()
        {
            // If we're not in our opener
            if (!inOpeningBook) {
                const auto availableMinerals = Broodwar->self()->minerals() - BuildOrder::getMinQueued();
                rampDesired = !productionSat && ((techUnit == None && availableMinerals >= 150 && (techSat || Stations::getGasingStationsCount() >= 3)) || availableMinerals >= 300);

                // Adding production
                auto maxGates = Players::vT() ? 16 : 12;
                auto gatesPerBase = 3.0 - (0.5 * (int(isTechUnit(Protoss_Carrier) || isTechUnit(Protoss_Scout)) || int(Stations::getStations(PlayerState::Self).size()) >= 3));
                productionSat = (vis(Protoss_Gateway) >= int(2.5 * Stations::getGasingStationsCount()) || vis(Protoss_Gateway) >= maxGates);
                if (rampDesired) {
                    auto gateCount = min({ maxGates, int(round(Stations::getGasingStationsCount() * gatesPerBase)), vis(Protoss_Gateway) + 1 });
                    auto stargateCount = min({ 4, int(isTechUnit(Protoss_Carrier) || isTechUnit(Protoss_Scout)) * Stations::getGasingStationsCount(), vis(Protoss_Stargate) + 1 });
                    buildQueue[Protoss_Gateway] = gateCount;
                    buildQueue[Protoss_Stargate] = stargateCount;
                }
            }
        }

        void calculateGasLimit()
        {
            // Gas limits
            if ((buildCount(Protoss_Assimilator) == 0 && com(Protoss_Probe) <= 12) || com(Protoss_Probe) <= 8)
                gasLimit = 0;
            else if (com(Protoss_Probe) < 20)
                gasLimit = min(gasLimit, com(Protoss_Probe) / 4);
            else if (!inOpeningBook && com(Protoss_Probe) >= 20)
                gasLimit = INT_MAX;
        }

        void removeExcessGas()
        {

        }
    }
    
    bool goonRange() {
        return Broodwar->self()->isUpgrading(UpgradeTypes::Singularity_Charge) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Singularity_Charge);
    }

    void opener()
    {
        if (Players::getRaceCount(Races::Unknown, PlayerState::Enemy) > 0 && !Players::PvFFA() && !Players::PvTVB())
            againstRandom = true;

        if (Players::PvT())
            PvT();
        else if (Players::PvP())
            PvP();
        else if (Players::PvZ() || againstRandom)
            PvZ();
        else if (Players::PvFFA() || Players::PvTVB())
            PvFFA();
    }

    void tech()
    {
        if (!atPercent(Protoss_Cybernetics_Core, 1.00))
            return;

        const auto firstTechUnit = !techList.empty() ? *techList.begin() : None;
        const auto skipOneTech = int(firstUnit == None || (firstUnit != None && Stations::getStations(PlayerState::Self).size() >= 2) || Spy::getEnemyBuild() == "FFE" || (Spy::enemyGasSteal() && !Terrain::isNarrowNatural()));
        const auto techVal = int(techList.size()) + skipOneTech - isTechUnit(Protoss_Shuttle) + isTechUnit(Protoss_Arbiter) - (com(Protoss_Nexus) >= 3 && isTechUnit(Protoss_Dark_Templar));

        // PvP
        if (Players::PvP()) {
            if (firstTechUnit == Protoss_Dark_Templar)
                techOrder ={ Protoss_High_Templar, Protoss_Observer };
            else
                techOrder ={ Protoss_Reaver, Protoss_High_Templar };
        }

        // PvZ
        if (Players::PvZ()) {
            if (firstTechUnit == Protoss_Reaver)
                techOrder ={ Protoss_Corsair, Protoss_High_Templar };
            else if (firstTechUnit == Protoss_Corsair)
                techOrder ={ Protoss_High_Templar, Protoss_Reaver };
            else if (firstTechUnit == Protoss_High_Templar)
                techOrder ={ Protoss_Corsair, Protoss_Reaver };
            else
                techOrder ={ Protoss_Corsair, Protoss_Observer, Protoss_High_Templar };
        }

        // PvT
        if (Players::PvT()) {
            if (firstTechUnit == Protoss_Dark_Templar)
                techOrder ={ Protoss_Arbiter, Protoss_Observer, Protoss_High_Templar };
            else if (firstTechUnit == Protoss_Carrier)
                techOrder ={ Protoss_Observer, Protoss_High_Templar };
            else
                techOrder ={ Protoss_Observer, Protoss_Arbiter, Protoss_High_Templar };
        }

        // PvFFA
        if (Players::PvFFA()) {
            techOrder ={ Protoss_Observer, Protoss_Reaver, Protoss_Carrier };
        }
        techSat = techVal >= Stations::getGasingStationsCount();

        // If we have our tech unit, set to none
        if (techComplete())
            techUnit = None;

        // Change desired detection if we get Cannons
        // TODO: Clean up all below this section
        if (Spy::enemyInvis() && desiredDetection == Protoss_Forge) {
            buildQueue[Protoss_Forge] = 1;
            buildQueue[Protoss_Photon_Cannon] = com(Protoss_Forge) * 2;

            if (com(Protoss_Photon_Cannon) >= 1) {
                desiredDetection = Protoss_Observer;
                hideTech = true;
            }
        }

        // If production is saturated and none are idle or we need detection, choose a tech
        if ((!inOpeningBook && !getTech && !techSat && techUnit == None) || (Spy::enemyInvis() && !isTechUnit(desiredDetection)))
            getTech = true;

        // If we need detection
        if (getTech && Spy::enemyInvis() && !isTechUnit(desiredDetection))
            techUnit = desiredDetection;

        // Various hardcoded tech choices
        else if (getTech && currentTransition == "DoubleExpand" && !isTechUnit(Protoss_High_Templar))
            techUnit = Protoss_High_Templar;
        else if (getTech && Spy::getEnemyTransition() == "4Gate" && !isTechUnit(Protoss_Dark_Templar) && !Spy::enemyGasSteal())
            techUnit = Protoss_Dark_Templar;

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

        // If we want to wall at the natural but we don't have a wall there, check main
        if (wallNat && !Walls::getNaturalWall() && Walls::getMainWall()) {
            wallNat = false;
            wallMain = true;
        }
    }

    void composition()
    {
        if (inOpeningBook && techList.empty())
            return;

        armyComposition.clear();
        armyComposition[Protoss_Probe] = 1.00;

        // Ordered sections in reverse tech order such that it only checks the most relevant section first
        if (Players::vP()) {
            if (Stations::getStations(PlayerState::Self).size() >= 4) {
                armyComposition[Protoss_Zealot] = 0.50;
                armyComposition[Protoss_Dragoon] = 0.25;
                armyComposition[Protoss_Archon] = 0.25;
            }
            else if (isTechUnit(Protoss_High_Templar)) {
                armyComposition[Protoss_Zealot] = 0.50;
                armyComposition[Protoss_Dragoon] = 0.50;
            }
            else {
                armyComposition[Protoss_Zealot] = 0.05;
                armyComposition[Protoss_Dragoon] = 0.95;
            }
        }

        if (Players::vT()) {
            if (isTechUnit(Protoss_Carrier)) {
                armyComposition[Protoss_Zealot] = 0.25;
                armyComposition[Protoss_Dragoon] = 0.75;
            }
            else if (isTechUnit(Protoss_High_Templar) || isTechUnit(Protoss_Arbiter)) {
                armyComposition[Protoss_Zealot] = 0.40;
                armyComposition[Protoss_Dragoon] = 0.60;
            }
            else
                armyComposition[Protoss_Dragoon] = 1.00;            
        }

        if (Players::vZ()) {
            if (currentTransition == "4Gate" || currentTransition == "5GateGoon")
                armyComposition[Protoss_Dragoon] = 1.00;
            else if (Stations::getStations(PlayerState::Self).size() >= 3) {
                armyComposition[Protoss_Zealot] = 0.40;
                armyComposition[Protoss_Dragoon] = 0.20;
                armyComposition[Protoss_Archon] = 0.40;
            }
            else
                armyComposition[Protoss_Zealot] = 1.00;
        }

        if (Players::PvFFA()) {
            if (isTechUnit(Protoss_Observer)) {
                armyComposition[Protoss_Zealot] = 0.40;
                armyComposition[Protoss_Dragoon] = 0.60;
            }
            else
                armyComposition[Protoss_Dragoon] = 1.00;
        }

        for (auto &type : techList)
            armyComposition[type] = 0.05;
    }

    void unlocks()
    {
        // Unlocking units
        unlockedType.clear();
        for (auto &[type, per] : armyComposition) {
            if (per > 0.0)
                unlockedType.insert(type);
        }

        // Leg upgrade check
        auto zealotLegs = Broodwar->self()->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) > 0
            || (com(Protoss_Citadel_of_Adun) > 0 && s >= 200);

        // Check if we should always make Zealots
        if (unitLimits[Protoss_Zealot] > vis(Protoss_Zealot)
            || armyComposition[Protoss_Zealot] > 0.10
            || zealotLegs)
            unlockedType.insert(Protoss_Zealot);
        else
            unlockedType.erase(Protoss_Zealot);

        // Check if we should always make Dragoons
        if ((Players::vZ() && Broodwar->getFrameCount() > 20000)
            || Players::getVisibleCount(PlayerState::Enemy, Zerg_Lurker) > 0
            || unitLimits[Protoss_Dragoon] > vis(Protoss_Dragoon))
            unlockedType.insert(Protoss_Dragoon);
        else
            unlockedType.erase(Protoss_Dragoon);

        //// Add Observers if we have a Reaver
        //if (vis(Protoss_Reaver) >= 2) {
        //    techList.insert(Protoss_Observer);
        //    unlockedType.insert(Protoss_Observer);
        //}

        //// Add Reavers if we have a Observer in PvP
        //if (Players::vP() && vis(Protoss_Observer) >= 1) {
        //    techList.insert(Protoss_Reaver);
        //    unlockedType.insert(Protoss_Reaver);
        //}

        // Add Shuttles if we have Reavers/HT
        if (com(Protoss_Robotics_Facility) > 0 && (isTechUnit(Protoss_Reaver) || isTechUnit(Protoss_High_Templar) || (Players::vP() && !Spy::enemyInvis() && isTechUnit(Protoss_Observer)))) {
            techList.insert(Protoss_Shuttle);
            unlockedType.insert(Protoss_Shuttle);
        }

        // Add DT late game
        if (Stations::getStations(PlayerState::Self).size() >= 4) {
            techList.insert(Protoss_Dark_Templar);
            unlockedType.insert(Protoss_Dark_Templar);
        }

        // Add HT or Arbiter if enemy has detection
        if (com(Protoss_Dark_Templar) > 0) {
            auto substitute = Players::vT() ? Protoss_Arbiter : Protoss_High_Templar;
            if (!Players::vP() && Players::hasDetection(PlayerState::Enemy)) {
                unlockedType.insert(substitute);
                techList.insert(substitute);
            }
        }

        // Remove DT if enemy has Observers in PvP
        if (Players::PvP() && total(Protoss_Dark_Templar) >= 4 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Observer) > 0) {
            unlockedType.erase(Protoss_Dark_Templar);
            techList.erase(Protoss_Dark_Templar);
        }
    }
}