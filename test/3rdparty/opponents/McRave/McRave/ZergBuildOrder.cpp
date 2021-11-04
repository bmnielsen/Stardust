#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

// Notes:
// Lings: 3 drones per hatch
// Hydra: 5.5 drones per hatch - 0.5 gas per hatch
// Muta: 8 drones per hatch - 1 gas per hatch

namespace McRave::BuildOrder::Zerg {

    enum class Composition {
        None, Ling, Hydra, HydraLurker, HydraLurkerDefiler, HydraMuta, HydraMutaDefiler, Muta, MutaLing, MutaLurkerLing, MutaLingDefiler, UltraLing, UltraLingDefiler
    };

    namespace {
        bool againstRandom = false;
        bool needSpores = false;
        bool needSunks = false;
        Composition currentComposition;

        void queueGasTrick()
        {
            // Executing gas trick
            if (gasTrick) {
                if (Broodwar->self()->minerals() >= 80 && total(Zerg_Extractor) == 0)
                    buildQueue[Zerg_Extractor] = 1;
                if (vis(Zerg_Extractor) > 0)
                    buildQueue[Zerg_Extractor] = (vis(Zerg_Drone) < 9);
            }
        }

        void queueWallDefenses()
        {
            // Adding Wall defenses
            auto needLarvaSpending = vis(Zerg_Larva) > 3 && Broodwar->self()->supplyUsed() < Broodwar->self()->supplyTotal() && unitLimits[Zerg_Larva] == 0 && Util::getTime() < Time(4, 30) && com(Zerg_Sunken_Colony) >= 2;
            if (!needLarvaSpending && !rush && !pressure && (vis(Zerg_Drone) >= 9 || Players::ZvZ())) {
                for (auto &[_, wall] : BWEB::Walls::getWalls()) {

                    if (!Terrain::isInAllyTerritory(wall.getArea()))
                        continue;

                    auto colonies = 0;
                    auto stationNeeds = wall.getStation() && (Stations::needAirDefenses(wall.getStation()) > 0 || Stations::needGroundDefenses(wall.getStation()) > 0);
                    for (auto& tile : wall.getDefenses()) {
                        if (BWEB::Map::isUsed(tile) == Zerg_Creep_Colony)
                            colonies++;
                        if (BWEB::Map::isUsed(tile) == Zerg_Creep_Colony && stationNeeds && wall.getStation()->getDefenses().find(tile) != wall.getStation()->getDefenses().end())
                            colonies--;
                    }

                    if ((vis(Zerg_Evolution_Chamber) > 0 && Walls::needAirDefenses(wall) > colonies) || (vis(Zerg_Spawning_Pool) > 0 && Walls::needGroundDefenses(wall) > colonies))
                        buildQueue[Zerg_Creep_Colony] += clamp(Walls::needGroundDefenses(wall) + Walls::needAirDefenses(wall) - colonies, 0, 2);
                    if (Walls::needAirDefenses(wall) > colonies)
                        needSpores = true;
                    if (Walls::needGroundDefenses(wall) > colonies)
                        needSunks = true;
                }
            }
        }

