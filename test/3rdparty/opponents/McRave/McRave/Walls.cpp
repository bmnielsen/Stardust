#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace BWEM;
using namespace UnitTypes;

namespace McRave::Walls {

    namespace {
        BWEB::Wall* mainWall = nullptr;
        BWEB::Wall* naturalWall = nullptr;
        vector<UnitType> buildings;
        vector<UnitType> defenses;
        bool tight;
        bool openWall;
        UnitType tightType = None;

        void initializeWallParameters()
        {
            // Figure out what we need to be tight against
            if (Players::TvP())
                tightType = Protoss_Zealot;
            else if (Players::TvZ())
                tightType = Zerg_Zergling;
            else
                tightType = None;

            // Protoss wall parameters
            if (Broodwar->self()->getRace() == Races::Protoss) {
                if (Players::vZ()) {
                    tight = false;
                    buildings ={ Protoss_Gateway, Protoss_Forge, Protoss_Pylon };
                    defenses.insert(defenses.end(), 10, Protoss_Photon_Cannon);
                }
                else {
                    tight = false;
                    buildings ={ Protoss_Forge, Protoss_Pylon, Protoss_Pylon, Protoss_Pylon };
                    defenses.insert(defenses.end(), 10, Protoss_Photon_Cannon);
                }
            }

            // Terran wall parameters
            if (Broodwar->self()->getRace() == Races::Terran) {
                tight = true;
                buildings ={ Terran_Barracks, Terran_Supply_Depot, Terran_Supply_Depot };
            }

            // Zerg wall parameters
            if (Broodwar->self()->getRace() == Races::Zerg) {
                tight = false;
                defenses.insert(defenses.end(), 20, Zerg_Creep_Colony);
                //buildings ={ Zerg_Hatchery, Zerg_Evolution_Chamber };
            }
        }
    }

    void findNaturalWall()
    {
        naturalWall = BWEB::Walls::getWall(BWEB::Map::getNaturalChoke());
    }

    void findMainWall()
    {
        mainWall = BWEB::Walls::getWall(BWEB::Map::getMainChoke());
    }

