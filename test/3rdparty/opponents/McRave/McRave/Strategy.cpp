#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Strategy {

    namespace {

        struct Strat {
            bool possible = false;
            bool confirmed = false;
            int framesTrue = 0;
            int framesRequired = 50;
            string name = "Unknown";

            void update() {
                (possible || name != "Unknown") ? framesTrue++ : framesTrue = 0;
                if (framesTrue > framesRequired)
                    confirmed = true;
                else {
                    possible = false;
                    name = "Unknown";
                }
            }
        };

        struct StrategySelections {
            Strat expand, rush, wall, proxy, early, steal, pressure, greedy, invis;
            Strat build, opener, transition;
            vector<Strat*> listOfStrats;

            StrategySelections() {
                listOfStrats ={ &expand, &rush, &wall, &proxy, &early, &steal, &pressure, &greedy, &invis, &build, &opener, &transition };
                build.framesRequired = 360;
                opener.framesRequired = 360;
                transition.framesRequired = 360;
            }
        };

        StrategySelections enemyStrat;
        int workersNearUs = 0;
        int enemyGas = 0;
        set<UnitType> typeUpgrading;
        Time rushArrivalTime;
        Time enemyBuildTime, enemyOpenerTime, enemyTransitionTime;

        struct Timings {
            vector<Time> countStartedWhen;
            vector<Time> countCompletedWhen;
            vector<Time> countArrivesWhen;
            Time firstStartedWhen;
            Time firstCompletedWhen;
            Time firstArrivesWhen;
        };
        map<UnitType, Timings> timingsList;
        set<Unit> unitsStored;

        bool finishedSooner(UnitType t1, UnitType t2)
        {
            if (timingsList.find(t1) == timingsList.end())
                return false;
            if (timingsList.find(t2) == timingsList.end())
                return true;

            auto timings1 = timingsList[t1];
            auto timings2 = timingsList[t2];
            return (timings1.firstCompletedWhen < timings2.firstCompletedWhen);
        }

        bool startedEarlier(UnitType t1, UnitType t2)
        {
            if (timingsList.find(t1) == timingsList.end())
                return false;
            if (timingsList.find(t2) == timingsList.end())
                return true;

            auto timings1 = timingsList[t1];
            auto timings2 = timingsList[t2];
            return (timings1.firstStartedWhen < timings2.firstStartedWhen);
        }

        bool completesBy(int count, UnitType type, Time beforeThis)
        {
            int current = 0;
            for (auto &time : timingsList[type].countCompletedWhen) {
                if (time <= beforeThis)
                    current++;
            }
            return current >= count;
        }

        bool arrivesBy(int count, UnitType type, Time beforeThis)
        {
            int current = 0;
            for (auto &time : timingsList[type].countArrivesWhen) {
                if (time <= beforeThis)
                    current++;
            }
            return current >= count;
        }

        void enemyStrategicInfo(PlayerInfo& player)
        {
            // Monitor for a wall
            if (Players::vT() && Terrain::getEnemyStartingPosition().isValid() && Util::getTime() < Time(5, 00)) {
                auto pathPoint = Terrain::getEnemyStartingPosition();
                auto closest = DBL_MAX;

                for (int x = Terrain::getEnemyStartingTilePosition().x - 2; x < Terrain::getEnemyStartingTilePosition().x + 5; x++) {
                    for (int y = Terrain::getEnemyStartingTilePosition().y - 2; y < Terrain::getEnemyStartingTilePosition().y + 5; y++) {
                        auto center = Position(TilePosition(x, y)) + Position(16, 16);
                        auto dist = center.getDistance(Terrain::getEnemyStartingPosition());
                        if (dist < closest && BWEB::Map::isUsed(TilePosition(x, y)) == UnitTypes::None) {
                            closest = dist;
                            pathPoint = center;
                        }
                    }
                }

                BWEB::Path newPath(Position(BWEB::Map::getMainChoke()->Center()), pathPoint, Zerg_Zergling);
                newPath.generateJPS([&](auto &t) { return newPath.unitWalkable(t); });
                if (!newPath.isReachable())
                    enemyStrat.wall.possible = true;
            }

            workersNearUs = 0;
            for (auto &u : player.getUnits()) {
                UnitInfo &unit =*u;

                // Monitor the soonest the enemy will scout us
                if (unit.getType().isWorker()) {
                    if (Terrain::isInAllyTerritory(unit.getTilePosition()))
                        workersNearUs++;
                }

                // Monitor gas intake or gas steal
                if (unit.getType().isRefinery() && unit.unit()->exists()) {
                    if (Terrain::isInAllyTerritory(unit.getTilePosition()))
                        enemyStrat.steal.possible = true;
                    else
                        enemyGas = unit.unit()->getInitialResources() - unit.unit()->getResources();
                }

                // Monitor for any upgrades coming
                if (unit.unit()->isUpgrading() && typeUpgrading.find(unit.getType()) == typeUpgrading.end())
                    typeUpgrading.insert(unit.getType());

                // Estimate the finishing frame - HACK: Sometimes time arrival was negative
                if (unitsStored.find(unit.unit()) == unitsStored.end() && (unit.getType().isBuilding() || unit.timeArrivesWhen().minutes > 0)) {
                    timingsList[unit.getType()].countStartedWhen.push_back(unit.timeStartedWhen());
                    timingsList[unit.getType()].countCompletedWhen.push_back(unit.timeCompletesWhen());
                    timingsList[unit.getType()].countArrivesWhen.push_back(unit.timeArrivesWhen());

                    if (!unit.getType().isBuilding())
                        McRave::easyWrite(string(unit.getType().c_str()) + " arrives at " + unit.timeArrivesWhen().toString() + ", observed at " + Util::getTime().toString());
                    else
                        McRave::easyWrite(string(unit.getType().c_str()) + " completes at " + unit.timeCompletesWhen().toString() + ", observed at " + Util::getTime().toString());

                    if (timingsList[unit.getType()].firstArrivesWhen == Time(999, 0) || unit.timeArrivesWhen() < timingsList[unit.getType()].firstArrivesWhen)
                        timingsList[unit.getType()].firstArrivesWhen = unit.timeArrivesWhen();

                    if (timingsList[unit.getType()].firstStartedWhen == Time(999, 0) || unit.timeStartedWhen() < timingsList[unit.getType()].firstStartedWhen)
                        timingsList[unit.getType()].firstStartedWhen = unit.timeStartedWhen();

                    if (timingsList[unit.getType()].firstCompletedWhen == Time(999, 0) || unit.timeCompletesWhen() < timingsList[unit.getType()].firstCompletedWhen)
                        timingsList[unit.getType()].firstCompletedWhen = unit.timeCompletesWhen();

                    unitsStored.insert(unit.unit());
                }

                // Monitor for a fast expand
                if (unit.getType().isResourceDepot()) {
                    for (auto &base : Terrain::getAllBases()) {
                        if (!base->Starting() && base->Center().getDistance(unit.getPosition()) < 64.0)
                            enemyStrat.expand.possible = true;
                    }
                }

                // Proxy detection
                if (Util::getTime() < Time(5, 00)) {
                    if (unit.isProxy())
                        enemyStrat.proxy.possible = true;
                }
            }
        }

        void enemyZergBuilds(PlayerInfo& player)
        {
            auto enemyHatchCount = Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hive);

            for (auto &u : player.getUnits()) {
                UnitInfo &unit =*u;

                // If this is our first time seeing a Zergling, see how soon until it arrives
                if (unit.getType() == UnitTypes::Zerg_Zergling) {
                    if (unit.timeArrivesWhen().minutes > 0 && unit.timeArrivesWhen() < rushArrivalTime)
                        rushArrivalTime = unit.timeArrivesWhen();
                }
            }

            // Zerg builds
            if ((enemyStrat.expand.possible || enemyHatchCount > 1) && completesBy(1, Zerg_Hatchery, Time(3, 15)))
                enemyStrat.build.name = "HatchPool";
            else if ((enemyStrat.expand.possible || enemyHatchCount > 1) && completesBy(1, Zerg_Hatchery, Time(4, 00)))
                enemyStrat.build.name = "PoolHatch";
            else if (!enemyStrat.expand.possible && enemyHatchCount == 1 && Players::getCompleteCount(PlayerState::Enemy, Zerg_Lair) > 0 && Terrain::foundEnemy() && Util::getTime() < Time(4, 00))
                enemyStrat.build.name = "PoolLair";

            // 4Pool
            if (rushArrivalTime < Time(2, 45) || completesBy(1, Zerg_Zergling, Time(2, 05)) || completesBy(1, Zerg_Spawning_Pool, Time(1, 40)) || arrivesBy(8, Zerg_Zergling, Time(3, 00)))
                enemyStrat.build.name = "PoolHatch";
        }

        void enemyZergOpeners(PlayerInfo& player)
        {
            auto enemyHatchCount = Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hive);

            // Pool timing
            if (enemyStrat.build.name == "PoolHatch" || enemyStrat.build.name == "PoolLair" || (vis(Zerg_Zergling) > 0 && Util::getTime() < Time(2, 40))) {
                if (rushArrivalTime < Time(2, 45) || completesBy(1, Zerg_Zergling, Time(2, 05)) || completesBy(1, Zerg_Spawning_Pool, Time(1, 40)) || arrivesBy(8, Zerg_Zergling, Time(3, 00)))
                    enemyStrat.opener.name = "4Pool";
                else if (rushArrivalTime < Time(2, 45))
                    enemyStrat.opener.name = "7Pool";
                else if (rushArrivalTime < Time(3, 00)
                    || completesBy(6, Zerg_Zergling, Time(2, 40))
                    || completesBy(8, Zerg_Zergling, Time(3, 00))
                    || completesBy(10, Zerg_Zergling, Time(3, 10))
                    || completesBy(12, Zerg_Zergling, Time(3, 20))
                    || completesBy(14, Zerg_Zergling, Time(3, 30))
                    || completesBy(1, Zerg_Spawning_Pool, Time(2, 00)))
                    enemyStrat.opener.name = "9Pool";
                else if (enemyStrat.expand.possible && (rushArrivalTime < Time(3, 50) || completesBy(1, Zerg_Zergling, Time(3, 00)) || completesBy(1, Zerg_Spawning_Pool, Time(2, 00))))
                    enemyStrat.opener.name = "12Pool";
            }

            // Hatch timing
            if (enemyStrat.build.name == "HatchPool") {
                if (completesBy(1, Zerg_Hatchery, Time(2, 50)))
                    enemyStrat.opener.name = "9Hatch";
                if (completesBy(1, Zerg_Hatchery, Time(3, 05)))
                    enemyStrat.opener.name = "10Hatch";
            }
        }

        void enemyZergTransitions(PlayerInfo& player)
        {
            auto enemyHatchCount = Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hive);

            // Ling rush detection
            if (enemyStrat.opener.name == "4Pool"
                || (!Players::ZvZ() && enemyStrat.opener.name == "9Pool" && Util::getTime() > Time(3, 30) && Util::getTime() < Time(4, 30) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) == 0 && enemyHatchCount == 1))
                enemyStrat.transition.name = "LingRush";

            // Zergling all-in transitions
            if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) == 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) == 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) == 0) {
                if (Players::ZvZ() && Util::getTime() > Time(3, 30) && completesBy(2, Zerg_Hatchery, Time(4, 00)))
                    enemyStrat.transition.name = "2HatchSpeedling";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 3 && enemyGas < 148 && enemyGas >= 50 && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 12)
                    enemyStrat.transition.name = "3HatchSpeedling";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Evolution_Chamber) > 0 && !enemyStrat.expand.possible && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 12)
                    enemyStrat.transition.name = "+1Ling";
            }

            // Hydralisk/Lurker build detection
            if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) == 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) > 0) {
                if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 3)
                    enemyStrat.transition.name = "3HatchHydra";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 2)
                    enemyStrat.transition.name = "2HatchHydra";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 1)
                    enemyStrat.transition.name = "1HatchHydra";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 2)
                    enemyStrat.transition.name = "2HatchLurker";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) == 1 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 0)
                    enemyStrat.transition.name = "1HatchLurker";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) >= 4)
                    enemyStrat.transition.name = "5Hatch";
            }

            // Mutalisk transition detection
            if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) == 0) {
                if ((Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) > 0) && enemyHatchCount == 3)
                    enemyStrat.transition.name = "3HatchMuta";
                else if (Util::getTime() < Time(3, 30) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) > 0 && (enemyHatchCount == 2 || enemyStrat.expand.possible))
                    enemyStrat.transition.name = "2HatchMuta";
                else if (Util::getTime() < Time(3, 00) && enemyHatchCount == 1 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) > 0 && Players::ZvZ()
                    || (completesBy(1, Zerg_Spire, Time(5, 15))))
                    enemyStrat.transition.name = "1HatchMuta";
            }
        }

        void enemyTerranBuilds(PlayerInfo& player)
        {
            for (auto &u : player.getUnits()) {
                UnitInfo &unit = *u;

                // Marine timing
                if (unit.getType() == Terran_Marine && Players::getTotalCount(PlayerState::Enemy, Terran_Marine) >= 2) {
                    if (enemyStrat.transition.name == "Unknown" && unit.timeArrivesWhen().minutes > 0 && unit.timeArrivesWhen() < rushArrivalTime)
                        rushArrivalTime = unit.timeArrivesWhen();
                }

                // FE Detection
                if (Util::getTime() < Time(4, 00) && unit.getType() == Terran_Bunker && Terrain::getEnemyNatural() && Terrain::getEnemyMain()) {
                    auto closestMain = BWEB::Stations::getClosestMainStation(unit.getTilePosition());
                    if (closestMain && Stations::ownedBy(closestMain) != PlayerState::Self) {
                        auto natDistance = unit.getPosition().getDistance(Position(Terrain::getEnemyNatural()->getChokepoint()->Center()));
                        auto mainDistance = unit.getPosition().getDistance(Position(Terrain::getEnemyMain()->getChokepoint()->Center()));
                        if (natDistance < mainDistance)
                            enemyStrat.expand.possible = true;
                    }
                }
            }

            auto hasMech = Players::getVisibleCount(PlayerState::Enemy, Terran_Vulture) > 0
                || Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Siege_Mode) > 0
                || Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Tank_Mode) > 0
                || Players::getVisibleCount(PlayerState::Enemy, Terran_Goliath) > 0;

            // 2Rax
            if ((rushArrivalTime < Time(3, 10) && Util::getTime() < Time(3, 25) && Players::getTotalCount(PlayerState::Enemy, Terran_Marine) >= 3)
                || (Util::getTime() < Time(2, 55) && Players::getTotalCount(PlayerState::Enemy, Terran_Barracks) >= 2)
                || (Util::getTime() < Time(4, 00) && Players::getTotalCount(PlayerState::Enemy, Terran_Barracks) >= 2 && Players::getTotalCount(PlayerState::Enemy, Terran_Refinery) == 0)
                || (Util::getTime() < Time(3, 15) && Players::getTotalCount(PlayerState::Enemy, Terran_Marine) >= 5)
                || (Util::getTime() < Time(3, 35) && Players::getTotalCount(PlayerState::Enemy, Terran_Marine) >= 7)
                || (Util::getTime() < Time(3, 55) && Players::getTotalCount(PlayerState::Enemy, Terran_Marine) >= 9))
                enemyStrat.build.name = "2Rax";

            // RaxCC
            if ((completesBy(2, Terran_Command_Center, Time(4, 30)))
                || rushArrivalTime < Time(2, 45)
                || completesBy(1, Terran_Barracks, Time(1, 40))
                || (enemyStrat.expand.possible && Util::getTime() < Time(4, 00))
                || (enemyStrat.proxy.possible && Players::getVisibleCount(PlayerState::Enemy, Terran_Barracks) == 1))
                enemyStrat.build.name = "RaxCC";

            // RaxFact
            if (Util::getTime() < Time(3, 00) && Players::getTotalCount(PlayerState::Enemy, Terran_Factory) > 0
                || Util::getTime() < Time(5, 00) && hasMech)
                enemyStrat.build.name = "RaxFact";

            // 2Rax Proxy - No info estimation
            if (Scouts::gotFullScout() && Util::getTime() < Time(3, 30) && Players::getVisibleCount(PlayerState::Enemy, Terran_Barracks) == 0 && Players::getVisibleCount(PlayerState::Enemy, Terran_Refinery) == 0 && Players::getVisibleCount(PlayerState::Enemy, Terran_Command_Center) <= 1) {
                enemyStrat.build.name = "2Rax";
                enemyStrat.proxy.possible = true;
            }
        }

        void enemyTerranOpeners(PlayerInfo& player)
        {
            // Bio builds
            if (enemyStrat.build.name == "2Rax") {
                if (enemyStrat.expand.possible)
                    enemyStrat.opener.name = "Expand";
                else if (enemyStrat.proxy.possible)
                    enemyStrat.opener.name = "Proxy";
                else if (Util::getTime() > Time(4, 00))
                    enemyStrat.opener.name = "Main";
            }

            // Mech builds
            if (enemyStrat.build.name == "RaxFact") {
            }

            // Expand builds
            if (enemyStrat.build.name == "RaxCC") {
                if (rushArrivalTime < Time(2, 45)
                    || completesBy(1, Terran_Barracks, Time(1, 50))
                    || completesBy(1, Terran_Marine, Time(2, 05)))
                    enemyStrat.opener.name = "8Rax";
                else if (enemyStrat.expand.possible)
                    enemyStrat.opener.name = "1RaxFE";
            }
        }

        void enemyTerranTransitions(PlayerInfo& player)
        {
            auto hasTanks = Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Siege_Mode) > 0 || Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Tank_Mode) > 0;
            auto hasGols = Players::getVisibleCount(PlayerState::Enemy, Terran_Goliath) > 0;
            auto hasWraiths = Players::getVisibleCount(PlayerState::Enemy, Terran_Wraith) > 0;

            if (workersNearUs >= 3 && Util::getTime() < Time(3, 00))
                enemyStrat.transition.name = "WorkerRush";

            // PvT
            if (Players::PvT()) {
                if ((Players::getVisibleCount(PlayerState::Enemy, Terran_Siege_Tank_Siege_Mode) > 0 && Players::getVisibleCount(PlayerState::Enemy, Terran_Vulture) == 0)
                    || (enemyStrat.expand.possible && Players::getVisibleCount(PlayerState::Enemy, Terran_Machine_Shop) > 0))
                    enemyStrat.transition.name = "SiegeExpand";

                if ((Players::getTotalCount(PlayerState::Enemy, Terran_Vulture_Spider_Mine) > 0 && Util::getTime() < Time(6, 00))
                    || (Players::getTotalCount(PlayerState::Enemy, Terran_Factory) >= 2 && typeUpgrading.find(Terran_Machine_Shop) != typeUpgrading.end())
                    || (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) >= 3 && Util::getTime() < Time(5, 30))
                    || (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) >= 5 && Util::getTime() < Time(6, 00)))
                    enemyStrat.transition.name = "2Fact";
            }

            // ZvT
            if (Players::ZvT()) {

                // RaxFact
                if (enemyStrat.build.name == "RaxFact") {
                    if ((hasGols && enemyStrat.expand.possible && Players::getVisibleCount(PlayerState::Enemy, Terran_Factory) >= 4) || (Util::getTime() < Time(8, 30) && Players::getVisibleCount(PlayerState::Enemy, Terran_Armory) > 0))
                        enemyStrat.transition.name = "5FactGoliath";
                    if (hasWraiths && Util::getTime() < Time(6, 00))
                        enemyStrat.transition.name = "2PortWraith";

                    if ((Players::getTotalCount(PlayerState::Enemy, Terran_Machine_Shop) >= 2 && typeUpgrading.find(Terran_Machine_Shop) != typeUpgrading.end() && Players::getTotalCount(PlayerState::Enemy, Terran_Vulture_Spider_Mine) > 0)
                        || (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) >= 3 && Util::getTime() < Time(5, 00))
                        || (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) >= 5 && Util::getTime() < Time(5, 30))
                        || (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) >= 7 && Util::getTime() < Time(6, 00)))
                        enemyStrat.transition.name = "2Fact";
                }

                // 2Rax
                if (enemyStrat.build.name == "2Rax") {
                    if (enemyStrat.expand.possible && (hasTanks || Players::getVisibleCount(PlayerState::Enemy, Terran_Machine_Shop) > 0) && Players::getVisibleCount(PlayerState::Enemy, Terran_Factory) <= 1 && Players::getVisibleCount(PlayerState::Enemy, Terran_Barracks) >= 3 && Util::getTime() < Time(10, 30))
                        enemyStrat.transition.name = "1FactTanks";
                    else if (rushArrivalTime < Time(3, 10)
                        || completesBy(2, Terran_Barracks, Time(2, 35))
                        || completesBy(3, Terran_Barracks, Time(4, 00)))
                        enemyStrat.transition.name = "MarineRush";
                    else if (!enemyStrat.expand.possible && (completesBy(1, Terran_Academy, Time(5, 10)) || player.hasTech(TechTypes::Stim_Packs) || arrivesBy(1, Terran_Medic, Time(6, 00)) || arrivesBy(1, Terran_Firebat, Time(6, 00))))
                        enemyStrat.transition.name = "Academy";
                    else if (enemyStrat.expand.possible && Players::getVisibleCount(PlayerState::Enemy, Terran_Barracks) >= 5 && Util::getTime() < Time(7, 00))
                        enemyStrat.transition.name = "+1 5Rax";
                }

                // RaxCC
                if (enemyStrat.build.name == "RaxCC") {
                    if (enemyStrat.expand.possible && (hasTanks || Players::getVisibleCount(PlayerState::Enemy, Terran_Machine_Shop) > 0) && Players::getVisibleCount(PlayerState::Enemy, Terran_Factory) <= 1 && Players::getVisibleCount(PlayerState::Enemy, Terran_Barracks) >= 3 && Util::getTime() < Time(10, 30))
                        enemyStrat.transition.name = "1FactTanks";
                    if ((hasGols && enemyStrat.expand.possible && Players::getVisibleCount(PlayerState::Enemy, Terran_Factory) >= 4 && Players::getVisibleCount(PlayerState::Enemy, Terran_Armory) > 0)
                        || (Util::getTime() < Time(7, 30) && Players::getVisibleCount(PlayerState::Enemy, Terran_Armory) > 0)
                        || (Util::getTime() < Time(7, 30) && Players::getTotalCount(PlayerState::Enemy, Terran_Goliath) >= 5)
                        || (Util::getTime() < Time(8, 30) && Players::getTotalCount(PlayerState::Enemy, Terran_Goliath) >= 8))
                        enemyStrat.transition.name = "5FactGoliath";
                    else if (enemyStrat.expand.possible && Players::getVisibleCount(PlayerState::Enemy, Terran_Barracks) >= 5 && Util::getTime() < Time(7, 00))
                        enemyStrat.transition.name = "+1 5Rax";
                }
            }

            // TvT
        }

        void enemyProtossBuilds(PlayerInfo& player)
        {
            for (auto &u : player.getUnits()) {
                UnitInfo &unit = *u;

                // Zealot timing
                if (unit.getType() == Protoss_Zealot) {
                    if (unit.timeArrivesWhen().minutes > 0 && unit.timeArrivesWhen() < rushArrivalTime)
                        rushArrivalTime = unit.timeArrivesWhen();
                }

                // CannonRush
                if (unit.getType() == Protoss_Forge && Scouts::gotFullScout() && Terrain::getEnemyStartingPosition().isValid() && unit.getPosition().getDistance(Terrain::getEnemyStartingPosition()) < 200.0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Nexus) <= 1) {
                    enemyStrat.build.name = "CannonRush";
                    enemyStrat.proxy.possible = true;
                }
                if ((unit.getType() == Protoss_Forge || unit.getType() == Protoss_Photon_Cannon) && unit.isProxy()) {
                    enemyStrat.build.name = "CannonRush";
                    enemyStrat.proxy.possible = true;
                }
                if (unit.getType() == Protoss_Pylon && unit.isProxy())
                    enemyStrat.early.possible = true;

                // FFE
                if (Util::getTime() < Time(4, 00) && Terrain::getEnemyNatural() && (unit.getType() == Protoss_Photon_Cannon || unit.getType() == Protoss_Forge)) {
                    if (unit.getPosition().getDistance(Position(Terrain::getEnemyNatural()->getChokepoint()->Center())) < 320.0 || unit.getPosition().getDistance(Position(Terrain::getEnemyNatural()->getBase()->Center())) < 320.0) {
                        enemyStrat.build.name = "FFE";
                        enemyStrat.expand.possible = true;
                    }
                }
            }

            // 1GateCore - Gas estimation
            if ((completesBy(1, Protoss_Assimilator, Time(2, 15)) && !enemyStrat.steal.possible)
                || (Players::getTotalCount(PlayerState::Enemy, Protoss_Gateway) > 0 && completesBy(1, Protoss_Assimilator, Time(2, 50)) && !enemyStrat.steal.possible))
                enemyStrat.build.name = "1GateCore";

            // 1GateCore - Core estimation
            if (completesBy(1, Protoss_Cybernetics_Core, Time(3, 25)))
                enemyStrat.build.name = "1GateCore";

            // 1GateCore - Goon estimation
            if (completesBy(1, Protoss_Dragoon, Time(4, 10))
                || completesBy(2, Protoss_Dragoon, Time(4, 30)))
                enemyStrat.build.name = "1GateCore";

            // 1GateCore - Tech estimation
            if (completesBy(1, Protoss_Scout, Time(5, 15))
                || completesBy(1, Protoss_Corsair, Time(5, 15))
                || completesBy(1, Protoss_Stargate, Time(4, 10)))
                enemyStrat.build.name = "1GateCore";

            // 1GateCore - Turtle estimation
            if (Players::getTotalCount(PlayerState::Enemy, Protoss_Forge) > 0
                && Players::getTotalCount(PlayerState::Enemy, Protoss_Gateway) > 0
                && Players::getTotalCount(PlayerState::Enemy, Protoss_Cybernetics_Core) > 0
                && Util::getTime() < Time(4, 00))
                enemyStrat.build.name = "1GateCore";

            // 1GateCore - fallback assumption
            if (Util::getTime() > Time(3, 45) && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 2)
                enemyStrat.build.name = "1GateCore";

            // 2Gate Proxy - No info estimation
            if (Scouts::gotFullScout() && Util::getTime() < Time(3, 30) && !completesBy(1, Protoss_Pylon, Time(1,15)) && Players::getVisibleCount(PlayerState::Enemy, Protoss_Forge) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Nexus) <= 1) {
                enemyStrat.build.name = "2Gate";
                enemyStrat.proxy.possible = true;
            }

            // 2Gate - Zealot estimation
            if ((Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 5 && Util::getTime() < Time(4, 0))
                || (Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 3 && Util::getTime() < Time(3, 30))
                || (enemyStrat.proxy.possible == true && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) > 0)
                || arrivesBy(2, Protoss_Zealot, Time(3, 25))
                || arrivesBy(3, Protoss_Zealot, Time(4, 00))
                || arrivesBy(4, Protoss_Zealot, Time(4, 20))
                || completesBy(2, Protoss_Gateway, Time(2, 55))
                || completesBy(3, Protoss_Zealot, Time(3, 20))) {
                enemyStrat.build.name = "2Gate";
            }
        }

        void enemyProtossOpeners(PlayerInfo& player)
        {
            // 2Gate Openers
            if (enemyStrat.build.name == "2Gate" || enemyStrat.build.name == "CannonRush") {
                if (enemyStrat.proxy.possible || arrivesBy(1, Protoss_Zealot, Time(2, 50)) || arrivesBy(2, Protoss_Zealot, Time(3, 00)) || arrivesBy(4, Protoss_Zealot, Time(3, 25))) {
                    enemyStrat.opener.name = "Proxy";
                    enemyStrat.proxy.possible = true;
                }
                else if (arrivesBy(2, Protoss_Zealot, Time(3, 25)) || arrivesBy(3, Protoss_Zealot, Time(3, 35)) || arrivesBy(4, Protoss_Zealot, Time(4, 00)) || arrivesBy(5, Protoss_Zealot, Time(4, 10)))
                    enemyStrat.opener.name = "9/9";
                else if ((completesBy(3, Protoss_Zealot, Time(3, 10)) && arrivesBy(3, Protoss_Zealot, Time(4, 00))) || arrivesBy(4, Protoss_Zealot, Time(4, 20)))
                    enemyStrat.opener.name = "10/12";
                else if ((completesBy(3, Protoss_Zealot, Time(3, 35)) && arrivesBy(3, Protoss_Zealot, Time(4, 15))) || arrivesBy(2, Protoss_Dragoon, Time(5, 00)) || completesBy(1, Protoss_Cybernetics_Core, Time(3, 40)))
                    enemyStrat.opener.name = "10/17";
            }

            // FFE Openers - need timings for when Nexus/Forge/Gate complete
            if (enemyStrat.build.name == "FFE") {
                if (startedEarlier(Protoss_Nexus, Protoss_Forge))
                    enemyStrat.opener.name = "Nexus";
                else if (startedEarlier(Protoss_Gateway, Protoss_Forge))
                    enemyStrat.opener.name = "Gateway";
                else
                    enemyStrat.opener.name = "Forge";
            }

            // 1Gate Openers -  need timings for when Core completes
            if (enemyStrat.build.name == "1GateCore") {
                if (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) == 0)
                    enemyStrat.opener.name = "0Zealot";
                else if (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) == 1)
                    enemyStrat.opener.name = "1Zealot";
                else if (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 2)
                    enemyStrat.opener.name = "2Zealot";
            }
        }

        void enemyProtossTransitions(PlayerInfo& player)
        {
            // Rush detection
            if (enemyStrat.build.name == "2Gate") {
                if (enemyStrat.opener.name == "Proxy"
                    || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Assimilator) == 0 && Players::getCompleteCount(PlayerState::Enemy, Protoss_Zealot) >= 8 && Util::getTime() < Time(4, 30))
                    || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Assimilator) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) == 0 && Util::getTime() > Time(6, 00))
                    || completesBy(3, Protoss_Gateway, Time(3, 30)))
                    enemyStrat.transition.name = "ZealotRush";
            }

            if (workersNearUs >= 3 && Util::getTime() < Time(3, 00))
                enemyStrat.transition.name = "WorkerRush";

            // 2Gate
            if (enemyStrat.build.name == "2Gate" || enemyStrat.build.name == "1GateCore") {

                // DT
                if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Citadel_of_Adun) >= 1 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) > 0)
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Templar_Archives) >= 1
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Dark_Templar) >= 1
                    || (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) < 2 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) >= 1 && Util::getTime() > Time(6, 45)))
                    enemyStrat.transition.name = "DT";

                // Corsair
                if (Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) > 0
                    || completesBy(1, Protoss_Scout, Time(5, 15))
                    || completesBy(1, Protoss_Corsair, Time(5, 15)))
                    enemyStrat.transition.name = "Corsair";

                // Robo
                if (Players::getVisibleCount(PlayerState::Enemy, Protoss_Shuttle) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Reaver) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Observer) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Robotics_Facility) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Robotics_Support_Bay) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Observatory) > 0)
                    enemyStrat.transition.name = "Robo";

                // 4Gate
                if (Players::PvP()) {
                    if ((Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 7 && Util::getTime() < Time(6, 30))
                        || (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 11 && Util::getTime() < Time(7, 15))
                        || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) >= 4 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) >= 1 && Util::getTime() < Time(5, 30)))
                        enemyStrat.transition.name = "4Gate";
                }
                if (Players::ZvP()) {

                    if ((!enemyStrat.expand.possible && Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 4)
                        || (typeUpgrading.find(Protoss_Cybernetics_Core) != typeUpgrading.end() && Util::getTime() > Time(4, 00))
                        || (Players::getPlayerInfo(Broodwar->enemy())->hasUpgrade(UpgradeTypes::Singularity_Charge))
                        || (arrivesBy(3, Protoss_Dragoon, Time(5, 45)))
                        || (arrivesBy(5, Protoss_Dragoon, Time(6, 05)))
                        || (completesBy(4, Protoss_Gateway, Time(5, 30)) && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) >= 1))
                        enemyStrat.transition.name = "4Gate";
                }

                // 5ZealotExpand
                if (Players::PvP() && enemyStrat.expand.possible && (Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) >= 2 || Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 5) && Util::getTime() < Time(4, 45))
                    enemyStrat.transition.name = "5ZealotExpand";
            }

            // FFE transitions
            if (Players::ZvP() && enemyStrat.build.name == "FFE") {
                if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) >= 4 && Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 1) || (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 4 && Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) == 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Stargate) == 0))
                    enemyStrat.transition.name = "5GateGoon";
                else if (completesBy(1, Protoss_Stargate, Time(5, 45)) && completesBy(1, Protoss_Citadel_of_Adun, Time(5, 45)) && typeUpgrading.find(Protoss_Forge) != typeUpgrading.end())
                    enemyStrat.transition.name = "NeoBisu";
                else if (completesBy(1, Protoss_Stargate, Time(5, 45)) && completesBy(1, Protoss_Robotics_Facility, Time(6, 30)))
                    enemyStrat.transition.name = "CorsairReaver";
                else if (completesBy(1, Protoss_Templar_Archives, Time(7, 30)))
                    enemyStrat.transition.name = "ZealotArchon";
                else if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) <= 0 && typeUpgrading.find(Protoss_Forge) != typeUpgrading.end() && typeUpgrading.find(Protoss_Cybernetics_Core) == typeUpgrading.end() && Util::getTime() < Time(5, 30)) || (completesBy(1, Protoss_Citadel_of_Adun, Time(5, 30)) && completesBy(2, Protoss_Gateway, Time(5, 45))))
                    enemyStrat.transition.name = "Speedlot";
                else if (completesBy(2, Protoss_Stargate, Time(7, 00)))
                    enemyStrat.transition.name = "DoubleStargate";
                else if (completesBy(1, Protoss_Fleet_Beacon, Time(9, 30)) || completesBy(1, Protoss_Carrier, Time(12, 00)))
                    enemyStrat.transition.name = "Carriers";
            }
        }

        void checkEnemyRush()
        {
            // Rush builds are immediately aggresive builds
            auto supplySafe = Broodwar->self()->getRace() == Races::Zerg ? Players::getSupply(PlayerState::Self, Races::None) >= 60 : Players::getSupply(PlayerState::Self, Races::None) >= 80;
            enemyStrat.rush.possible = !supplySafe && (enemyStrat.transition.name == "MarineRush" || enemyStrat.transition.name == "ZealotRush" || enemyStrat.transition.name == "LingRush" || enemyStrat.transition.name == "WorkerRush");
            if (supplySafe)
                enemyStrat.rush.confirmed = false;
        }

        void checkEnemyPressure()
        {
            // Pressure builds are delayed aggresive builds
            auto supplySafe = Broodwar->self()->getRace() == Races::Zerg ? Players::getSupply(PlayerState::Self, Races::None) >= 100 : Players::getSupply(PlayerState::Self, Races::None) >= 120;
            enemyStrat.pressure.possible = !supplySafe && (enemyStrat.transition.name == "4Gate" || enemyStrat.transition.name == "2HatchSpeedling" || enemyStrat.transition.name == "Sparks" || enemyStrat.transition.name == "2Fact" || enemyStrat.transition.name == "Academy");
            if (supplySafe)
                enemyStrat.pressure.confirmed = false;
        }

        void checkEnemyInvis()
        {
            // DTs, Vultures, Lurkers
            enemyStrat.invis.possible = (Players::getVisibleCount(PlayerState::Enemy, Protoss_Dark_Templar) >= 1 || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Citadel_of_Adun) >= 1 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) > 0) || Players::getVisibleCount(PlayerState::Enemy, Protoss_Templar_Archives) >= 1)
                || (Players::getVisibleCount(PlayerState::Enemy, Terran_Ghost) >= 1 || Players::getVisibleCount(PlayerState::Enemy, Terran_Vulture) >= 4)
                || (Players::getVisibleCount(PlayerState::Enemy, Zerg_Lurker) >= 1 || (Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) >= 1 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) >= 1 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) <= 0))
                || (enemyStrat.build.name == "1HatchLurker" || enemyStrat.build.name == "2HatchLurker" || enemyStrat.build.name == "1GateDT");

            // Protoss
            if (Broodwar->self()->getRace() == Races::Protoss) {
                if (com(Protoss_Observer) > 0)
                    enemyStrat.invis.possible = false;
            }

            // Zerg
            if (Broodwar->self()->getRace() == Races::Zerg) {
                if (Players::getPlayerInfo(Broodwar->self())->hasUpgrade(UpgradeTypes::Pneumatized_Carapace))
                    enemyStrat.invis.possible = false;
            }

            // Terran
            else if (Broodwar->self()->getRace() == Races::Terran) {
                if (com(Terran_Comsat_Station) > 0)
                    enemyStrat.invis.possible = false;
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk) > 0 || Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) > 0)
                    enemyStrat.invis.possible = true;
            }
        }

        void checkEnemyEarly()
        {
            // If we have seen an enemy worker before we've scouted the enemy, follow it
            enemyStrat.early.possible = false;
            if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Probe) > 0 || Players::getVisibleCount(PlayerState::Enemy, Terran_SCV) > 0) && Util::getTime() < Time(2, 00)) {
                auto &enemyWorker = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u.getType().isWorker();
                });
                if (enemyWorker) {
                    auto distMain = enemyWorker->getPosition().getDistance(BWEB::Map::getMainPosition());
                    auto distNat = enemyWorker->getPosition().getDistance(BWEB::Map::getNaturalPosition());
                    enemyStrat.early.possible = enemyWorker && (distMain < 320.0 || distNat < 320.0);
                }
            }
            if (Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) > 0 || Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) > 0 || Util::getTime() > Time(2, 00))
                enemyStrat.early.confirmed = false;
        }

        void checkEnemyProxy()
        {
            // Proxy builds are built closer to me than the enemy
            auto supplySafe = Broodwar->self()->getRace() == Races::Zerg ? Players::getSupply(PlayerState::Self, Races::None) >= 40 : Players::getSupply(PlayerState::Self, Races::None) >= 80;
            if (supplySafe)
                enemyStrat.proxy.confirmed = false;
        }

        void checkEnemyGreedy()
        {
            // Greedy detection
            enemyStrat.greedy.possible = (Players::ZvP() && enemyStrat.build.name != "FFE" && int(Stations::getEnemyStations().size()) >= 3 && Util::getTime() < Time(10, 00))
                || (Players::ZvP() && enemyStrat.build.name != "FFE" && enemyStrat.expand.confirmed && Util::getTime() < Time(6, 45))
                || (Players::ZvT() && int(Stations::getEnemyStations().size()) >= 3 && Util::getTime() < Time(10, 00));
            if (Util::getTime() > Time(10, 00))
                enemyStrat.greedy.confirmed = false;
        }

        void updateEnemyBuild()
        {
            for (auto &p : Players::getPlayers()) {
                PlayerInfo &player = p.second;

                if (player.isEnemy()) {
                    enemyStrategicInfo(player);
                    if (player.getCurrentRace() == Races::Zerg) {
                        if (!enemyStrat.build.confirmed)
                            enemyZergBuilds(player);
                        if (!enemyStrat.opener.confirmed)
                            enemyZergOpeners(player);
                        if (!enemyStrat.transition.confirmed)
                            enemyZergTransitions(player);
                    }
                    else if (player.getCurrentRace() == Races::Protoss) {
                        if (!enemyStrat.build.confirmed)
                            enemyProtossBuilds(player);
                        if (!enemyStrat.opener.confirmed)
                            enemyProtossOpeners(player);
                        if (!enemyStrat.transition.confirmed)
                            enemyProtossTransitions(player);
                    }
                    else if (player.getCurrentRace() == Races::Terran) {
                        if (!enemyStrat.build.confirmed)
                            enemyTerranBuilds(player);
                        if (!enemyStrat.opener.confirmed)
                            enemyTerranOpeners(player);
                        if (!enemyStrat.transition.confirmed)
                            enemyTerranTransitions(player);
                    }
                }
            }

            // Set a timestamp for when we detected a piece of the enemy build order
            if (enemyStrat.build.confirmed && enemyBuildTime == Time(999, 00))
                enemyBuildTime = Util::getTime();
            if (enemyStrat.opener.confirmed && enemyOpenerTime == Time(999, 00))
                enemyOpenerTime = Util::getTime();
            if (enemyStrat.transition.confirmed && enemyTransitionTime == Time(999, 00))
                enemyTransitionTime = Util::getTime();

            // Verify strategy checking for confirmations
            for (auto &strat : enemyStrat.listOfStrats) {
                if (strat)
                    strat->update();
            }
        }

        void globalReactions()
        {
            checkEnemyInvis();
            checkEnemyPressure();
            checkEnemyEarly();
            checkEnemyProxy();
            checkEnemyRush();
            checkEnemyGreedy();
        }
    }

    void onFrame()
    {
        updateEnemyBuild();
        globalReactions();
    }

    string getEnemyBuild() { return enemyStrat.build.name; }
    string getEnemyOpener() { return enemyStrat.opener.name; }
    string getEnemyTransition() { return enemyStrat.transition.name; }
    Time getEnemyBuildTime() { return enemyBuildTime; }
    Time getEnemyOpenerTime() { return enemyOpenerTime; }
    Time getEnemyTransitionTime() { return enemyTransitionTime; }

    int getWorkersNearUs() { return workersNearUs; }
    bool enemyFastExpand() { return enemyStrat.expand.confirmed; }
    bool enemyRush() { return enemyStrat.rush.confirmed; }
    bool enemyInvis() { return enemyStrat.invis.confirmed; }
    bool enemyPossibleProxy() { return enemyStrat.early.confirmed; }
    bool enemyProxy() { return enemyStrat.proxy.confirmed; }
    bool enemyGasSteal() { return enemyStrat.steal.confirmed; }
    bool enemyBust() { return enemyStrat.build.name.find("Hydra") != string::npos; }   // A bit hacky
    bool enemyPressure() { return enemyStrat.pressure.confirmed; }
    bool enemyWalled() { return enemyStrat.wall.confirmed; }
    bool enemyGreedy() { return enemyStrat.greedy.confirmed; }
}