        void queueStationDefenses()
        {
            // Adding Station defenses
            if (vis(Zerg_Drone) >= 8 || Players::ZvZ()) {
                for (auto &station : Stations::getMyStations()) {
                    auto colonies = 0;
                    auto wallNeeds = station->getChokepoint() && BWEB::Walls::getWall(station->getChokepoint()) && (Walls::needGroundDefenses(*BWEB::Walls::getWall(station->getChokepoint())) > 0 || Walls::needAirDefenses(*BWEB::Walls::getWall(station->getChokepoint())) > 0);
                    for (auto& tile : station->getDefenses()) {
                        if (BWEB::Map::isUsed(tile) == Zerg_Creep_Colony)
                            colonies++;
                        if (BWEB::Map::isUsed(tile) == Zerg_Creep_Colony && wallNeeds && BWEB::Walls::getWall(station->getChokepoint())->getDefenses().find(tile) != BWEB::Walls::getWall(station->getChokepoint())->getDefenses().end())
                            colonies--;
                    }

                    if ((vis(Zerg_Spawning_Pool) > 0 && Stations::needGroundDefenses(station) > colonies) || (vis(Zerg_Evolution_Chamber) > 0 && Stations::needAirDefenses(station) > colonies))
                        buildQueue[Zerg_Creep_Colony] += clamp(Stations::needGroundDefenses(station) + Stations::needAirDefenses(station) - colonies, 0, 2);
                    if (Stations::needAirDefenses(station) > 0)
                        needSpores = true;
                    if (Stations::needGroundDefenses(station) > 0)
                        needSunks = true;
                }
            }
        }

        void queueOverlords()
        {
            // Adding Overlords outside opening book supply
            if (!inBookSupply) {
                int providers = vis(Zerg_Overlord) > 0 ? 14 : 16;
                int count = 1 + min(26, s / providers);
                if (vis(Zerg_Overlord) >= 3)
                    buildQueue[Zerg_Overlord] = count;
            }

            // Adding Overlords if we are sacrificing a scout or know we will lose one
            if ((Scouts::isSacrificeScout() || Strategy::getEnemyTransition() == "Corsair") && Util::getTime() > Time(3, 45))
                buildQueue[Zerg_Overlord] = max(3, buildQueue[Zerg_Overlord]);

            if (Players::ZvP() && Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) > 0 && vis(Zerg_Lair) == 0 && vis(Zerg_Hydralisk_Den) == 0 && vis(Zerg_Spore_Colony) == 0)
                buildQueue[Zerg_Overlord] = 0;
        }

        void queueGeysers()
        {
            // Adding Extractors
            if (shouldAddGas() && !inOpeningBook)
                buildQueue[Zerg_Extractor] = min(vis(Zerg_Extractor) + 1, Resources::getGasCount());

            // Prevent extractors if no gas
            if (gasLimit == 0 && !Players::ZvZ())
                buildQueue[Zerg_Extractor] = 0;
        }

        void queueUpgradeStructures()
        {
            // Adding Evolution Chambers
            if ((s >= 180 && Stations::getMyStations().size() >= 3)
                || (techUnit == Zerg_Ultralisk && vis(Zerg_Queens_Nest) > 0))
                buildQueue[Zerg_Evolution_Chamber] = 1 + (Stations::getMyStations().size() >= 4);
            if (needSpores)
                buildQueue[Zerg_Evolution_Chamber] = max(buildQueue[Zerg_Evolution_Chamber], 1);

            // Hive upgrades
            if (int(Stations::getMyStations().size()) >= 4 - Players::ZvT() && vis(Zerg_Drone) >= 36) {
                buildQueue[Zerg_Queens_Nest] = 1;
                buildQueue[Zerg_Hive] = com(Zerg_Queens_Nest) >= 1;
                buildQueue[Zerg_Lair] = com(Zerg_Queens_Nest) < 1;
            }
        }

