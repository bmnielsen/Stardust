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
        Time enemyExpandTimer = Time(999, 00);

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

        void findNaturalWall()
        {
            naturalWall = BWEB::Walls::getWall(BWEB::Map::getNaturalChoke());
        }

        void findMainWall()
        {
            mainWall = BWEB::Walls::getWall(BWEB::Map::getMainChoke());
        }

        void findWalls()
        {
            initializeWallParameters();

            // Create a wall and reduce the building count on each iteration
            const auto genWall = [&](auto buildings, auto area, auto choke) {
                while (!BWEB::Walls::getWall(choke)) {
                    BWEB::Walls::createWall(buildings, area, choke, tightType, defenses, openWall, tight);
                    if (Players::PvZ() || buildings.empty())
                        break;

                    UnitType lastBuilding = *buildings.rbegin();
                    buildings.pop_back();
                    if (lastBuilding == Zerg_Hatchery)
                        buildings.push_back(Zerg_Evolution_Chamber);
                }
            };

            // Create a Zerg/Protoss wall at every natural
            if (Broodwar->self()->getRace() != Races::Terran) {
                openWall = true;

                // In FFA just make a wall at our natural (if we have one)
                if (Players::vFFA() && Terrain::getMyNatural() && BWEB::Map::getNaturalChoke()) {
                    genWall(buildings, Terrain::getMyNatural()->getBase()->GetArea(), BWEB::Map::getNaturalChoke());
                }
                else {
                    for (auto &station : BWEB::Stations::getStations()) {
                        initializeWallParameters();
                        if (!station.isNatural())
                            continue;
                        genWall(buildings, station.getBase()->GetArea(), station.getChokepoint());
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

        int calcSaturationRatio(BWEB::Wall& wall, int defensesDesired)
        {
            auto closestNatural = BWEB::Stations::getClosestNaturalStation(TilePosition(wall.getCentroid()));
            auto closestMain = BWEB::Stations::getClosestMainStation(TilePosition(wall.getCentroid()));

            auto saturationRatio = clamp((Stations::getSaturationRatio(closestNatural) + Stations::getSaturationRatio(closestMain)), 0.1, 1.0);
            if (Spy::enemyRush() || Spy::enemyPressure() || Util::getTime() < Time(10, 00))
                return defensesDesired;
            return int(ceil(saturationRatio * double(defensesDesired)));
        }

        int calcGroundDefZvP(BWEB::Wall& wall) {

            // Try to see what we expect based on first Zealot push out
            if (Spy::getEnemyBuild() == "Unknown" && Scouts::enemyDeniedScout() && Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) >= 1 && wall.getGroundDefenseCount() == 0) {
                auto closestZealot = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Self, [&](auto &u) {
                    return u->getType() == Protoss_Zealot;
                });
                if (closestZealot && closestZealot->timeArrivesWhen() < Time(3, 50) && BWEB::Map::getGroundDistance(closestZealot->getPosition(), BWEB::Map::getNaturalPosition()) < 640.0)
                    return (Util::getTime() > Time(2, 50));
            }

            if (Spy::enemyFastExpand() && enemyExpandTimer == Time(999, 00))
                enemyExpandTimer = Util::getTime();

            // See if they expanded or got some tech at a reasonable point for 1 base play
            auto noExpand = !Spy::enemyFastExpand();
            auto noTech = Spy::getEnemyTransition() == "Unknown"
                && Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) == 0
                && Players::getTotalCount(PlayerState::Enemy, Protoss_Dark_Templar) == 0
                && Players::getTotalCount(PlayerState::Enemy, Protoss_High_Templar) == 0
                && Players::getTotalCount(PlayerState::Enemy, Protoss_Reaver) == 0
                && Players::getTotalCount(PlayerState::Enemy, Protoss_Archon) == 0;
            auto noExpandOrTech = noExpand && noTech;

            // 1 base transitions
            if (Spy::getEnemyBuild() == "2Gate" || Spy::getEnemyBuild() == "1GateCore") {

                // 4Gate
                if (Spy::getEnemyTransition() == "4Gate" && Util::getTime() < Time(12, 00)) {
                    return 1
                        + (Util::getTime() > Time(4, 10))
                        + (enemyExpandTimer > Time(4, 30) && Util::getTime() > Time(4, 40))
                        + (enemyExpandTimer > Time(5, 00) && Util::getTime() > Time(5, 10))
                        + (enemyExpandTimer > Time(5, 30) && Util::getTime() > Time(5, 40))
                        + (enemyExpandTimer > Time(6, 00) && Util::getTime() > Time(6, 10))
                        + (enemyExpandTimer > Time(6, 30) && Util::getTime() > Time(6, 40))
                        + (enemyExpandTimer > Time(7, 00) && Util::getTime() > Time(7, 10))
                        + (enemyExpandTimer > Time(7, 30) && Util::getTime() > Time(7, 40));
                }

                // DT
                if (Util::getTime() < Time(8, 30) && Spy::getEnemyTransition() == "DT") {
                    return 1
                        + (enemyExpandTimer > Time(4, 05) && Util::getTime() > Time(4, 15))
                        + (enemyExpandTimer > Time(4, 50) && Util::getTime() > Time(5, 00))
                        + (enemyExpandTimer > Time(5, 10) && Util::getTime() > Time(5, 20))
                        + (enemyExpandTimer > Time(5, 30) && Util::getTime() > Time(5, 40));
                }

                // Corsair
                if (Util::getTime() < Time(8, 30) && Spy::getEnemyTransition() == "Corsair") {
                    return 1
                        + (enemyExpandTimer > Time(4, 20) && Util::getTime() > Time(4, 30))
                        + (enemyExpandTimer > Time(6, 50) && Util::getTime() > Time(7, 00));
                }

                // Speedlot
                if (Util::getTime() < Time(8, 30) && (Spy::getEnemyTransition() == "Speedlot" || Spy::getEnemyTransition() == "ZealotRush")) {
                    return 1
                        + (enemyExpandTimer > Time(3, 00) && Util::getTime() > Time(3, 10))
                        + (enemyExpandTimer > Time(3, 50) && Util::getTime() > Time(4, 00))
                        + (enemyExpandTimer > Time(4, 20) && Util::getTime() > Time(4, 30))
                        + (enemyExpandTimer > Time(5, 10) && Util::getTime() > Time(5, 20));
                }

                if (Util::getTime() > Time(7, 00) && Spy::enemyFastExpand() && Spy::getEnemyTransition() == "Unknown") {
                    return 1
                        + (noTech && Util::getTime() > Time(7, 00))
                        + (noTech && Util::getTime() > Time(7, 30))
                        + (noTech && Util::getTime() > Time(8, 00))
                        + (noTech && Util::getTime() > Time(8, 30));
                }
            }

            // 1GateCore
            if (Spy::getEnemyBuild() == "1GateCore" || (Spy::getEnemyBuild() == "Unknown" && Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) >= 1)) {
                return (Util::getTime() > Time(3, 45))
                    + (noTech && Util::getTime() > Time(4, 30))
                    + (noExpandOrTech && Util::getTime() > Time(5, 20))
                    + (noExpandOrTech && Util::getTime() > Time(5, 40))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 40));
            }

            // 2Gate
            if (Spy::getEnemyBuild() == "2Gate" && Util::getTime() < Time(7, 30) && !Spy::enemyProxy()) {
                if (Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) > 0)
                    return (Util::getTime() > Time(3, 45))
                    + (noTech && Util::getTime() > Time(4, 10))
                    + (noExpandOrTech && Util::getTime() > Time(5, 20))
                    + (noExpandOrTech && Util::getTime() > Time(5, 40))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 30));
                else if (Spy::getEnemyOpener() == "10/17")
                    return (Util::getTime() > Time(3, 45))
                    + (noTech && Util::getTime() > Time(4, 30))
                    + (noExpandOrTech && Util::getTime() > Time(5, 20))
                    + (noExpandOrTech && Util::getTime() > Time(5, 40))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 30));
                else if (Spy::getEnemyOpener() == "10/12" || Spy::getEnemyOpener() == "Unknown")
                    return (Util::getTime() > Time(3, 00))
                    + (noTech && Util::getTime() > Time(4, 15))
                    + (noExpandOrTech && Util::getTime() > Time(5, 20))
                    + (noExpandOrTech && Util::getTime() > Time(5, 40))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 30));
                else if (Spy::getEnemyOpener() == "9/9")
                    return 1
                    + (noTech && Util::getTime() > Time(4, 00))
                    + (noExpandOrTech && Util::getTime() > Time(5, 20))
                    + (noExpandOrTech && Util::getTime() > Time(5, 40))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 00))
                    + (noExpandOrTech && Util::getTime() > Time(6, 30));
            }

            // FFE
            if (Spy::getEnemyBuild() == "FFE") {

                if (BuildOrder::getCurrentTransition() == "6HatchHydra")
                    return 2 * (Util::getTime() > Time(5, 45));

                if (Spy::getEnemyTransition() == "Carriers")
                    return 0;
                if (Spy::getEnemyTransition() == "5GateGoon" && Util::getTime() < Time(10, 00))
                    return (Util::getTime() > Time(5, 40))
                    + (Util::getTime() > Time(6, 00))
                    + (Util::getTime() > Time(6, 20))
                    + (Util::getTime() > Time(6, 40))
                    + (Util::getTime() > Time(7, 00))
                    + (Util::getTime() > Time(8, 00))
                    + (Util::getTime() > Time(9, 00));
                if (Spy::getEnemyTransition() == "CorsairGoon" && Util::getTime() < Time(10, 00))
                    return ((Util::getTime() > Time(5, 30))
                        + (Util::getTime() > Time(6, 15))
                        + (Util::getTime() > Time(6, 45))
                        + 2 * (Util::getTime() > Time(7, 15)));
                if (Spy::getEnemyTransition() == "NeoBisu" && Util::getTime() < Time(6, 30))
                    return ((2 * (Util::getTime() > Time(6, 00))));
                if (Spy::getEnemyTransition() == "Speedlot" && Util::getTime() < Time(7, 00))
                    return ((2 * (Util::getTime() > Time(6, 00))) + (2 * (Util::getTime() > Time(6, 30))) + (2 * (Util::getTime() > Time(7, 00))));
                if (Spy::getEnemyTransition() == "Unknown" && Util::getTime() < Time(5, 15))
                    return ((2 * (Util::getTime() > Time(6, 00))) + (Util::getTime() > Time(6, 30)) + (2 * (Util::getTime() > Time(8, 00))));
                if (Util::getTime() < Time(8, 00))
                    return ((Util::getTime() > Time(5, 30)) + (Util::getTime() > Time(6, 15)) + (Util::getTime() > Time(6, 45)));
            }
            return (Util::getTime() > Time(3, 45));
        }

        int calcGroundDefZvT(BWEB::Wall& wall)
        {
            if (Util::getTime() > Time(10, 00))
                return max(1, (Util::getTime().minutes / 4));

            // 2Rax
            if (Spy::getEnemyBuild() == "2Rax") {
                if (Spy::enemyProxy())
                    return 0;
                if (!Spy::enemyFastExpand() && !Spy::enemyRush() && Util::getTime() < Time(5, 00))
                    return (2 * (Util::getTime() > Time(3, 15))) + (2 * (Util::getTime() > Time(4, 30)));
                if (Spy::enemyRush())
                    return (2 + (Util::getTime() > Time(4, 30)) + (Util::getTime() > Time(5, 30)));
            }

            // RaxCC
            if (Spy::getEnemyBuild() == "RaxCC") {
                if (Spy::getEnemyOpener() == "8Rax")
                    return 1;
                if (Spy::enemyProxy())
                    return 0;
                return (Util::getTime() > Time(4, 30)) + (Util::getTime() > Time(4, 45));
            }

            // RaxFact
            if (Spy::getEnemyBuild() == "RaxFact") {
                if (Spy::getEnemyTransition() == "5FacGoliath")
                    return 5 * (Util::getTime() > Time(11, 00));
                if (Spy::getEnemyTransition() == "2Fact" || Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0 || Spy::enemyWalled())
                    return (Util::getTime() > Time(3, 30));
            }
            return (Util::getTime() > Time(3, 00)) + (Util::getTime() > Time(5, 00));
        }

        int calcGroundDefZvZ(BWEB::Wall& wall) {
            return 0;
        }

        int calcGroundDefZvFFA(BWEB::Wall& wall) {
            return 1
                + (Util::getTime() > Time(5, 20))
                + (Util::getTime() > Time(5, 40));
        }
    }

    void onStart()
    {
        initializeWallParameters();
        findWalls();
        findMainWall();
        findNaturalWall();
    }

    void onFrame()
    {

    }

    int needGroundDefenses(BWEB::Wall& wall)
    {
        auto groundCount = wall.getGroundDefenseCount();
        if (!Terrain::inTerritory(PlayerState::Self, wall.getArea()))
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
                if (Spy::getEnemyTransition() == "2HatchHydra")
                    return 5 - groundCount;
                else if (Spy::getEnemyTransition() == "3HatchHydra")
                    return 4 - groundCount;
                else if (Spy::getEnemyTransition() == "2HatchMuta" && Util::getTime() > Time(4, 0))
                    return 3 - groundCount;
                else if (Spy::getEnemyTransition() == "3HatchMuta" && Util::getTime() > Time(5, 0))
                    return 3 - groundCount;
                else if (Spy::getEnemyOpener() == "4Pool")
                    return 2 + (Players::getSupply(PlayerState::Self, Races::Protoss) >= 24) - groundCount;
                return cannonCount - groundCount;
            }
        }

        // Terran

        // Zerg
        if (Broodwar->self()->getRace() == Races::Zerg) {
            if (Players::ZvP())
                return calcSaturationRatio(wall, calcGroundDefZvP(wall)) - groundCount;
            if (Players::ZvT())
                return calcSaturationRatio(wall, calcGroundDefZvT(wall)) - groundCount;
            if (Players::ZvZ())
                return calcSaturationRatio(wall, calcGroundDefZvZ(wall)) - groundCount;
            if (Players::ZvFFA())
                return calcSaturationRatio(wall, calcGroundDefZvFFA(wall)) - groundCount;
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

        if (!Terrain::inTerritory(PlayerState::Self, wall.getArea()))
            return 0;

        // Protoss
        if (Broodwar->self()->getRace() == Races::Protoss) {
            return 0;
        }

        // Terran

        // Zerg
        if (Broodwar->self()->getRace() == Races::Zerg) {
            if (Players::ZvZ() && Util::getTime() > Time(4, 30) && total(Zerg_Zergling) > Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) && com(Zerg_Spire) == 0 && Spy::getEnemyTransition() == "Unknown" && BuildOrder::getCurrentTransition() == "2HatchMuta")
                return 1 - airCount;
            if (Players::ZvZ() && Util::getTime() > Time(4, 15) && Spy::getEnemyTransition() == "1HatchMuta" && BuildOrder::getCurrentTransition() == "2HatchMuta")
                return 1 - airCount;
        }
        return 0;
    }

    BWEB::Wall* getMainWall() { return mainWall; }
    BWEB::Wall* getNaturalWall() { return naturalWall; }
}