    void findHardWalls()
    {
        if (!Terrain::isShitMap())
            return;

        // The unfortunate reality of playing Zerg on literally the shittiest map in a tournament.

        multimap<UnitType, TilePosition> wall1 ={
            { Zerg_Sunken_Colony, TilePosition(37,108) },
            { Zerg_Sunken_Colony, TilePosition(35,108) },
            { Zerg_Sunken_Colony, TilePosition(35,110) },
            { Zerg_Sunken_Colony, TilePosition(33,110) },
            { Zerg_Sunken_Colony, TilePosition(33,112) },
            { Zerg_Sunken_Colony, TilePosition(33,114) },
            { Zerg_Sunken_Colony, TilePosition(31,114) },
            { Zerg_Sunken_Colony, TilePosition(31,116) },
            { Zerg_Sunken_Colony, TilePosition(29,116) },
            { Zerg_Sunken_Colony, TilePosition(29,118) },
            { Zerg_Sunken_Colony, TilePosition(27,117) },
            { Zerg_Sunken_Colony, TilePosition(25,118) },
            { Zerg_Hatchery, TilePosition(35,112) }
        };
        auto choke1 = Util::getClosestChokepoint(Position(TilePosition(35, 112)));
        auto area1 = mapBWEM.GetArea(TilePosition(35, 112));
        BWEB::Walls::createHardWall(wall1, area1, choke1);

        multimap<UnitType, TilePosition> wall2 ={
            { Zerg_Sunken_Colony, TilePosition(58,97) },
            { Zerg_Sunken_Colony, TilePosition(58,99) },
            { Zerg_Sunken_Colony, TilePosition(56,97) },
            { Zerg_Sunken_Colony, TilePosition(56,99) },
            { Zerg_Sunken_Colony, TilePosition(56,101) },
            { Zerg_Sunken_Colony, TilePosition(54,99) },
            { Zerg_Sunken_Colony, TilePosition(54,101) },
            { Zerg_Sunken_Colony, TilePosition(54,103) },
            { Zerg_Sunken_Colony, TilePosition(52,101) },
            { Zerg_Sunken_Colony, TilePosition(52,103) },
            { Zerg_Sunken_Colony, TilePosition(52,105) },
            { Zerg_Sunken_Colony, TilePosition(50,103) },
            { Zerg_Hatchery, TilePosition(50,98) }
        };
        auto choke2 = Util::getClosestChokepoint(Position(TilePosition(50, 98)));
        auto area2 = mapBWEM.GetArea(TilePosition(50, 98));
        BWEB::Walls::createHardWall(wall2, area2, choke2);

        multimap<UnitType, TilePosition> wall3 ={
            { Zerg_Sunken_Colony, TilePosition(114,64) },
            { Zerg_Sunken_Colony, TilePosition(114,66) },
            { Zerg_Sunken_Colony, TilePosition(116,64) },
            { Zerg_Sunken_Colony, TilePosition(116,66) },
            { Zerg_Sunken_Colony, TilePosition(118,64) },
            { Zerg_Sunken_Colony, TilePosition(118,66) },
            { Zerg_Sunken_Colony, TilePosition(120,64) },
            { Zerg_Sunken_Colony, TilePosition(120,66) },
            { Zerg_Sunken_Colony, TilePosition(120,68) },
            { Zerg_Sunken_Colony, TilePosition(122,68) },
            { Zerg_Sunken_Colony, TilePosition(122,70) },
            { Zerg_Sunken_Colony, TilePosition(124,69) },
            { Zerg_Hatchery, TilePosition(122,65) }
        };
        auto choke3 = Util::getClosestChokepoint(Position(TilePosition(122, 65)));
        auto area3 = mapBWEM.GetArea(TilePosition(122, 65));
        BWEB::Walls::createHardWall(wall3, area3, choke3);

        multimap<UnitType, TilePosition> wall4 ={
             { Zerg_Sunken_Colony, TilePosition(118,24) },
             { Zerg_Sunken_Colony, TilePosition(118,26) },
             { Zerg_Sunken_Colony, TilePosition(118,28) },
             { Zerg_Sunken_Colony, TilePosition(118,30) },
             { Zerg_Sunken_Colony, TilePosition(118,32) },
             { Zerg_Sunken_Colony, TilePosition(118,34) },
             { Zerg_Sunken_Colony, TilePosition(120,24) },
             { Zerg_Sunken_Colony, TilePosition(120,26) },
             { Zerg_Sunken_Colony, TilePosition(120,28) },
             { Zerg_Sunken_Colony, TilePosition(120,30) },
             { Zerg_Sunken_Colony, TilePosition(120,32) },
             { Zerg_Sunken_Colony, TilePosition(120,34) },
             { Zerg_Hatchery, TilePosition(122,27) }
        };
        auto choke4 = Util::getClosestChokepoint(Position(TilePosition(112, 27)));
        auto area4 = mapBWEM.GetArea(TilePosition(122, 27));
        BWEB::Walls::createHardWall(wall4, area4, choke4);


        multimap<UnitType, TilePosition> wall5 ={
            { Zerg_Sunken_Colony, TilePosition(38,0) },
            { Zerg_Sunken_Colony, TilePosition(36,0) },
            { Zerg_Sunken_Colony, TilePosition(36,2) },
            { Zerg_Sunken_Colony, TilePosition(34,0) },
            { Zerg_Sunken_Colony, TilePosition(34,2) },
            { Zerg_Sunken_Colony, TilePosition(34,4) },
            { Zerg_Sunken_Colony, TilePosition(32,2) },
            { Zerg_Sunken_Colony, TilePosition(32,4) },
            { Zerg_Sunken_Colony, TilePosition(32,6) },
            { Zerg_Sunken_Colony, TilePosition(30,4) },
            { Zerg_Sunken_Colony, TilePosition(30,6) },
            { Zerg_Sunken_Colony, TilePosition(30,8) },
            { Zerg_Hatchery, TilePosition(28,1) }
        };
        auto choke5 = Util::getClosestChokepoint(Position(TilePosition(28, 1)));
        auto area5 = mapBWEM.GetArea(TilePosition(28, 1));
        BWEB::Walls::createHardWall(wall5, area5, choke5);

        multimap<UnitType, TilePosition> wall6 ={
            { Zerg_Sunken_Colony, TilePosition(22,26) },
            { Zerg_Sunken_Colony, TilePosition(24,25) },
            { Zerg_Sunken_Colony, TilePosition(24,27) },
            { Zerg_Sunken_Colony, TilePosition(26,26) },
            { Zerg_Sunken_Colony, TilePosition(26,28) },
            { Zerg_Sunken_Colony, TilePosition(28,26) },
            { Zerg_Sunken_Colony, TilePosition(28,28) },
            { Zerg_Sunken_Colony, TilePosition(28,30) },
            { Zerg_Sunken_Colony, TilePosition(30,30) },
            { Zerg_Sunken_Colony, TilePosition(31,32) },
            { Zerg_Sunken_Colony, TilePosition(32,30) },
            { Zerg_Sunken_Colony, TilePosition(33,32) },
            { Zerg_Hatchery, TilePosition(30,27) }
        };
        auto choke6 = Util::getClosestChokepoint(Position(TilePosition(30, 27)));
        auto area6 = mapBWEM.GetArea(TilePosition(30, 27));
        BWEB::Walls::createHardWall(wall6, area6, choke6);
    }