        void queueExpansions()
        {
            if (!inOpeningBook && unitLimits[Zerg_Larva] == 0) {
                const auto hatchCount = vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive);
                const auto availableMinerals = Broodwar->self()->minerals() - BuildOrder::getMinQueued();
                const auto resourceSat = Resources::isGasSaturated() && (int(Stations::getMyStations().size()) >= 4 ? Resources::isMineralSaturated() : Resources::isHalfMineralSaturated());

                expandDesired = (techUnit == None && resourceSat && techSat && productionSat)
                    || (Players::ZvZ() && Players::getTotalCount(PlayerState::Enemy, Zerg_Spore_Colony) > 0 && Stations::getMyStations().size() < Stations::getEnemyStations().size() && availableMinerals > 300)
                    || (vis(Zerg_Drone) >= 16 && int(Stations::getMyStations().size()) <= 1);

                buildQueue[Zerg_Hatchery] = max(buildQueue[Zerg_Hatchery], vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive) + expandDesired + rampDesired);
            }
        }

        void queueProduction()
        {
            auto vsMech = Strategy::getEnemyTransition() == "2Fact"
                || Strategy::getEnemyTransition() == "1FactTanks"
                || Strategy::getEnemyTransition() == "5FactGoliath"
                || Players::getVisibleCount(PlayerState::Enemy, Terran_Factory) >= 3;

            // Adding Hatcheries
            const auto hatchCount = vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive);
            auto desiredProduction = int(Stations::getMyStations().size());
            if (Players::ZvT())
                desiredProduction = int(Stations::getMyStations().size()) + max(0, int(Stations::getMyStations().size()) - 3) + max(0, int(Stations::getMyStations().size()) - 4);
            if (Players::ZvP())
                desiredProduction = int(Stations::getMyStations().size()) + (2 * max(0, int(Stations::getMyStations().size()) - 2)) + (Strategy::getEnemyBuild() != "FFE");
            if (Players::ZvZ())
                desiredProduction = int(Stations::getMyStations().size()) + max(0, int(Stations::getMyStations().size()) - 1) - (int(Stations::getEnemyStations().size() >= 2));

            // Situational increases
            if (Strategy::getEnemyTransition() == "4Gate" && int(Stations::getMyStations().size()) <= 2)
                desiredProduction = 4;

            productionSat = hatchCount >= min(7, desiredProduction);
            auto maxSat = clamp(int(Stations::getMyStations().size()) * 2, 5, 8);
            if (vsMech && int(Stations::getMyStations().size()) >= 4 && armyComposition[Zerg_Zergling] > 0.0)
                maxSat = 24;

            if (!inOpeningBook && unitLimits[Zerg_Larva] == 0) {
                const auto availableMinerals = Broodwar->self()->minerals() - BuildOrder::getMinQueued();

                rampDesired = (Resources::isHalfMineralSaturated() && Resources::isGasSaturated() && !productionSat)
                    || (vis(Zerg_Larva) < min(3, hatchCount) && !productionSat)
                    || (availableMinerals >= 600 && hatchCount < maxSat);

                buildQueue[Zerg_Hatchery] = max(buildQueue[Zerg_Hatchery], vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive) + expandDesired + rampDesired);
            }
        }

        void calculateGasLimit()
        {
            // Calculate the number of gas workers we need outside of the opening book
            if (!inOpeningBook) {
                auto totalMin = 0;
                auto totalGas = 0;
                for (auto &[type, percent] : armyComposition) {
                    if (unitLimits[type] > 0)
                        continue;
                    auto percentExists = max(0.01, double(vis(type) * type.supplyRequired()) / double(s));
                    totalMin += max(0, int(round(percent * double(type.mineralPrice()) / percentExists)));
                    totalGas += max(0, int(round(percent * double(type.gasPrice()) / percentExists)));
                }

                auto resourceRate = (3.0 * 103.0 / (67.1));
                auto resourceCount = double(Resources::getGasCount()) / double(Resources::getMineralCount());

                gasLimit = int(double(vis(Zerg_Drone) * totalGas) / double(totalMin) * resourceRate * resourceCount);
                if (Players::ZvZ())
                    gasLimit += int(1.0 * double(Stations::getMyStations().size()));
                if (Players::getSupply(PlayerState::Self, Races::Zerg) > 100)
                    gasLimit += vis(Zerg_Evolution_Chamber);
            }
        }

        void removeExcessGas()
        {
            // Removing gas workers if we are adding Sunkens or have excess gas
            auto gasRemaining = Broodwar->self()->gas() - BuildOrder::getGasQueued();
            auto minRemaining = Broodwar->self()->minerals() - BuildOrder::getMinQueued();
            auto dropGasRush = (Strategy::enemyRush() && Broodwar->self()->gas() > 200);
            auto dropGasExcess = gasRemaining > 15 * vis(Zerg_Drone) - 100;
            auto dropGasDefenses = needSunks && Util::getTime() < Time(4, 00);
            auto dropGasBroke = minRemaining < 50 && Broodwar->self()->gas() >= 100 && Util::getTime() < Time(4, 30);
            auto dropGasDrones = !Players::ZvZ() && vis(Zerg_Lair) > 0 && vis(Zerg_Drone) < 18;

            if (unitLimits[Zerg_Larva] < 3 && !rush && !pressure && minRemaining < 100 && (dropGasRush || dropGasExcess || dropGasDefenses || dropGasDrones))
                gasLimit = 0;
            if (dropGasBroke)
                gasLimit = 0;
            if (needSpores && Players::ZvZ() && com(Zerg_Evolution_Chamber) == 0)
                gasLimit = 0;
            if (Units::getMyRoleCount(Role::Worker) < 5)
                gasLimit = 0;
            if (Players::ZvZ() && vis(Zerg_Drone) < 8 && gasRemaining >= 100)
                gasLimit = vis(Zerg_Drone) - 5;
        }
    }

    void opener()
    {
        if (Players::getRaceCount(Races::Unknown, PlayerState::Enemy) > 0)
            againstRandom = true;

        if (againstRandom) {
            if (Players::vZ()) {
                currentBuild = "PoolHatch";
                currentOpener = "Overpool";
                currentTransition = "2HatchSpeedling";
            }
            else if (Players::vT() || Players::vP()) {
                currentBuild = "PoolHatch";
                currentOpener = "Overpool";
                currentTransition = "2HatchMuta";
            }
        }

        if (Broodwar->getGameType() == GameTypes::Team_Free_For_All || Broodwar->getGameType() == GameTypes::Team_Melee) {
            buildQueue[Zerg_Hatchery] = Players::getSupply(PlayerState::Self, Races::None) >= 30;
            currentBuild = "PoolHatch";
            currentOpener = "9Pool";
            currentTransition = "2HatchMuta";
            ZvZ();
        }

        if (Players::vT())
            ZvT();
        else if (Players::vP() || Players::getRaceCount(Races::Unknown, PlayerState::Enemy) > 0)
            ZvP();
        else if (Players::vZ())
            ZvZ();

    }

    void tech()
    {
        auto vsMech = Strategy::getEnemyTransition() == "2Fact"
            || Strategy::getEnemyTransition() == "1FactTanks"
            || Strategy::getEnemyTransition() == "5FactGoliath"
            || Players::getVisibleCount(PlayerState::Enemy, Terran_Factory) >= 3;

        auto vsGoons = Strategy::getEnemyTransition() == "4Gate"
            || Strategy::getEnemyTransition() == "5GateGoon";

        const auto techVal = int(techList.size()) + (2 * Players::ZvT()) + (Players::ZvP()) + vsMech + vsGoons;
        const auto endOfTech = !techOrder.empty() && isTechUnit(techOrder.back());
        techSat = (techVal >= int(Stations::getMyStations().size())) || endOfTech;

        // ZvP
        if (Players::ZvP()) {
            if (Strategy::getEnemyTransition() == "Carriers")
                techOrder ={ Zerg_Mutalisk, Zerg_Hydralisk };
            else if (vsGoons)
                techOrder ={ Zerg_Mutalisk, Zerg_Hydralisk };
            else
                techOrder ={ Zerg_Mutalisk, Zerg_Hydralisk, Zerg_Lurker };
        }

        // ZvT
        if (Players::ZvT()) {
            if (vsMech)
                techOrder ={ Zerg_Mutalisk };
            else
                techOrder ={ Zerg_Mutalisk, Zerg_Ultralisk, Zerg_Defiler };
        }

        // ZvZ
        if (Players::ZvZ())
            techOrder ={ Zerg_Mutalisk };

        // If we have our tech unit, set to none
        if (techComplete())
            techUnit = None;

        // Adding tech
        auto readyToTech = vis(Zerg_Extractor) >= int(Stations::getMyStations().size()) || int(Stations::getMyStations().size()) >= 4 || techList.empty();
        if (!inOpeningBook && readyToTech && techUnit == None && !techSat && productionSat && vis(Zerg_Drone) >= 10)
            getTech = true;

        getNewTech();
        getTechBuildings();
    }

    void situational()
    {
        // Queue up defenses
        needSunks = false;
        needSpores = false;
        buildQueue[Zerg_Creep_Colony] = vis(Zerg_Creep_Colony) + vis(Zerg_Spore_Colony) + vis(Zerg_Sunken_Colony);
        queueWallDefenses();
        queueStationDefenses();
        defensesNow = buildQueue[Zerg_Creep_Colony] > 0 && vis(Zerg_Sunken_Colony) < 1 && (Strategy::getEnemyBuild() == "RaxFact" || Strategy::getEnemyBuild() == "2Gate" || Strategy::getEnemyBuild() == "1GateCore" || Strategy::enemyRush() || Players::ZvZ() || Util::getTime() > Time(6, 30));

        // Queue up supply, upgrade structures
        queueOverlords();
        queueUpgradeStructures();
        queueGeysers();
        queueGasTrick();

        // Outside of opening book, book no longer is in control of queuing production, expansions or gas limits
        queueExpansions();
        queueProduction();
        calculateGasLimit();

        // Optimize our gas mining by dropping gas mining at specific excessive values
        removeExcessGas();
    }

    void composition()
    {
        // Clear composition before setting
        if (!inOpeningBook)
            armyComposition.clear();

        // ZvT
        if (Players::vT() && !inOpeningBook) {
            auto vsMech = Strategy::getEnemyTransition() == "2Fact"
                || Strategy::getEnemyTransition() == "1FactTanks"
                || Strategy::getEnemyTransition() == "5FactGoliath";

            // Cleanup enemy
            if (Util::getTime() > Time(15, 0) && Stations::getEnemyStations().size() == 0 && Terrain::foundEnemy()) {
                armyComposition[Zerg_Drone] =                   0.40;
                armyComposition[Zerg_Mutalisk] =                0.60;
                currentComposition =                            Composition::Muta;
            }

            // Defiler tech
            else if (isTechUnit(Zerg_Defiler)) {

                if (s > 360 || (Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Siege_Mode) + Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Tank_Mode)) > 12) {
                    armyComposition[Zerg_Drone] =               0.50;
                    armyComposition[Zerg_Zergling] =            0.15;
                    armyComposition[Zerg_Mutalisk] =            0.20;
                    armyComposition[Zerg_Ultralisk] =           0.10;
                    armyComposition[Zerg_Defiler] =             0.05;
                    currentComposition =                        Composition::UltraLingDefiler;
                }
                else {
                    armyComposition[Zerg_Drone] =               0.55;
                    armyComposition[Zerg_Zergling] =            0.30;
                    armyComposition[Zerg_Ultralisk] =           0.10;
                    armyComposition[Zerg_Defiler] =             0.05;
                    currentComposition =                        Composition::UltraLingDefiler;
                }
            }

            // Ultralisk tech
            else if (isTechUnit(Zerg_Ultralisk) && com(Zerg_Hive)) {
                if (s > 360 || (Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Siege_Mode) + Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Tank_Mode)) > 6) {
                    armyComposition[Zerg_Drone] =               0.55;
                    armyComposition[Zerg_Mutalisk] =            0.20;
                    armyComposition[Zerg_Zergling] =            0.15;
                    armyComposition[Zerg_Ultralisk] =           0.10;
                    currentComposition =                        Composition::UltraLing;
                }
                else {
                    armyComposition[Zerg_Drone] =               0.60;
                    armyComposition[Zerg_Zergling] =            0.30;
                    armyComposition[Zerg_Ultralisk] =           0.10;
                    currentComposition =                        Composition::UltraLing;
                }
            }

            // Lurker tech
            else if (isTechUnit(Zerg_Lurker) && atPercent(TechTypes::Lurker_Aspect, 0.8)) {
                armyComposition[Zerg_Drone] =                   0.60;
                armyComposition[Zerg_Zergling] =                0.15;
                armyComposition[Zerg_Hydralisk] =               0.05;
                armyComposition[Zerg_Lurker] =                  1.00;
                armyComposition[Zerg_Mutalisk] =                0.20;
                currentComposition =                            Composition::MutaLurkerLing;
            }

            // Hydralisk tech
            else if (isTechUnit(Zerg_Hydralisk) && isTechUnit(Zerg_Mutalisk)) {
                armyComposition[Zerg_Drone] =                   0.70;
                armyComposition[Zerg_Zergling] =                0.00;
                armyComposition[Zerg_Hydralisk] =               0.20;
                armyComposition[Zerg_Mutalisk] =                0.10;
                currentComposition =                            Composition::HydraMuta;
            }

            // Mutalisk tech
            else if (isTechUnit(Zerg_Mutalisk)) {
                if (total(Zerg_Mutalisk) >= 32 && com(Zerg_Hive) > 0) {
                    armyComposition[Zerg_Drone] =               0.40;
                    armyComposition[Zerg_Zergling] =            0.40;
                    armyComposition[Zerg_Mutalisk] =            0.20;
                    currentComposition =                        Composition::MutaLing;
                }
                else {
                    armyComposition[Zerg_Drone] =               0.70;
                    armyComposition[Zerg_Zergling] =            0.00;
                    armyComposition[Zerg_Mutalisk] =            0.30;
                    currentComposition =                        Composition::Muta;
                }
            }

            // No tech
            else {
                if (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0) {
                    armyComposition[Zerg_Drone] =               1.00;
                    armyComposition[Zerg_Zergling] =            0.00;
                    currentComposition =                        Composition::None;
                }
                else {
                    armyComposition[Zerg_Drone] =               0.40;
                    armyComposition[Zerg_Zergling] =            0.60;
                    currentComposition =                        Composition::Ling;
                }
            }
        }

        // ZvP
        if (Players::vP() && !inOpeningBook) {
            if (isTechUnit(Zerg_Defiler)) {
                armyComposition[Zerg_Drone] =                   0.58;
                armyComposition[Zerg_Zergling] =                0.02;
                armyComposition[Zerg_Hydralisk] =               0.25;
                armyComposition[Zerg_Mutalisk] =                0.10;
                armyComposition[Zerg_Defiler] =                 0.05;
                currentComposition =                            Composition::HydraMutaDefiler;
            }
            else if (isTechUnit(Zerg_Hydralisk) && isTechUnit(Zerg_Mutalisk) && isTechUnit(Zerg_Lurker)) {
                armyComposition[Zerg_Drone] =                   0.60;
                armyComposition[Zerg_Zergling] =                0.00;
                armyComposition[Zerg_Hydralisk] =               0.25;
                armyComposition[Zerg_Lurker] =                  0.05;
                armyComposition[Zerg_Mutalisk] =                0.10;
                currentComposition =                            Composition::HydraLurker;
            }
            else if (isTechUnit(Zerg_Hydralisk) && isTechUnit(Zerg_Mutalisk)) {
                armyComposition[Zerg_Drone] =                   0.70;
                armyComposition[Zerg_Zergling] =                0.00;
                armyComposition[Zerg_Hydralisk] =               0.20;
                armyComposition[Zerg_Mutalisk] =                0.10;
                currentComposition =                            Composition::HydraMuta;
            }
            else if (isTechUnit(Zerg_Mutalisk)) {
                armyComposition[Zerg_Drone] =                   0.70;
                armyComposition[Zerg_Zergling] =                0.00;
                armyComposition[Zerg_Mutalisk] =                0.30;
                currentComposition =                            Composition::Muta;
            }
            else if (isTechUnit(Zerg_Lurker)) {
                armyComposition[Zerg_Drone] =                   0.60;
                armyComposition[Zerg_Zergling] =                0.20;
                armyComposition[Zerg_Hydralisk] =               0.20;
                armyComposition[Zerg_Lurker] =                  1.00;
                currentComposition =                            Composition::HydraLurker;
            }
            else {
                armyComposition[Zerg_Drone] =                   0.80;
                armyComposition[Zerg_Zergling] =                0.20;
                currentComposition =                            Composition::Ling;
            }
        }

        // ZvZ
        if (Players::ZvZ() && !inOpeningBook) {
            if (isTechUnit(Zerg_Mutalisk) && Util::getTime() > Time(10, 00) && vis(Zerg_Zergling) >= Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling)) {
                armyComposition[Zerg_Drone] =                   0.50;
                armyComposition[Zerg_Zergling] =                0.00;
                armyComposition[Zerg_Mutalisk] =                0.50;
                currentComposition =                            Composition::MutaLing;
            }
            else if (isTechUnit(Zerg_Mutalisk)) {
                armyComposition[Zerg_Drone] =                   0.40;
                armyComposition[Zerg_Zergling] =                0.20;
                armyComposition[Zerg_Mutalisk] =                0.40;
                currentComposition =                            Composition::MutaLing;
            }
            else {
                armyComposition[Zerg_Drone] =                   0.55;
                armyComposition[Zerg_Zergling] =                0.45;
                currentComposition =                            Composition::Ling;
            }
        }

        // Make lings if we're fully saturated, need to pump lings or we are behind on sunkens
        if (!inOpeningBook) {
            int hatchCount = vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive);
            int pumpLings = 0;
            if (Strategy::getEnemyTransition() == "Robo")
                pumpLings = 12;
            if (Strategy::getEnemyTransition() == "4Gate" && hatchCount >= 4)
                pumpLings = 24;
            if (Resources::isMineralSaturated() && Resources::isGasSaturated() && int(Stations::getMyStations().size()) <= 2)
                pumpLings = 200;
            if (Players::ZvZ() && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) == 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) == 0 && vis(Zerg_Drone) > Players::getVisibleCount(PlayerState::Enemy, Zerg_Drone))
                pumpLings = Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling);

            if (vis(Zerg_Zergling) < pumpLings) {
                armyComposition[Zerg_Zergling] = armyComposition[Zerg_Drone];
                armyComposition[Zerg_Drone] = 0.0;
                unitLimits[Zerg_Zergling] = pumpLings;
            }
        }

        // Specific compositions
        if (isTechUnit(Zerg_Hydralisk) && !Terrain::isIslandMap() && (Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) >= 3 || Players::getVisibleCount(PlayerState::Enemy, Protoss_Carrier) >= 4 || Players::getVisibleCount(PlayerState::Enemy, Protoss_Fleet_Beacon) >= 1 || Strategy::getEnemyTransition() == "DoubleStargate" || Strategy::getEnemyTransition() == "Carriers")) {
            armyComposition.clear();
            armyComposition[Zerg_Drone] = 0.50;
            armyComposition[Zerg_Hydralisk] = 0.50;
        }

        // Determine if we should drone up instead of build army
        if (Util::getTime() < Time(10, 00) && !inOpeningBook && !Strategy::enemyProxy() && !Strategy::enemyRush()) {
            auto enemyGroundRatio =     0.5 - clamp(Players::getStrength(PlayerState::Enemy).groundDefense / (0.01 + Players::getStrength(PlayerState::Enemy).groundToGround + Players::getStrength(PlayerState::Enemy).airToGround + Players::getStrength(PlayerState::Enemy).groundDefense), 0.0, 1.0);
            auto enemyAirRatio =        0.5 - clamp(Players::getStrength(PlayerState::Enemy).airDefense / (0.01 + Players::getStrength(PlayerState::Enemy).airToAir + Players::getStrength(PlayerState::Enemy).groundToAir + Players::getStrength(PlayerState::Enemy).airDefense), 0.0, 1.0);

            auto offsetBy = 0.2 * enemyAirRatio;

            armyComposition[Zerg_Drone] =       max(0.0, armyComposition[Zerg_Drone] - offsetBy);
            armyComposition[Zerg_Mutalisk] =    max(0.0, armyComposition[Zerg_Mutalisk] + offsetBy);
        }

        // Add Scourge if we have Mutas and enemy has air to air
        if (isTechUnit(Zerg_Mutalisk)) {
            auto airCount = Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) + Players::getVisibleCount(PlayerState::Enemy, Zerg_Mutalisk) + Players::getVisibleCount(PlayerState::Enemy, Terran_Wraith);
            auto needScourgeZvP = Players::ZvP() && (((airCount / 2) > vis(Zerg_Scourge) && (airCount >= 3 || vis(Zerg_Mutalisk) == 0) && airCount < 6) || (Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) > 0 && vis(Zerg_Scourge) < 2) || (Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) > 4 && vis(Zerg_Scourge) < 4));
            auto needScourgeZvZ = Players::ZvZ() && (airCount / 2) > vis(Zerg_Scourge) && (total(Zerg_Mutalisk) >= 3 || currentTransition == "2HatchMuta") && vis(Zerg_Scourge) < 2;
            auto needScourgeZvT = Players::ZvT() &&
                ((Strategy::getEnemyTransition() == "2PortWraith" && (airCount >= 3 || vis(Zerg_Mutalisk) == 0) && ((vis(Zerg_Scourge) / 2) - 1 < airCount && airCount < 6 && Players::getStrength(PlayerState::Enemy).airToAir > 0.0))
                    || (Util::getTime() > Time(10, 00) && vis(Zerg_Scourge) < 4));

            if (needScourgeZvP || needScourgeZvZ || needScourgeZvT) {
                armyComposition[Zerg_Scourge] = max(0.20, armyComposition[Zerg_Mutalisk]);
                armyComposition[Zerg_Mutalisk] = 0.00;
            }
        }
    }

    void unlocks()
    {
        unlockedType.clear();

        // Saving larva to burst out tech units
        int limitBy = int(Stations::getMyStations().size()) * 3;
        unitLimits[Zerg_Larva] = 0;
        if ((inOpeningBook || techList.size() == 1) && ((atPercent(Zerg_Spire, 0.50) && com(Zerg_Spire) == 0) || (atPercent(Zerg_Hydralisk_Den, 0.6) && com(Zerg_Hydralisk_Den) == 0)))
            unitLimits[Zerg_Larva] = max(0, limitBy - total(Zerg_Mutalisk) - total(Zerg_Hydralisk));

        // Unlocking units
        unlockedType.insert(Zerg_Overlord);
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

        // Remove Lings if we're against Vults
        if (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0 && total(Zerg_Hive) == 0 && unitLimits[Zerg_Zergling] <= vis(Zerg_Zergling))
            unlockedType.erase(Zerg_Zergling);

        // UMS Unlocking
        if (Broodwar->getGameType() == GameTypes::Use_Map_Settings) {
            for (auto &type : BWAPI::UnitTypes::allUnitTypes()) {
                if (!type.isBuilding() && type.getRace() == Races::Zerg && vis(type) >= 2) {
                    unlockedType.insert(type);
                    if (!type.isWorker())
                        techList.insert(type);
                }
            }
        }
    }
}