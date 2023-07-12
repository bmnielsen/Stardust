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
        bool info = false;
        bool fullScout = false;
        bool sacrifice = false;
        bool workerScoutDenied = false;
        bool onlyNaturalScout = false;
        bool firstOverlord = false;
        UnitType proxyType = None;
        Position proxyPosition = Positions::Invalid;
        vector<BWEB::Station*> scoutOrder, scoutOrderFirstOverlord;
        map<BWEB::Station *, Position> expansionWatchPositions;

        void addTarget(Position here, ScoutType state, int radius = 0) {
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
        }

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
                return u->getType().isWorker() && u->getRole() == Role::Scout;
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
                    + int(Players::PvP() && (Spy::enemyProxy() || Spy::enemyPossibleProxy()) && com(Protoss_Zealot) < 1);

                // No probe scouting when encountering the following situations
                if ((Players::PvZ() && Spy::enemyRush() && Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) >= 2)
                    || (Players::PvT() && (Spy::enemyPressure() || Spy::enemyWalled()))
                    || (BuildOrder::isPlayPassive() && Spy::enemyPressure())
                    || (Util::getTime() > Time(5, 00)))
                    desiredScoutTypeCounts[Protoss_Probe] = 0;
            }

            // Terran
            if (Broodwar->self()->getRace() == Races::Terran) {
                desiredScoutTypeCounts[Terran_SCV] = (BuildOrder::shouldScout() || Spy::enemyPossibleProxy() || Spy::enemyProxy());
            }

            // Zerg
            if (Broodwar->self()->getRace() == Races::Zerg) {
                desiredScoutTypeCounts[Zerg_Overlord] = 2;
                desiredScoutTypeCounts[Zerg_Drone] = int(BuildOrder::shouldScout() || Spy::enemyProxy())
                    + int(BuildOrder::shouldScout() && BuildOrder::isProxy());

                // No drone scouting when encountering the following situations
                if ((Players::ZvP() && Util::getTime() > Time(3, 30) && Spy::enemyFastExpand())
                    || (Players::ZvP() && Util::getTime() > Time(4, 30))
                    || (Players::ZvT() && Util::getTime() > Time(4, 30))
                    || workerScoutDenied
                    || Spy::enemyRush()
                    || Spy::enemyPressure()
                    || Spy::enemyWalled()
                    || Spy::enemyFastExpand()
                    || Spy::getEnemyOpener() == "8Rax"
                    || (BuildOrder::isProxy() && Terrain::getEnemyStartingPosition().isValid())
                    || Spy::getEnemyBuild() == "FFE"
                    || (Spy::getEnemyBuild() == "2Gate" && (Spy::enemyFastExpand() || Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 || Players::getCompleteCount(PlayerState::Enemy, Protoss_Cybernetics_Core) > 0))
                    || (Spy::getEnemyBuild() == "1GateCore" && (Spy::enemyFastExpand() || Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 || Players::getCompleteCount(PlayerState::Enemy, Protoss_Cybernetics_Core) > 0)))
                    desiredScoutTypeCounts[Zerg_Drone] = 0;

                auto enemyAir = Players::getStrength(PlayerState::Enemy).groundToAir > 0.0
                    || Players::getStrength(PlayerState::Enemy).airToAir > 0.0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) > 0
                    || Players::ZvT();

                // No overlord scouting main when encountering the following situations
                if (enemyAir || Spy::enemyFastExpand() || !Players::ZvP() || Util::getTime() > Time(3, 45)) {
                    onlyNaturalScout = true;
                    desiredScoutTypeCounts[Zerg_Overlord] = 1;
                }

                if (total(Zerg_Mutalisk) >= 6
                    || Spy::getEnemyBuild() == "FFE"
                    || (Players::ZvZ() && Util::getTime() > Time(5, 00)))
                    desiredScoutTypeCounts[Zerg_Overlord] = 0;
            }
        }

        void updateScoutRoles()
        {
            bool sendAnother = scoutDeadFrame < 0 && (Broodwar->getFrameCount() - scoutDeadFrame > 240 || (Util::getTime() < Time(4, 0) && Spy::getEnemyTransition() == "Unknown"));
            const auto assign = [&](UnitType type) {
                UnitInfo* scout = nullptr;

                auto assignPos = BWEB::Map::getNaturalChoke() ? Position(BWEB::Map::getNaturalChoke()->Center()) : BWEB::Map::getMainPosition();

                // Proxy takes furthest from natural choke
                if (type.isWorker() && BuildOrder::getCurrentOpener() == "Proxy") {
                    scout = Util::getFurthestUnit(assignPos, PlayerState::Self, [&](auto &u) {
                        return u->getRole() == Role::Worker && u->getType() == type && (!u->hasResource() || !u->getResource().lock()->getType().isRefinery()) && u->getBuildType() == None && !u->unit()->isCarryingMinerals() && !u->unit()->isCarryingGas();
                    });
                }
                else if (type.isFlyer()) {
                    scout = Util::getClosestUnit(assignPos, PlayerState::Self, [&](auto &u) {
                        return u->getRole() == Role::Support && u->getType() == type;
                    });
                }
                else if (type.isWorker())
                    scout = Util::getClosestUnitGround(assignPos, PlayerState::Self, [&](auto &u) {
                    return u->getRole() == Role::Worker && u->getType() == type && (!u->hasResource() || !u->getResource().lock()->getType().isRefinery()) && u->getBuildType() == None && !u->unit()->isCarryingMinerals() && !u->unit()->isCarryingGas();
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
                UnitInfo* scout = nullptr;
                if (type.isFlyer()) {
                    scout = Util::getFurthestUnit(Terrain::getEnemyStartingPosition(), PlayerState::Self, [&](auto &u) {
                        return u->getRole() == Role::Scout && u->getType() == type;
                    });
                }
                else {
                    scout = Util::getClosestUnitGround(BWEB::Map::getMainPosition(), PlayerState::Self, [&](auto &u) {
                        return u->getRole() == Role::Scout && u->getType() == type;
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

                // Sometimes proxies get placed at the 3rd closest Station
                if (Players::ZvP() && !Terrain::getEnemyStartingPosition().isValid() && Broodwar->mapFileName().find("Outsider") != string::npos) {
                    auto thirdStation = Stations::getClosestStationAir(BWEB::Map::getMainPosition(), PlayerState::None, [&](auto &station) {
                        return station != Terrain::getMyMain() && station != Terrain::getMyNatural();
                    });
                    if (thirdStation)
                        scoutOrder.push_back(thirdStation);
                }

                if (!Players::vFFA())
                    reverse(scoutOrder.begin(), scoutOrder.end());
            }
        }

        void updateScoutTargets()
        {
            scoutTargets.clear();
            proxyPosition = Positions::Invalid;

            // Against known proxies without visible proxy style buildings
            const auto closestProxyBuilding = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto& u) {
                return u->isProxy();
            });
            if (Spy::enemyProxy() && !closestProxyBuilding) {
                if (Spy::getEnemyBuild() == "CannonRush")
                    addTarget(BWEB::Map::getMainPosition(), ScoutType::Proxy, 200);
                else
                    addTarget(mapBWEM.Center(), ScoutType::Proxy, 200);
            }

            // If enemy start is valid, add targets around it and the natural
            if (Terrain::getEnemyMain() && Terrain::getEnemyNatural()) {

                // Add each enemy station as a target
                if (!fullScout)
                    addTarget(Position(Terrain::getEnemyMain()->getBase()->Center()), ScoutType::Main, 256);

                // Add enemy geyser as a scout
                if (Terrain::getEnemyMain() && fullScout && !Stations::isGeyserExplored(Terrain::getEnemyMain()) && Util::getTime() < Time(3, 45)) {
                    for (auto &geyser : Resources::getMyGas()) {
                        if (geyser->getStation() == Terrain::getEnemyMain())
                            addTarget(Position(geyser->getPosition()), ScoutType::Main);
                    }
                }

                // Add each enemy building as a target
                for (auto &unit : Units::getUnits(PlayerState::Enemy)) {
                    if (fullScout && unit->getType().isBuilding() && unit->getTilePosition().isValid() && mapBWEM.GetArea(unit->getTilePosition()) == Terrain::getEnemyMain()->getBase()->GetArea())
                        addTarget(unit->getPosition(), ScoutType::Main, 96.0);
                }

                // Add main choke as a target
                if (Terrain::getEnemyMain() && fullScout && Terrain::getEnemyMain()->getChokepoint() && Stations::isBaseExplored(Terrain::getEnemyMain()))
                    addTarget(Position(Terrain::getEnemyMain()->getChokepoint()->Center()), ScoutType::Main);

                // Add natural position as a target
                if (Terrain::getEnemyNatural() && Util::getTime() > Time(4, 00))
                    addTarget(Terrain::getEnemyNatural()->getBase()->Center(), ScoutType::Natural);

                // Add safe natural positions
                if (onlyNaturalScout) {
                    addTarget(expansionWatchPositions[Terrain::getEnemyNatural()], ScoutType::Safe);
                    if (Broodwar->getFrameCount() - Grids::lastVisibleFrame(TilePosition(Terrain::getEnemyNatural()->getBase()->Center())) >= 50)
                        addTarget(Terrain::getEnemyNatural()->getBase()->Center(), ScoutType::Safe);
                }
            }

            // Scout the popular middle proxy location if it's walkable
            if (!Players::vZ() && !Terrain::foundEnemy() && !scoutOrder.empty() && scoutOrder.front() && (Stations::isBaseExplored(scoutOrder.front()) || Broodwar->getStartLocations().size() >= 4) && !Terrain::isExplored(mapBWEM.Center()) && BWEB::Map::getGroundDistance(BWEB::Map::getMainPosition(), mapBWEM.Center()) != DBL_MAX)
                addTarget(mapBWEM.Center(), ScoutType::Proxy);

            // Determine if we achieved a full scout
            for (auto &target : scoutTargets) {
                if (target.type != ScoutType::Main)
                    continue;
                int exploredCount = 0;
                for (auto &pos : target.positions)
                    exploredCount += Broodwar->isExplored(TilePosition(pos));

                auto fullyExplored = Players::ZvZ() ? 2 : 7;
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
            if (bestTarget) {
                bestTarget->assigned = true;
            }

            if (scoutTargets.empty() || !unit.getDestination().isValid()) {
                auto &list = (firstOverlord && unit.getType() == Zerg_Overlord) ? scoutOrderFirstOverlord : scoutOrder;
                for (auto &station : list) {
                    auto closestNatural = BWEB::Stations::getClosestNaturalStation(station->getBase()->Location());
                    if (closestNatural && !Stations::isBaseExplored(closestNatural) && unit.getType() == Zerg_Overlord) {
                        unit.setDestination(closestNatural->getBase()->Center());
                        break;
                    }
                    if (!Stations::isBaseExplored(station)) {
                        unit.setDestination(station->getBase()->Center());
                        break;
                    }
                }
            }
            unit.setNavigation(unit.getDestination());
        }

        void updatePath(UnitInfo& unit)
        {
            if (!unit.isFlying()) {
                if (unit.getDestination().isValid() && unit.getDestinationPath().getTarget() != TilePosition(unit.getDestination()) && (!mapBWEM.GetArea(TilePosition(unit.getPosition())) || !mapBWEM.GetArea(TilePosition(unit.getDestination())) || mapBWEM.GetArea(TilePosition(unit.getPosition()))->AccessibleFrom(mapBWEM.GetArea(TilePosition(unit.getDestination()))))) {

                    const auto threat = [&](const TilePosition &t) {
                        return Grids::getEGroundThreat(Position(t) + Position(16, 16)) * 50.0;
                    };

                    BWEB::Path newPath(unit.getPosition(), unit.getDestination(), unit.getType());
                    newPath.generateAS([&](const TilePosition &t) { return !newPath.unitWalkable(t) ? DBL_MAX : threat(t); });
                    unit.setDestinationPath(newPath);
                }

                if (unit.getDestinationPath().getTarget() == TilePosition(unit.getDestination())) {
                    auto newDestination = Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                        return p.getDistance(unit.getPosition()) >= 64.0;
                    });

                    if (newDestination.isValid())
                        unit.setNavigation(newDestination);
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
                return Terrain::getEnemyMain() ? lhs.lock()->getPosition().getDistance(Terrain::getEnemyStartingPosition()) < rhs.lock()->getPosition().getDistance(Terrain::getEnemyStartingPosition())
                    : lhs.lock()->getPosition().getDistance(BWEB::Map::getMainPosition()) > rhs.lock()->getPosition().getDistance(BWEB::Map::getMainPosition());
            });

            info = false;
            for (auto &u : sortedScouts) {
                if (auto unit = u.lock()) {
                    updateDestination(*unit);
                    updatePath(*unit);
                    updateDecision(*unit);
                    if (unit->getType() == Zerg_Overlord)
                        firstOverlord = false;
                    if (Terrain::inTerritory(PlayerState::Enemy, unit->getPosition()))
                        info = true;
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
        //onStart();

        Visuals::endPerfTest("Scouts");
    }

    void onStart()
    {
        // Attempt to find a position that is higher altitude than the enemys natural and isn't in their main
        for (auto &station : BWEB::Stations::getStations()) {
            auto closestMain = BWEB::Stations::getClosestMainStation(TilePosition(station.getBase()->Center()));
            if (!station.isNatural()
                || !station.getChokepoint()
                || !closestMain
                || !closestMain->getChokepoint())
                continue;

            const auto closestWatchable = [&](auto &t, auto &p) {
                auto closest = 320.0;
                for (int x = -8; x <= 8; x++) {
                    for (int y = -8; y <= 8; y++) {
                        const auto tile = t + TilePosition(x, y);
                        if (!tile.isValid())
                            continue;

                        const auto enemyPos = Position(tile) + Position(16, 16);
                        const auto boxDist = Util::boxDistance(Zerg_Overlord, p, Protoss_Dragoon, enemyPos);
                        const auto walkableAndConnected = BWEB::Map::isWalkable(tile, Protoss_Dragoon) && mapBWEM.GetArea(tile) && !mapBWEM.GetArea(tile)->AccessibleNeighbours().empty();

                        if (boxDist < closest && walkableAndConnected) {
                            closest = boxDist;
                        }
                    }
                }
                return closest;
            };

            auto distBest = 0.0;
            auto posBest = station.getBase()->Center();
            for (int x = -14; x < 14; x++) {
                for (int y = -14; y < 14; y++) {
                    auto tile = TilePosition(station.getBase()->Center()) + TilePosition(x, y);
                    auto pos = Position(tile) + Position(16, 16);
                    auto dist = closestWatchable(tile, pos);

                    if (!pos.isValid() || !tile.isValid())
                        continue;

                    if (dist > distBest) {
                        distBest = dist;
                        posBest = pos;
                    }
                }
            }

            Visuals::drawCircle(posBest, 4, Colors::Cyan, false);
            expansionWatchPositions[&station] = posBest;
        }
    }

    void removeUnit(UnitInfo& unit)
    {
        scoutDeadFrame = Broodwar->getFrameCount();
    }

    bool gatheringInformation() { return info; }
    bool gotFullScout() { return fullScout; }
    bool isSacrificeScout() { return sacrifice; }
    bool enemyDeniedScout() { return workerScoutDenied; }
    vector<BWEB::Station*> getScoutOrder(UnitType type) {
        if (type == Zerg_Overlord || !Players::ZvZ())
            return scoutOrderFirstOverlord;
        return scoutOrder;
    }
}