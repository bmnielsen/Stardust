#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Scouts {

    namespace {

        enum class ScoutType {
            None, Main, Natural, Proxy, Safe
        };

        struct ScoutTarget {
            Position center = Positions::Invalid;
            ScoutType type = ScoutType::None;
            double dist = 0.0;
            bool assigned = false;
            vector<Position> positions;

            ScoutTarget(Position _center, double _dist, ScoutType _type, vector<Position> _positions ={}) {
                dist = _dist, center = _center, type = _type, positions = _positions;
                if (positions.empty())
                    positions.push_back(center);
            }

            bool operator< (const ScoutTarget& r) const {
                return dist < r.dist;
            }
        };

        set<Position> unexploredMains, unexploredNaturals;
        vector<ScoutTarget> scoutTargets;
        map<UnitType, int> currentScoutTypeCounts;
        map<UnitType, int> desiredScoutTypeCounts;
        int scoutDeadFrame = -5000;
        bool fullScout = false;
        bool sacrifice = false;
        bool workerScoutDenied = false;
        bool onlyNaturalScout = false;
        bool firstOverlord = false;
        UnitType proxyType = None;
        Position proxyPosition = Positions::Invalid;
        vector<BWEB::Station*> scoutOrder, scoutOrderFirstOverlord;
        map<BWEB::Station *, Position> expansionWatchPositions;

        bool scoutAllowedHere(ScoutTarget target, UnitInfo& unit) {
            if (unit.getType().isWorker()) {
                if (target.type == ScoutType::Safe)
                    return false;
            }
            else {
                if (onlyNaturalScout && target.type != ScoutType::Safe)
                    return false;
                if (target.type == ScoutType::Proxy)
                    return false;
            }
            return true;
        }

        void checkScoutDenied()
        {
            if (workerScoutDenied || Util::getTime() < Time(3, 30))
                return;

            auto closestWorker = Util::getFurthestUnit(BWEB::Map::getMainPosition(), PlayerState::Self, [&](auto &u) {
                return u.getType().isWorker() && u.getRole() == Role::Scout;
            });
            if (closestWorker) {
                auto closestMain = BWEB::Stations::getClosestMainStation(closestWorker->getTilePosition());
                auto closeToChoke = closestMain && closestMain->getChokepoint() && closestWorker->getPosition().getDistance(Position(closestMain->getChokepoint()->Center())) < 160.0;
                auto deniedFromMain = closestMain && mapBWEM.GetArea(closestWorker->getTilePosition()) != closestMain->getBase()->GetArea() && Grids::getEGroundThreat(closestWorker->getPosition()) > 0.0f;
                if (closeToChoke && deniedFromMain) {
                    easyWrite("Worker scout denied at " + Util::getTime().toString());
                    workerScoutDenied = true;
                }
            }
        }

        void updateScountCount()
        {
            // Set how many of each UnitType are scouts
            desiredScoutTypeCounts.clear();
            currentScoutTypeCounts.clear();
            unexploredMains.clear();
            unexploredNaturals.clear();
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;
                if (unit.getRole() == Role::Scout)
                    currentScoutTypeCounts[unit.getType()]++;
            }

            // Calculate the number of unexplored bases
            for (auto &station : BWEB::Stations::getStations()) {
                Position center = Position(station.getBase()->Location()) + Position(64, 48);

                if (!station.isMain() && !station.isNatural())
                    continue;

                auto &list = station.isMain() ? unexploredMains : unexploredNaturals;
                if (!Stations::isBaseExplored(&station))
                    list.insert(station.getBase()->Center());
            }

            // Protoss
            if (Broodwar->self()->getRace() == Races::Protoss) {
                desiredScoutTypeCounts[Protoss_Probe] = int(BuildOrder::shouldScout())
                    + int(Players::PvZ() && !Terrain::getEnemyStartingPosition().isValid() && mapBWEM.StartingLocations().size() == 4 && unexploredMains.size() == 2)
                    + int(Players::PvP() && (Strategy::enemyProxy() || Strategy::enemyPossibleProxy()) && com(Protoss_Zealot) < 1);

                // No probe scouting when encountering the following situations
                if ((Players::PvZ() && Strategy::enemyRush() && Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) >= 2)
                    || (Players::PvT() && (Strategy::enemyPressure() || Strategy::enemyWalled()))
                    || (BuildOrder::isPlayPassive() && Strategy::enemyPressure())
                    || (Util::getTime() > Time(5, 00)))
                    desiredScoutTypeCounts[Protoss_Probe] = 0;
            }

            // Terran
            if (Broodwar->self()->getRace() == Races::Terran) {
                desiredScoutTypeCounts[Terran_SCV] = (BuildOrder::shouldScout() || Strategy::enemyPossibleProxy() || Strategy::enemyProxy());
            }

            // Zerg
            if (Broodwar->self()->getRace() == Races::Zerg) {
                desiredScoutTypeCounts[Zerg_Overlord] = 2;
                desiredScoutTypeCounts[Zerg_Drone] = int(BuildOrder::shouldScout() || Strategy::enemyProxy())
                    + int(BuildOrder::shouldScout() && BuildOrder::isProxy());

                // No drone scouting when encountering the following situations
                if ((Players::ZvP() && Util::getTime() > Time(3, 30) && Strategy::enemyFastExpand())
                    || (Players::ZvP() && Util::getTime() > Time(4, 30))
                    || (Players::ZvT() && Util::getTime() > Time(4, 30))
                    || workerScoutDenied
                    || Strategy::enemyRush()
                    || Strategy::enemyPressure()
                    || Strategy::enemyWalled()
                    || Strategy::enemyFastExpand()
                    || (Terrain::isShitMap() && Terrain::getEnemyStartingPosition().isValid())
                    || (BuildOrder::isProxy() && Terrain::getEnemyStartingPosition().isValid())
                    || Strategy::getEnemyBuild() == "FFE"
                    || (Strategy::getEnemyBuild() == "2Gate" && (Util::getTime() > Time(3, 30) || Strategy::getEnemyOpener() != "Unknown"))
                    || Strategy::getEnemyBuild() == "1GateCore")
                    desiredScoutTypeCounts[Zerg_Drone] = 0;

                // No overlord scouting main when encountering the following situations
                if (Players::getStrength(PlayerState::Enemy).groundToAir > 0.0
                    || Players::getStrength(PlayerState::Enemy).airToAir > 0.0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) > 0
                    || Players::ZvT()) {
                    onlyNaturalScout = true;
                    desiredScoutTypeCounts[Zerg_Overlord] = 1;
                }

                // If we need to sacrifice an Overlord, ensure we have at least 1
                sacrifice = Players::ZvP() && Terrain::getEnemyStartingPosition().isValid() && Terrain::getEnemyNatural() && Broodwar->isExplored(Terrain::getEnemyNatural()->getBase()->Location()) && !Strategy::enemyFastExpand() && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) < 2 && Strategy::getEnemyTransition() == "Unknown";
                if (sacrifice)
                    desiredScoutTypeCounts[Zerg_Overlord] = 1;

                if (Util::getTime() > Time(10, 00)
                    || Strategy::getEnemyBuild() == "FFE"
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) > 0
                    || Players::getTotalCount(PlayerState::Enemy, Zerg_Mutalisk) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Zerg_Evolution_Chamber) > 0)
                    desiredScoutTypeCounts[Zerg_Overlord] = 0;
            }
        }

        void updateScoutRoles()
        {
            bool sendAnother = scoutDeadFrame < 0 && (Broodwar->getFrameCount() - scoutDeadFrame > 240 || (Util::getTime() < Time(4, 0) && Strategy::getEnemyTransition() == "Unknown"));
            const auto assign = [&](UnitType type) {
                shared_ptr<UnitInfo> scout = nullptr;

                // Proxy takes furthest from natural choke
                if (type.isWorker() && BuildOrder::getCurrentOpener() == "Proxy") {
                    scout = Util::getFurthestUnit(Position(BWEB::Map::getNaturalChoke()->Center()), PlayerState::Self, [&](auto &u) {
                        return u.getRole() == Role::Worker && u.getType() == type && (!u.hasResource() || !u.getResource().getType().isRefinery()) && u.getBuildType() == None && !u.unit()->isCarryingMinerals() && !u.unit()->isCarryingGas();
                    });
                }
                else if (type.isFlyer()) {
                    scout = Util::getClosestUnit(Position(BWEB::Map::getNaturalChoke()->Center()), PlayerState::Self, [&](auto &u) {
                        return u.getRole() == Role::Support && u.getType() == type;
                    });
                }
                else if (type.isWorker())
                    scout = Util::getClosestUnitGround(Position(BWEB::Map::getNaturalChoke()->Center()), PlayerState::Self, [&](auto &u) {
                    return u.getRole() == Role::Worker && u.getType() == type && (!u.hasResource() || !u.getResource().getType().isRefinery()) && u.getBuildType() == None && !u.unit()->isCarryingMinerals() && !u.unit()->isCarryingGas();
                });

                if (scout) {
                    scout->setRole(Role::Scout);
                    scout->setBuildingType(None);
                    scout->setBuildPosition(TilePositions::Invalid);

                    if (scout->hasResource())
                        Workers::removeUnit(*scout);
                }
            };

            const auto remove = [&](UnitType type) {
                shared_ptr<UnitInfo> scout = nullptr;
                if (type.isFlyer()) {
                    scout = Util::getFurthestUnit(Terrain::getEnemyStartingPosition(), PlayerState::Self, [&](auto &u) {
                        return u.getRole() == Role::Scout && u.getType() == type;
                    });
                }
                else {
                    scout = Util::getClosestUnitGround(BWEB::Map::getMainPosition(), PlayerState::Self, [&](auto &u) {
                        return u.getRole() == Role::Scout && u.getType() == type;
                    });
                }

                if (scout)
                    scout->setRole(Role::None);
            };

            for (auto &[type, count] : desiredScoutTypeCounts) {
                if (sendAnother && currentScoutTypeCounts[type] < desiredScoutTypeCounts[type])
                    assign(type);
                else if (currentScoutTypeCounts[type] > desiredScoutTypeCounts[type])
                    remove(type);
            }
        }

        void updateScoutOrder()
        {
            scoutOrder.clear();
            scoutOrderFirstOverlord.clear();
            vector<BWEB::Station*> mainStations;

            // Get first natural by air for overlord order
            BWEB::Station * closestNatural = nullptr;
            auto distBest = DBL_MAX;
            for (auto &station : BWEB::Stations::getStations()) {

                // Add to main stations
                if (station.isMain() && &station != Terrain::getMyMain())
                    mainStations.push_back(&station);

                if (!station.isNatural() || station == Terrain::getMyNatural())
                    continue;

                auto dist = station.getBase()->Center().getDistance(Terrain::getMyMain()->getBase()->Center());
                if (dist < distBest) {
                    closestNatural = &station;
                    distBest = dist;
                }
            }

            if (closestNatural) {

                // Create overlord scouting order
                auto startingPosition = closestNatural->getBase()->Center();
                sort(mainStations.begin(), mainStations.end(), [&](auto &lhs, auto &rhs) {
                    return lhs->getBase()->Center().getDistance(startingPosition) < rhs->getBase()->Center().getDistance(startingPosition);
                });
                for (auto &station : mainStations)
                    scoutOrderFirstOverlord.push_back(station);

                // Create worker scouting order
                scoutOrder = scoutOrderFirstOverlord;
                reverse(scoutOrder.begin(), scoutOrder.end());

                /*int i = 0;
                for (auto &station : scoutOrderFirstOverlord) {
                    Broodwar->drawTextMap(station->getBase()->Center(), "%d", i);
                    i++;
                }                i = 0;
                for (auto &station : scoutOrder) {
                    Broodwar->drawTextMap(station->getBase()->Center() + Position(0, 16), "%d", i);
                    i++;
                }*/
            }
        }

        void updateScoutTargets()
        {
            scoutTargets.clear();
            proxyPosition = Positions::Invalid;

            const auto addTarget = [](Position here, ScoutType state, int radius = 0) {
                auto center = Util::clipPosition(here);
                auto sRadius = int(round(1.5*radius));

                if (radius == 0)
                    scoutTargets.push_back(ScoutTarget(center, center.getDistance(BWEB::Map::getMainPosition()), state));
                else {
                    vector<Position> positions ={ here + Position(sRadius, 0),
                        here + Position(-sRadius, 0),
                        here + Position(0, sRadius),
                        here + Position(0, -sRadius),
                        here + Position(radius, radius),
                        here + Position(-radius, radius),
                        here + Position(radius, -radius),
                        here + Position(-radius, -radius) };
                    for (auto &pos : positions)
                        pos = Util::clipPosition(pos);
                    scoutTargets.push_back(ScoutTarget(center, center.getDistance(BWEB::Map::getMainPosition()), state, positions));
                }
            };

            // Against known proxies without visible proxy style buildings
            const auto closestProxyBuilding = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto& u) {
                return u.isProxy();
            });
            if (Strategy::enemyProxy() && !closestProxyBuilding) {
                if (Strategy::getEnemyBuild() == "CannonRush")
                    addTarget(BWEB::Map::getMainPosition(), ScoutType::Proxy, 200);
                else
                    addTarget(mapBWEM.Center(), ScoutType::Proxy, 200);
            }

            // If enemy start is valid, add targets around it and the natural
            if (Terrain::getEnemyStartingPosition().isValid()) {

                // Add each enemy station as a target
                addTarget(Position(Terrain::getEnemyMain()->getBase()->Center()), ScoutType::Main, 160);
                if (onlyNaturalScout)
                    addTarget(Position(Terrain::getEnemyNatural()->getBase()->Center()), ScoutType::Natural, 160);

                // Add enemy geyser as a scout
                if (Terrain::getEnemyMain() && !Stations::isGeyserExplored(Terrain::getEnemyMain()) && Util::getTime() < Time(3, 45)) {
                    for (auto &geyser : Resources::getMyGas()) {
                        if (geyser->getStation() == Terrain::getEnemyMain())
                            addTarget(Position(geyser->getPosition()), ScoutType::Main, 0);
                    }
                }

                // Add each enemy pylon as a target
                for (auto &unit : Units::getUnits(PlayerState::Enemy)) {
                    if (unit->getType() == Protoss_Pylon && unit->getTilePosition().isValid() && mapBWEM.GetArea(unit->getTilePosition()) == Terrain::getEnemyMain()->getBase()->GetArea())
                        addTarget(unit->getPosition(), ScoutType::Main, 0);
                }

                // Add main choke as a target
                if (Terrain::getEnemyMain() && Terrain::getEnemyMain()->getChokepoint() && Stations::isBaseExplored(Terrain::getEnemyMain()))
                    addTarget(Position(Terrain::getEnemyMain()->getChokepoint()->Center()), ScoutType::Main, 0);

                // Add safe natural positions
                if (onlyNaturalScout) {
                    addTarget(expansionWatchPositions[Terrain::getEnemyNatural()], ScoutType::Safe);
                    if (Broodwar->getFrameCount() - Grids::lastVisibleFrame(Terrain::getEnemyNatural()->getBase()->Location()) >= 240)
                        addTarget(Terrain::getEnemyNatural()->getBase()->Center(), ScoutType::Safe);
                }
            }

            // Scout the popular middle proxy location if it's walkable
            if (!Players::vZ() && !Terrain::foundEnemy() && scoutOrder.front() && (Stations::isBaseExplored(scoutOrder.front()) || Broodwar->getStartLocations().size() >= 4) && !Terrain::isExplored(mapBWEM.Center()) && BWEB::Map::getGroundDistance(BWEB::Map::getMainPosition(), mapBWEM.Center()) != DBL_MAX)
                addTarget(mapBWEM.Center(), ScoutType::Proxy);

            // Determine if we achieved a full scout
            for (auto &target : scoutTargets) {
                if (target.type != ScoutType::Main)
                    continue;
                int exploredCount = 0;
                for (auto &pos : target.positions)
                    exploredCount += Broodwar->isExplored(TilePosition(pos));

                auto fullyExplored = Players::ZvZ() ? 2 : 5;
                if (exploredCount > fullyExplored && Terrain::getEnemyNatural() && Broodwar->isExplored(TilePosition(Terrain::getEnemyNatural()->getBase()->Center())))
                    fullScout = true;
            }
        }

        void updateDestination(UnitInfo& unit)
        {
            auto start = unit.getWalkPosition();
            auto distBest = DBL_MAX;

            // If we have scout targets, find the closest scout target
            auto best = -1.0;
            auto minTimeDiff = 0;

            ScoutTarget * bestTarget = nullptr;
            for (auto &target : scoutTargets) {

                if (!scoutAllowedHere(target, unit)
                    || target.assigned)
                    continue;

                for (auto &pos : target.positions) {
                    auto time = Grids::lastVisibleFrame(TilePosition(pos));
                    auto timeDiff = Broodwar->getFrameCount() - time;
                    auto score = double(timeDiff) / target.dist;

                    if (!Broodwar->isExplored(TilePosition(target.center)) && pos != target.center)
                        continue;

                    if (score > best) {
                        best = score;
                        unit.setDestination(pos);
                        bestTarget = &target;
                    }
                }
            }
            if (bestTarget)
                bestTarget->assigned = true;

            if (scoutTargets.empty() || !unit.getDestination().isValid()) {
                auto &list = (firstOverlord && unit.getType() == Zerg_Overlord) ? scoutOrderFirstOverlord : scoutOrder;
                for (auto &station : list) {
                    auto closestNatural = BWEB::Stations::getClosestNaturalStation(station->getBase()->Location());
                    if (closestNatural && !Stations::isBaseExplored(closestNatural) && unit.getType() == Zerg_Overlord && !Terrain::isShitMap()) {
                        unit.setDestination(closestNatural->getBase()->Center());
                        break;
                    }
                    if (!Stations::isBaseExplored(station)) {
                        unit.setDestination(station->getBase()->Center());
                        break;
                    }
                }
            }
        }

        void updatePath(UnitInfo& unit)
        {
            if (!unit.isFlying()) {
                if (unit.getDestination().isValid() && unit.getDestinationPath().getTarget() != TilePosition(unit.getDestination()) && (!mapBWEM.GetArea(TilePosition(unit.getPosition())) || !mapBWEM.GetArea(TilePosition(unit.getDestination())) || mapBWEM.GetArea(TilePosition(unit.getPosition()))->AccessibleFrom(mapBWEM.GetArea(TilePosition(unit.getDestination()))))) {
                    BWEB::Path newPath(unit.getPosition(), unit.getDestination(), unit.getType());
                    newPath.generateJPS([&](const TilePosition &t) { return newPath.terrainWalkable(t); });
                    unit.setDestinationPath(newPath);
                }

                if (unit.getDestinationPath().getTarget() == TilePosition(unit.getDestination())) {
                    auto newDestination = Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                        return p.getDistance(unit.getPosition()) >= 64.0;
                    });

                    if (newDestination.isValid())
                        unit.setDestination(newDestination);
                }
            }
        }

        constexpr tuple commands{ Command::attack, Command::kite, Command::explore, Command::move };
        void updateDecision(UnitInfo& unit)
        {
            // Convert our commands to strings to display what the unit is doing for debugging
            map<int, string> commandNames{
                make_pair(0, "Attack"),
                make_pair(1, "Kite"),
                make_pair(2, "Explore"),
                make_pair(3, "Move")
            };

            // Gas steal tester
            if (Broodwar->self()->getName() == "McRaveGasSteal" && Terrain::foundEnemy()) {
                auto gas = Broodwar->getClosestUnit(Terrain::getEnemyStartingPosition(), Filter::GetType == Resource_Vespene_Geyser);
                if (gas && gas->exists() && gas->getPosition().getDistance(Terrain::getEnemyStartingPosition()) < 320 && unit.getPosition().getDistance(Terrain::getEnemyStartingPosition()) < 160) {
                    if (unit.unit()->getLastCommand().getType() != UnitCommandTypes::Build)
                        unit.unit()->build(Broodwar->self()->getRace().getRefinery(), gas->getTilePosition());
                    return;
                }
                unit.unit()->move(gas->getPosition());
            }

            int width = unit.getType().isBuilding() ? -16 : unit.getType().width() / 2;
            int i = Util::iterateCommands(commands, unit);
            Broodwar->drawTextMap(unit.getPosition() + Position(width, 0), "%c%s", Text::White, commandNames[i].c_str());
        }

        void updateScouts()
        {
            vector<std::weak_ptr<UnitInfo>> sortedScouts;
            firstOverlord = true;

            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;
                if (unit.getRole() == Role::Scout)
                    sortedScouts.push_back(unit.weak_from_this());
            }

            sort(sortedScouts.begin(), sortedScouts.end(), [&](auto &lhs, auto &rhs) {
                return lhs.lock()->getPosition().getDistance(BWEB::Map::getMainPosition()) > rhs.lock()->getPosition().getDistance(BWEB::Map::getMainPosition());
            });

            for (auto &u : sortedScouts) {
                if (auto unit = u.lock()) {
                    updateDestination(*unit);
                    updatePath(*unit);
                    updateDecision(*unit);
                    if (unit->getType() == Zerg_Overlord)
                        firstOverlord = false;
                }
            }
        }
    }

    void onFrame()
    {
        Visuals::startPerfTest();
        checkScoutDenied();
        updateScoutOrder();
        updateScountCount();
        updateScoutTargets();
        updateScoutRoles();
        updateScouts();

        for (auto &pos : expansionWatchPositions) {
            Visuals::drawCircle(pos.second, 10, Colors::Green);
        }
        onStart();

        Visuals::endPerfTest("Scouts");
    }

    void onStart()
    {
        // Attempt to find a position that is higher altitude than the enemys natural and isn't in their main
        for (auto &station : BWEB::Stations::getStations()) {
            auto closestMain = BWEB::Stations::getClosestMainStation(TilePosition(station.getBase()->Center()));
            if (!station.isNatural()
                || !closestMain)
                continue;

            auto distBest = 0.0;
            auto posBest = station.getBase()->Center();
            for (int x = -14; x < 14; x++) {
                for (int y = -14; y < 14; y++) {
                    auto tile = TilePosition(station.getBase()->Center()) + TilePosition(x, y);
                    auto pos = Position(tile);
                    auto dist = pos.getDistance(Position(closestMain->getChokepoint()->Center())) + pos.getDistance(Position(station.getChokepoint()->Center())) + pos.getDistance(closestMain->getBase()->Center());

                    if (!Grids::hasCliffVision(tile))
                        dist = dist * 10;

                    if (dist > distBest) {
                        distBest = dist;
                        posBest = pos;
                    }
                }
            }

            Visuals::drawCircle(posBest, 4, Colors::Cyan, true);
            expansionWatchPositions[&station] = posBest;
        }
    }

    void removeUnit(UnitInfo& unit)
    {
        scoutDeadFrame = Broodwar->getFrameCount();
    }

    bool gotFullScout() { return fullScout; }
    bool isSacrificeScout() { return sacrifice; }
    bool enemyDeniedScout() { return workerScoutDenied; }
    vector<BWEB::Station*> getScoutOrder(UnitType type) {
        if (type == Zerg_Overlord || !Players::ZvZ())
            return scoutOrderFirstOverlord;
        return scoutOrder;
    }
}