#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave {

}

namespace McRave::Goals {

    namespace {

        void assignNumberToGoal(Position here, UnitType type, int count, GoalType gType = GoalType::None)
        {
            if (!here.isValid())
                return;

            for (int current = 0; current < count; current++) {

                if (type.isFlyer()) {
                    const auto closest = Util::getClosestUnit(here, PlayerState::Self, [&](auto &u) {
                        if (gType == GoalType::Attack && u.globalRetreat())
                            return false;
                        return u.getType() == type && !u.getGoal().isValid();
                    });

                    if (closest) {
                        closest->setGoal(here);
                        closest->setGoalType(gType);
                    }
                }

                else {
                    const auto closest = Util::getClosestUnitGround(here, PlayerState::Self, [&](auto &u) {
                        if (gType == GoalType::Attack && u.globalRetreat())
                            return false;
                        return u.getType() == type && !u.getGoal().isValid();
                    });

                    if (closest) {
                        closest->setGoal(here);
                        closest->setGoalType(gType);
                    }
                }
            }
        }

        void assignNumberToGoal(WalkPosition here, UnitType type, int count, GoalType gType = GoalType::None)
        {
            assignNumberToGoal(Position(here) + Position(4, 4), type, count, gType);
        }

        void assignNumberToGoal(TilePosition here, UnitType type, int count, GoalType gType = GoalType::None)
        {
            assignNumberToGoal(Position(here) + Position(16, 16), type, count, gType);
        }

        void assignPercentToGoal(Position here, UnitType type, double percent, GoalType gType = GoalType::None)
        {
            int count = int(percent * double(vis(type)));
            assignNumberToGoal(here, type, count, gType);
        }

        void assignPercentToGoal(WalkPosition here, UnitType type, double percent, GoalType gType = GoalType::None)
        {
            assignPercentToGoal(Position(here) + Position(4, 4), type, percent, gType);
        }

        void assignPercentToGoal(TilePosition here, UnitType type, double percent, GoalType gType = GoalType::None)
        {
            assignPercentToGoal(Position(here) + Position(16, 16), type, percent, gType);
        }

        // HACK: Need a pred function integrated above 
        void assignWorker(Position here) {
            auto worker = Util::getClosestUnitGround(here, PlayerState::Self, [&](auto &u) {
                return Workers::canAssignToBuild(u) || u.getPosition().getDistance(here) < 64.0;
            });
            if (worker)
                worker->setGoal(here);
        }

        void clearGoals()
        {
            for (auto &unit : Units::getUnits(PlayerState::Self)) {
                unit->setGoal(Positions::Invalid);
                unit->setGoalType(GoalType::None);
            }
        }