    void findWalls()
    {
        if (Terrain::isShitMap())
            return;

        // Create a Zerg/Protoss wall at every natural
        if (Broodwar->self()->getRace() != Races::Terran) {
            openWall = true;
            for (auto &station : BWEB::Stations::getStations()) {
                initializeWallParameters();
                if (!station.isNatural())
                    continue;

                // Create a wall and reduce the building count on each iteration
                while (!BWEB::Walls::getWall(station.getChokepoint())) {
                    BWEB::Walls::createWall(buildings, station.getBase()->GetArea(), station.getChokepoint(), tightType, defenses, openWall, tight);
                    if (Players::PvZ() || buildings.empty())
                        break;

                    UnitType lastBuilding = *buildings.rbegin();
                    buildings.pop_back();
                    if (lastBuilding == Zerg_Hatchery)
                        buildings.push_back(Zerg_Evolution_Chamber);
                }
            }
        }

        // Create a Terran wall at every main
        else {
            for (auto &station : BWEB::Stations::getStations()) {
                if (!station.isMain())
                    continue;
                BWEB::Walls::createWall(buildings, station.getBase()->GetArea(), station.getChokepoint(), tightType, defenses, openWall, tight);
            }
        }
    }

    void onStart()
    {
        initializeWallParameters();
        findWalls();
        findHardWalls();
        findMainWall();
        findNaturalWall();
    }

    void onFrame()
    {
        if (Terrain::isShitMap() && Terrain::getEnemyStartingPosition().isValid()) {
            if (BWEB::Map::getMainTile() == TilePosition(8, 7)) {
                if (Terrain::getEnemyStartingTilePosition() == TilePosition(43, 118))
                    naturalWall = BWEB::Walls::getClosestWall(TilePosition(30, 27));
                else
                    naturalWall = BWEB::Walls::getClosestWall(TilePosition(28, 1));
            }

            if (BWEB::Map::getMainTile() == TilePosition(43, 118)) {
                if (Terrain::getEnemyStartingTilePosition() == TilePosition(8, 7))
                    naturalWall = BWEB::Walls::getClosestWall(TilePosition(35, 112));
                else
                    naturalWall = BWEB::Walls::getClosestWall(TilePosition(50, 98));
            }

            if (BWEB::Map::getMainTile() == TilePosition(117, 51)) {
                if (Terrain::getEnemyStartingTilePosition() == TilePosition(8, 7))
                    naturalWall = BWEB::Walls::getClosestWall(TilePosition(122, 27));
                else
                    naturalWall = BWEB::Walls::getClosestWall(TilePosition(122, 65));
            }
        }
    }