        void updateGenericGoals()
        {
            auto rangedType = UnitTypes::None;
            auto meleeType = UnitTypes::None;
            auto airType = UnitTypes::None;
            auto detector = UnitTypes::None;
            auto base = UnitTypes::None;

            if (Broodwar->self()->getRace() == Races::Protoss) {
                rangedType = Protoss_Dragoon;
                meleeType = Protoss_Zealot;
                airType = Protoss_Corsair;
                detector = Protoss_Observer;
                base = Protoss_Nexus;
            }
            if (Broodwar->self()->getRace() == Races::Zerg) {
                rangedType = Zerg_Hydralisk;
                meleeType = Zerg_Zergling;
                airType = Zerg_Mutalisk;
                detector = Zerg_Overlord;
                base = Zerg_Hatchery;
            }
            if (Broodwar->self()->getRace() == Races::Terran) {
                rangedType = Terran_Vulture;
                meleeType = Terran_Firebat;
                airType = Terran_Wraith;
                detector = Terran_Science_Vessel;
                base = Terran_Command_Center;
            }

            // Defend my expansions
            if (Stations::getMyStations().size() >= 3) {
                for (auto &station : Stations::getMyStations()) {

                    if (Stations::getGroundDefenseCount(station) >= 2 || Stations::needGroundDefenses(station) == 0 || (Broodwar->self()->getRace() == Races::Protoss && Stations::needPower(station)))
                        continue;

                    // If it's a main, defend at the natural
                    if (station->isMain()) {
                        auto closestNatural = BWEB::Stations::getClosestNaturalStation(station->getBase()->Location());
                        if (closestNatural && closestNatural->getBase()->Center()) {
                            auto closestWall = BWEB::Walls::getClosestWall(closestNatural->getBase()->Location());
                            if (closestWall && closestWall->getGroundDefenseCount() >= 2)
                                continue;

                            auto naturalDefendPosition = Position(closestNatural->getBase()->Center());
                            assignPercentToGoal(naturalDefendPosition, rangedType, 0.25, GoalType::None);
                            continue;
                        }
                    }

                    // Otherwise defend at this base
                    auto defendPosition = Position(station->getBase()->Center());
                    assignPercentToGoal(defendPosition, rangedType, 0.25, GoalType::None);
                }
            }

            // Send a worker early when we want to
            if (BuildOrder::isPlanEarly()) {
                if (int(Stations::getMyStations().size() < 2))
                    assignWorker(Terrain::getMyNatural()->getBase()->Center());
                else if (Planning::getCurrentExpansion())
                    assignWorker(Planning::getCurrentExpansion()->getBase()->Center());
            }

            // Send detector to next expansion
            if (Planning::getCurrentExpansion()) {
                auto nextExpand = Planning::getCurrentExpansion()->getBase()->Center();
                auto needDetector = Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0 || Players::getTotalCount(PlayerState::Enemy, Protoss_Dark_Templar) > 0 || Players::vZ();
                if (nextExpand.isValid() && needDetector && BWEB::Map::isUsed(Planning::getCurrentExpansion()->getBase()->Location()) == UnitTypes::None) {
                    if (Stations::getMyStations().size() >= 2 && BuildOrder::buildCount(base) > vis(base))
                        assignNumberToGoal(nextExpand, detector, 1);
                }

                // Escort expanders
                if (nextExpand.isValid() && Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) >= 2) {
                    auto closestBuilder = Util::getClosestUnit(nextExpand, PlayerState::Self, [&](auto &u) {
                        return u.getBuildType().isResourceDepot();
                    });
                    auto type = (vis(airType) > 0 && Broodwar->self()->getRace() == Races::Zerg) ? airType : rangedType;

                    if (closestBuilder) {
                        for (auto &t : closestBuilder->getTargetedBy()) {
                            if (auto targeter = t.lock())
                                assignNumberToGoal(targeter->getPosition(), type, 1, GoalType::Escort);                            
                        }
                    }
                }
            }

            // Aggresively deny enemy expansions
            if (Terrain::getEnemyStartingPosition().isValid() && Util::getTime() > Time(5, 00) && !BuildOrder::isRush() && !Players::ZvZ()) {
                multimap<double, BWEB::Station> stationsByDistance;

                for (auto &station : BWEB::Stations::getStations()) {

                    auto closestSelfStation = Stations::getClosestStationGround(PlayerState::Self, station.getBase()->Center());
                    auto closestEnemyStation = Stations::getClosestStationGround(PlayerState::Enemy, station.getBase()->Center());

                    if (!closestSelfStation || !closestEnemyStation)
                        continue;

                    if (Terrain::isInEnemyTerritory(station.getBase()->GetArea()))
                        continue;
                    if (station == Terrain::getEnemyNatural() || station == Terrain::getEnemyMain())
                        continue;
                    if (station == Terrain::getMyNatural() || station == Terrain::getMyMain() || (Planning::getCurrentExpansion() && station == Planning::getCurrentExpansion()))
                        continue;
                    if (Stations::ownedBy(&station) == PlayerState::Self || Stations::ownedBy(&station) == PlayerState::Enemy)
                        continue;
                    if (station.getBase()->Center().getDistance(closestSelfStation->getBase()->Center()) < station.getBase()->Center().getDistance(closestEnemyStation->getBase()->Center())
                        && BWEB::Map::getGroundDistance(station.getBase()->Center(), closestSelfStation->getBase()->Center()) < BWEB::Map::getGroundDistance(station.getBase()->Center(), closestEnemyStation->getBase()->Center()))
                        continue;

                    // Create a path and check if areas are already enemy owned
                    auto badStation = false;
                    auto path = mapBWEM.GetPath(station.getBase()->Center(), BWEB::Map::getMainPosition());
                    for (auto &choke : path) {
                        if (Terrain::isInEnemyTerritory(choke->GetAreas().first) || Terrain::isInEnemyTerritory(choke->GetAreas().second))
                            badStation = true;
                    }
                    if (badStation)
                        continue;

                    auto dist = station.getBase()->Center().getDistance(Terrain::getEnemyStartingPosition()) * (station.getBase()->Geysers().empty() ? 4.0 : 1.0);
                    stationsByDistance.emplace(make_pair(dist, station));
                }

                // Zerg deny every base
                if (Broodwar->self()->getRace() == Races::Zerg && Util::getTime() > Time(8, 00)) {
                    for (auto &[dist, station] : stationsByDistance) {
                        auto type = meleeType;

                        if (!Strategy::enemyGreedy())
                            assignNumberToGoal(station.getBase()->Center(), type, 1, GoalType::Contain);
                    }
                }

                // Protoss denies closest base
                if (Players::PvT()) {
                    int i = 0;
                    for (auto &station : stationsByDistance) {
                        auto type = (Players::PvT() || Players::PvP()) ? rangedType : meleeType;
                        assignNumberToGoal(station.second.getBase()->Center(), type, 2);
                        i++;
                        if (i > 1)
                            break;
                    }
                }
            }

        }

        void updateProtossGoals()
        {
            if (Broodwar->self()->getRace() != Races::Protoss)
                return;

            map<UnitType, int> unitTypes;
            auto enemyStrength = Players::getStrength(PlayerState::Enemy);
            auto myRace = Broodwar->self()->getRace();

            // Attack enemy expansions with a small force
            if (Players::vP() || Players::vT()) {
                auto distBest = 0.0;
                auto posBest = Positions::Invalid;
                for (auto &station : Stations::getEnemyStations()) {
                    auto pos = station->getBase()->Center();
                    auto dist = BWEB::Map::getGroundDistance(pos, Terrain::getEnemyStartingPosition());
                    if (dist > distBest) {
                        distBest = dist;
                        posBest = pos;
                    }
                }

                if (Players::vP())
                    assignPercentToGoal(posBest, Protoss_Dragoon, 0.15);
                else
                    assignPercentToGoal(posBest, Protoss_Zealot, 0.15);
            }

            // Send a DT / Zealot + Goon squad to enemys furthest station
            if (Stations::getMyStations().size() >= 3) {
                auto distBest = 0.0;
                auto posBest = Positions::Invalid;
                for (auto &station : Stations::getEnemyStations()) {
                    auto pos = station->getBase()->Center();
                    auto dist = pos.getDistance(Terrain::getEnemyStartingPosition());

                    if (dist > distBest) {
                        posBest = pos;
                        distBest = dist;
                    }
                }

                if (Actions::overlapsDetection(nullptr, posBest, PlayerState::Enemy) || vis(Protoss_Dark_Templar) == 0) {
                    if (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) > 0)
                        assignNumberToGoal(posBest, Protoss_Zealot, 4);
                    else
                        assignNumberToGoal(posBest, Protoss_Dragoon, 4);
                }
                else
                    assignNumberToGoal(posBest, Protoss_Dark_Templar, vis(Protoss_Dark_Templar));
            }