    int calcSaturationRatio(BWEB::Wall& wall, int defensesDesired)
    {
        auto closestNatural = BWEB::Stations::getClosestNaturalStation(TilePosition(wall.getCentroid()));
        auto closestMain = BWEB::Stations::getClosestMainStation(TilePosition(wall.getCentroid()));

        auto saturationRatio = clamp((Stations::getSaturationRatio(closestNatural) + Stations::getSaturationRatio(closestMain)), 0.1, 1.0);
        if (Strategy::enemyRush() || Strategy::enemyPressure() || Util::getTime() < Time(6, 00))
            return defensesDesired;
        return int(ceil(saturationRatio * double(defensesDesired)));
    }

    int calcGroundDefZvP(BWEB::Wall& wall) {

        // Try to see what we expect based on first Zealot push out
        if (Strategy::getEnemyBuild() == "Unknown" && Scouts::enemyDeniedScout() && Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) >= 1 && wall.getGroundDefenseCount() == 0) {
            auto closestZealot = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Self, [&](auto &u) {
                return u.getType() == Protoss_Zealot;
            });
            if (closestZealot && closestZealot->timeArrivesWhen() < Time(3, 50) && BWEB::Map::getGroundDistance(closestZealot->getPosition(), BWEB::Map::getNaturalPosition()) < 640.0)
                return (Util::getTime() > Time(2, 50));
        }

        // See if they expanded or got some tech at a reasonable point for 1 base play
        auto noExpandOrTech = Util::getTime() > Time(5, 00) && !Strategy::enemyFastExpand()
            && Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) == 0
            && Players::getTotalCount(PlayerState::Enemy, Protoss_Dark_Templar) == 0
            && Players::getTotalCount(PlayerState::Enemy, Protoss_High_Templar) == 0
            && Players::getTotalCount(PlayerState::Enemy, Protoss_Reaver) == 0
            && Players::getTotalCount(PlayerState::Enemy, Protoss_Archon) == 0;

        // 4Gate - Upwards of 12 sunkens allowed... Still sane Exile?
        if (Strategy::getEnemyTransition() == "4Gate" && Util::getTime() < Time(12, 00)) {

            if (Strategy::enemyFastExpand()) {
                return 1
                    + (Util::getTime() > Time(4, 00))
                    + (Util::getTime() > Time(4, 30))
                    + (Util::getTime() > Time(5, 00))
                    + (Util::getTime() > Time(5, 20))
                    + (Util::getTime() > Time(5, 40))
                    + (Util::getTime() > Time(6, 00))
                    + (Util::getTime() > Time(6, 20))
                    + (Util::getTime() > Time(6, 40))
                    + (Util::getTime() > Time(7, 20))
                    + (Util::getTime() > Time(8, 20))
                    + (Util::getTime() > Time(9, 20));
            }
            else {
                return 1
                    + (Util::getTime() > Time(4, 00))
                    + (Util::getTime() > Time(4, 30))
                    + (Util::getTime() > Time(5, 00))
                    + (Util::getTime() > Time(5, 20))
                    + (Util::getTime() > Time(5, 40))
                    + (Util::getTime() > Time(6, 00))
                    + (Util::getTime() > Time(6, 20))
                    + (Util::getTime() > Time(6, 40))
                    + (Util::getTime() > Time(7, 00))
                    + (Util::getTime() > Time(8, 00))
                    + (Util::getTime() > Time(9, 00));
            }
        }

        // 1GateCore
        if (Strategy::getEnemyBuild() == "1GateCore" || (Strategy::getEnemyBuild() == "Unknown" && Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) >= 1)) {
            if (Strategy::getEnemyTransition() == "Corsair" && Util::getTime() < Time(8, 30))
                return (Util::getTime() > Time(3, 40)) + (Util::getTime() > Time(7, 30));
            else if (Strategy::getEnemyTransition() == "DT" && Util::getTime() < Time(8, 30))
                return (Util::getTime() > Time(3, 40)) + (Util::getTime() > Time(4, 20)) + (Util::getTime() > Time(5, 00)) + (Util::getTime() > Time(5, 20)) + (Util::getTime() > Time(5, 40));
            else if (Util::getTime() < Time(6, 15) && Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) >= 1)
                return (Util::getTime() > Time(3, 40)) + (Util::getTime() > Time(4, 00)) + (Util::getTime() > Time(4, 20)) + noExpandOrTech;
            else if (Util::getTime() < Time(6, 15))
                return (Util::getTime() > Time(3, 40)) + noExpandOrTech;
        }

        // 2Gate
        if (Strategy::getEnemyBuild() == "2Gate") {

            if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) > 0 || Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) > 0) && Util::getTime() < Time(5, 15))
                return (Util::getTime() > Time(3, 00)) + (Util::getTime() > Time(4, 00)) + (Util::getTime() > Time(4, 20)) + noExpandOrTech;

            else if (Util::getTime() < Time(5, 15) && Strategy::getEnemyOpener() == "10/17")
                return (Util::getTime() > Time(3, 30)) + (Util::getTime() > Time(4, 00)) + (Util::getTime() > Time(4, 20)) + noExpandOrTech;

            else if (Util::getTime() < Time(5, 15) && (Strategy::getEnemyOpener() == "10/12" || Strategy::getEnemyOpener() == "Unknown"))
                return (Util::getTime() > Time(3, 10)) + (Util::getTime() > Time(3, 30)) + (Util::getTime() > Time(4, 00)) + noExpandOrTech;

            else if (Util::getTime() < Time(5, 15) && (Strategy::getEnemyOpener() == "9/9" || Strategy::getEnemyTransition() == "ZealotRush"))
                return (Util::getTime() > Time(2, 50)) + (Util::getTime() > Time(4, 00)) + noExpandOrTech;
        }

        // FFE
        if (Strategy::getEnemyBuild() == "FFE") {
            if (Strategy::getEnemyTransition() == "Carriers")
                return 0;
            if (Strategy::getEnemyTransition() == "5GateGoon" && Util::getTime() < Time(10, 00))
                return ((Util::getTime() > Time(5, 40)) + (Util::getTime() > Time(6, 00)) + (Util::getTime() > Time(6, 20)) + (Util::getTime() > Time(6, 40))) + (Util::getTime() > Time(7, 00)) + (Util::getTime() > Time(8, 00)) + (Util::getTime() > Time(9, 00));
            if (Strategy::getEnemyTransition() == "NeoBisu" && Util::getTime() < Time(6, 30))
                return ((2 * (Util::getTime() > Time(6, 00))));
            if (Strategy::getEnemyTransition() == "Speedlot" && Util::getTime() < Time(7, 00))
                return ((2 * (Util::getTime() > Time(5, 00))) + (2 * (Util::getTime() > Time(5, 30))) + (2 * (Util::getTime() > Time(6, 00))));
            if (Strategy::getEnemyTransition() == "Unknown" && Util::getTime() < Time(5, 15))
                return ((2 * (Util::getTime() > Time(6, 00))) + (Util::getTime() > Time(6, 30)) + (2 * (Util::getTime() > Time(8, 00))));
            if (Util::getTime() < Time(8, 00))
                return ((Util::getTime() > Time(5, 45)) + (Util::getTime() > Time(6, 15)) + (Util::getTime() > Time(6, 45)));
        }

        // Outside of openers, base it off how large the ground army of enemy is
        if (Util::getTime() > Time(8, 00)) {
            const auto divisor = 2.0 + max(0.0, (double(Util::getTime().minutes - 6) / 4.0));
            const auto count = int(double(Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) + Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon)) / divisor);
            return (count);
        }
        return 0;
    }

    int calcGroundDefZvT(BWEB::Wall& wall) {

        // 2Rax
        if (Strategy::getEnemyBuild() == "2Rax") {
            if (Strategy::enemyProxy())
                return 0;
            if (!Strategy::enemyFastExpand() && !Strategy::enemyRush() && Util::getTime() < Time(5, 00))
                return (2 * (Util::getTime() > Time(3, 15))) + (2 * (Util::getTime() > Time(4, 30)));
            if (Strategy::enemyRush())
                return (2 + (Util::getTime() > Time(4, 30)) + (Util::getTime() > Time(5, 30)));
        }

        // RaxCC
        if (Strategy::getEnemyBuild() == "RaxCC") {
            if (Strategy::enemyProxy())
                return 0;
            return (Util::getTime() > Time(4, 30)) + (Util::getTime() > Time(4, 45));
        }

        // RaxFact
        if (Strategy::getEnemyBuild() == "RaxFact") {
            if (Strategy::getEnemyTransition() == "5FacGoliath")
                return 5 * (Util::getTime() > Time(11, 00));
            if (Strategy::getEnemyTransition() == "2Fact" || Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0 || Strategy::enemyWalled())
                return (Util::getTime() > Time(3, 30));
        }

        if (!Strategy::enemyFastExpand() && Strategy::getEnemyBuild() != "RaxCC")
            return ((Util::getTime() > Time(3, 30)) + (Util::getTime() > Time(4, 30)) + (Util::getTime() > Time(5, 00)));
        if (Util::getTime() > Time(10, 00))
            return max(1, (Util::getTime().minutes / 4));
        return (Util::getTime() > Time(3, 30));
    }

    int calcGroundDefZvZ(BWEB::Wall& wall) {
        if ((BuildOrder::getCurrentTransition().find("Muta") != string::npos || Util::getTime() > Time(6, 00)) && (BuildOrder::takeNatural() || int(Stations::getMyStations().size()) >= 2)) {
            if (Players::ZvZ() && BuildOrder::isOpener() && BuildOrder::buildCount(Zerg_Spire) > 0 && vis(Zerg_Spire) == 0)
                return 0;
            else if (Strategy::getEnemyTransition() == "2HatchSpeedling")
                return (Util::getTime() > Time(3, 45)) + (Util::getTime() > Time(4, 00)) + (Util::getTime() > Time(5, 30));
            else if (Util::getTime() < Time(6, 00) && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 40)
                return 6;
            else if (Util::getTime() < Time(6, 00) && (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) >= 3 || Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 26))
                return 4;
            else if (Strategy::enemyPressure())
                return (Util::getTime() > Time(4, 10)) + (vis(Zerg_Sunken_Colony) > 0) + (vis(Zerg_Drone) >= 8 && com(Zerg_Sunken_Colony) >= 2);
            else if (Strategy::enemyRush() && total(Zerg_Zergling) >= 6)
                return 1 + (vis(Zerg_Sunken_Colony) > 0) + (vis(Zerg_Drone) >= 8 && com(Zerg_Sunken_Colony) >= 2);
        }
        return 0;
    }

    int needGroundDefenses(BWEB::Wall& wall)
    {
        auto groundCount = wall.getGroundDefenseCount() /*+ (BuildOrder::getCurrentTransition().find("2Hatch") == string::npos) * ((Util::getTime() > Time(6, 00)) + (Util::getTime() > Time(7, 00)))*/;
        if (!Terrain::isInAllyTerritory(wall.getArea()))
            return 0;

        // Protoss
        if (Broodwar->self()->getRace() == Races::Protoss) {
            auto prepareExpansionDefenses = Util::getTime() < Time(10, 0) && vis(Protoss_Nexus) >= 2 && com(Protoss_Forge) > 0;

            if (Players::vP() && prepareExpansionDefenses && BuildOrder::isWallNat()) {
                auto cannonCount = 2 + int(1 + Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) + Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) - com(Protoss_Zealot) - com(Protoss_Dragoon) - com(Protoss_High_Templar) - com(Protoss_Dark_Templar)) / 2;
                return cannonCount - groundCount;
            }

            if (Players::vZ() && BuildOrder::isWallNat()) {
                auto cannonCount = int(com(Protoss_Forge) > 0)
                    + (Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) >= 6)
                    + (Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) >= 12)
                    + (Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) >= 24)
                    + (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk) / 2);

                // TODO: If scout died, go to 2 cannons, if next scout dies, go 3 cannons   
                if (Strategy::getEnemyTransition() == "2HatchHydra")
                    return 5 - groundCount;
                else if (Strategy::getEnemyTransition() == "3HatchHydra")
                    return 4 - groundCount;
                else if (Strategy::getEnemyTransition() == "2HatchMuta" && Util::getTime() > Time(4, 0))
                    return 3 - groundCount;
                else if (Strategy::getEnemyTransition() == "3HatchMuta" && Util::getTime() > Time(5, 0))
                    return 3 - groundCount;
                else if (Strategy::getEnemyOpener() == "4Pool")
                    return 2 + (Players::getSupply(PlayerState::Self, Races::Protoss) >= 24) - groundCount;
                return cannonCount - groundCount;
            }
        }

        // Terran

        // Zerg
        if (Broodwar->self()->getRace() == Races::Zerg) {
            if (Players::vP())
                return calcSaturationRatio(wall, calcGroundDefZvP(wall)) - groundCount;
            if (Players::vT())
                return calcSaturationRatio(wall, calcGroundDefZvT(wall)) - groundCount;
            if (Players::vZ())
                return calcSaturationRatio(wall, calcGroundDefZvZ(wall)) - groundCount;
        }
        return 0;
    }

    int needAirDefenses(BWEB::Wall& wall)
    {
        auto airCount = wall.getAirDefenseCount();
        const auto enemyAir = Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) > 0
            || Players::getTotalCount(PlayerState::Enemy, Protoss_Scout) > 0
            || Players::getTotalCount(PlayerState::Enemy, Protoss_Stargate) > 0
            || Players::getTotalCount(PlayerState::Enemy, Terran_Wraith) > 0
            || Players::getTotalCount(PlayerState::Enemy, Terran_Valkyrie) > 0
            || Players::getTotalCount(PlayerState::Enemy, Zerg_Mutalisk) > 0
            || (Players::getTotalCount(PlayerState::Enemy, Zerg_Spire) > 0 && Util::getTime() > Time(4, 45));

        if (!Terrain::isInAllyTerritory(wall.getArea()))
            return 0;

        // Protoss
        if (Broodwar->self()->getRace() == Races::Protoss) {
            return 0;
        }

        // Terran

        // Zerg
        if (Broodwar->self()->getRace() == Races::Zerg) {
            if (Players::ZvZ() && Util::getTime() > Time(4, 30) && total(Zerg_Zergling) > Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) && com(Zerg_Spire) == 0 && Strategy::getEnemyTransition() == "Unknown" && BuildOrder::getCurrentTransition() == "2HatchMuta")
                return 1 - airCount;
            if (Players::ZvZ() && Util::getTime() > Time(4, 15) && Strategy::getEnemyTransition() == "1HatchMuta" && BuildOrder::getCurrentTransition() == "2HatchMuta")
                return 1 - airCount;

            if (Players::ZvP() && Util::getTime() > Time(4, 00) && Strategy::getEnemyBuild() == "1GateCore" && Strategy::getEnemyTransition() == "Corsair")
                return 1 - airCount;
            if (Players::ZvP() && Util::getTime() > Time(4, 30) && Strategy::getEnemyBuild() == "2Gate" && Strategy::getEnemyTransition() == "Corsair")
                return 1 - airCount;
        }
        return 0;
    }

    BWEB::Wall* getMainWall() { return mainWall; }
    BWEB::Wall* getNaturalWall() { return naturalWall; }
}