            // Escort shuttles
            if (Players::vZ() && Players::getStrength(PlayerState::Enemy).airToAir > 0.0) {
                for (auto &u : Units::getUnits(PlayerState::Self)) {
                    UnitInfo &unit = *u;
                    if (unit.getRole() == Role::Transport)
                        assignPercentToGoal(unit.getPosition(), Protoss_Corsair, 0.25);
                }
            }
        }

        void updateTerranGoals()
        {

        }

        void updateZergGoals()
        {
            if (Broodwar->self()->getRace() != Races::Zerg)
                return;

            auto enemyStrength = Players::getStrength(PlayerState::Enemy);

            // Clear out base early game
            if (Util::getTime() < Time(5, 00) && !Strategy::enemyRush() && !Strategy::enemyPressure() && !Players::ZvZ()) {
                auto oldestTile = Terrain::getOldestPosition(BWEB::Map::getMainArea());

                if (oldestTile.isValid())
                    assignNumberToGoal(oldestTile, Zerg_Zergling, 1, GoalType::Explore);
            }

            // Assign an Overlord to watch our Choke early on
            if ((Util::getTime() < Time(3, 00) && !Strategy::enemyProxy()) || (Util::getTime() < Time(2, 15) && Strategy::enemyProxy()) || (Players::ZvZ() && enemyStrength.airToAir <= 0.0))
                assignNumberToGoal(Position(BWEB::Map::getNaturalChoke()->Center()), Zerg_Overlord, 1, GoalType::Escort);

            // Assign the first available Overlord to each natural
            for (auto &station : Stations::getMyStations()) {
                if (station->isNatural())
                    assignNumberToGoal(station->getBase()->Center(), Zerg_Overlord, 1, GoalType::Escort);
            }

            // Assign an Overlord to each Station
            for (auto &station : Stations::getMyStations()) {
                auto closestSunk = Util::getClosestUnit(mapBWEM.Center(), PlayerState::Self, [&](auto &u) {
                    return u.getType() == Zerg_Sunken_Colony && find(station->getDefenses().begin(), station->getDefenses().end(), u.getTilePosition()) != station->getDefenses().end();
                });
                auto closestSpore = Util::getClosestUnit(mapBWEM.Center(), PlayerState::Self, [&](auto &u) {
                    return u.getType() == Zerg_Spore_Colony && find(station->getDefenses().begin(), station->getDefenses().end(), u.getTilePosition()) != station->getDefenses().end();
                });

                if (closestSpore)
                    assignNumberToGoal(closestSpore->getPosition(), Zerg_Overlord, 1, GoalType::Escort);
                else if (closestSunk)
                    assignNumberToGoal(closestSunk->getPosition(), Zerg_Overlord, 1, GoalType::Escort);
                else
                    assignNumberToGoal(station->getBase()->Center(), Zerg_Overlord, 1, GoalType::Escort);
            }

            // Assign an Overlord to each Wall
            if (!Players::vT()) {
                for (auto &[_, wall] : BWEB::Walls::getWalls()) {
                    if (!Terrain::isInAllyTerritory(wall.getArea()))
                        continue;

                    auto closestSunk = Util::getClosestUnit(mapBWEM.Center(), PlayerState::Self, [&](auto &u) {
                        return u.getType() == Zerg_Sunken_Colony && wall.getDefenses().find(u.getTilePosition()) != wall.getDefenses().end();
                    });

                    if (closestSunk)
                        assignNumberToGoal(closestSunk->getPosition(), Zerg_Overlord, 1, GoalType::Escort);
                }
            }

            // Assign an Overlord to each main choke
            for (auto &station : Stations::getMyStations()) {
                if (station->isMain() && station->getChokepoint())
                    assignNumberToGoal(station->getChokepoint()->Center(), Zerg_Overlord, 1, GoalType::Escort);
            }

            // Attack enemy expansions with a small force         
            if (Util::getTime() > Time(6, 00) || Strategy::enemyProxy()) {
                auto distBest = 0.0;
                auto posBest = Positions::Invalid;
                for (auto &station : Stations::getEnemyStations()) {

                    if (!Strategy::enemyProxy()) {
                        if (station == Terrain::getEnemyNatural()
                            || station == Terrain::getEnemyMain())
                            continue;
                    }

                    auto pos = station->getBase()->Center();
                    auto dist = BWEB::Map::getGroundDistance(pos, Terrain::getEnemyStartingPosition());
                    if (dist > distBest) {
                        distBest = dist;
                        posBest = pos;
                    }
                }
                if (posBest.isValid()) {
                    assignPercentToGoal(posBest, Zerg_Zergling, 1.00, GoalType::Attack);
                    if (Players::getPlayerInfo(Broodwar->self())->hasUpgrade(UpgradeTypes::Grooved_Spines) && Players::getPlayerInfo(Broodwar->self())->hasUpgrade(UpgradeTypes::Muscular_Augments))
                        assignPercentToGoal(posBest, Zerg_Hydralisk, 0.50, GoalType::Attack);
                }
            }

            // Assign Scourge to watch for drops coming in
            if (Players::ZvT() && Util::getTime() > Time(10, 00)) {
                vector<Position> validSecondaryNodes;
                Position firstNode = Positions::Invalid;
                Position secondNode = Positions::Invalid;

                auto playableWidth = Broodwar->mapWidth() * 32 - 320;
                auto playableHeight = Broodwar->mapHeight() * 32 - 320;
                auto widthIncrement = playableWidth / 10;
                auto heightIncrement = playableHeight / 10;
                vector<Position> nodes;

                for (int x = 160; x < Broodwar->mapWidth() * 32 - 160; x+=widthIncrement) {
                    nodes.push_back(Position(x, 160));
                    nodes.push_back(Position(x, Broodwar->mapHeight() * 32 - 160));
                }
                for (int y = 160; y < Broodwar->mapHeight() * 32 - 160; y+=heightIncrement) {
                    nodes.push_back(Position(160, y));
                    nodes.push_back(Position(Broodwar->mapWidth() * 32 - 160, y));
                }
                sort(nodes.begin(), nodes.end(), [&](auto &l, auto &r) { return l.getDistance(Terrain::getEnemyStartingPosition()) < r.getDistance(Terrain::getEnemyStartingPosition()); });

                int i = 0;
                for (auto &pos : nodes) {

                    auto closestEnemyStation = Stations::getClosestStationAir(PlayerState::Enemy, pos);
                    auto closestSelfStation = Stations::getClosestStationAir(PlayerState::Self, pos);

                    if (!closestEnemyStation
                        || !closestSelfStation
                        || pos.getDistance(closestEnemyStation->getBase()->Center()) < pos.getDistance(closestSelfStation->getBase()->Center())
                        || Terrain::isInEnemyTerritory(TilePosition(pos)))
                        continue;

                    validSecondaryNodes.push_back(pos);
                    //Broodwar->drawTextMap(pos, "%d", i);
                    //Broodwar->drawCircleMap(pos, 8, Colors::Yellow);
                    i++;
                }

                // First node is automatically the closest to the enemy
                if (!validSecondaryNodes.empty()) {
                    firstNode = validSecondaryNodes.front();
                }

                // Second node is furthest from first node
                if (!validSecondaryNodes.empty()) {
                    sort(validSecondaryNodes.begin(), validSecondaryNodes.end(), [&](auto &l, auto &r) { return l.getDistance(firstNode) > r.getDistance(firstNode); });
                    secondNode = validSecondaryNodes.front();
                }

                if (firstNode.isValid())
                    assignNumberToGoal(firstNode, Zerg_Scourge, 2, GoalType::Contain);
                if (secondNode.isValid())
                    assignNumberToGoal(secondNode, Zerg_Scourge, 2, GoalType::Contain);
            }

            // Send a Zergling to a low energy Defiler
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;
                if (unit.getType() == Zerg_Defiler && unit.getEnergy() < 150)
                    assignNumberToGoal(unit.getTilePosition(), Zerg_Zergling, 1, GoalType::Attack);
            }
        }

        void updateCampaignGoals()
        {
            // Future stuff
            if (Broodwar->getGameType() == GameTypes::Use_Map_Settings && !Broodwar->isMultiplayer()) {

            }
        }
    }

    void onFrame()
    {
        clearGoals();
        updateGenericGoals();
        updateProtossGoals();
        updateTerranGoals();
        updateZergGoals();
        updateCampaignGoals();
    }
}