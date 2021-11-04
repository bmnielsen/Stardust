#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Combat {

    namespace {

        int lastCheckFrame = 0;
        weak_ptr<UnitInfo> checker;

        int lastRoleChange = 0;
        set<Position> retreatPositions;
        set<Position> defendPositions;
        vector<Position> lastSimPositions;
        multimap<double, Position> combatClusters;
        map<const BWEM::ChokePoint*, vector<WalkPosition>> concaveCache;
        multimap<double, UnitInfo&> combatUnitsByDistance;
        vector<TilePosition> occupiedTiles;

        bool holdChoke = false;;
        bool multipleUnitsBlocked = false;
        vector<BWEB::Station*> combatScoutOrder;

        BWEB::Path airClusterPath;
        weak_ptr<UnitInfo> airCommander;
        pair<double, Position> airCluster;
        pair<UnitCommandType, Position> airCommanderCommand;
        multimap<double, Position> groundCleanupPositions;
        multimap<double, Position> airCleanupPositions;

        struct Formation {
            Position center, start, from;
            map<int, vector<Position>> positions;
            double range = 0.0;
            double baseRadius, radius = 0.0;
            double angle = 0.0;
            double unitTangentSize;
            UnitType type = UnitTypes::None;
            int count = 0;

            Formation(double _range, UnitType _type, Position _center, Position _start, Position _from) {
                range = _range;
                type = _type;
                center = _center;
                start = _start;
                from = _from;
                count = 1;
            }
        };

        multimap<Position, Formation> groundFormations;
        constexpr tuple commands{ Command::misc, Command::special, Command::attack, Command::approach, Command::kite, Command::defend, Command::explore, Command::escort, Command::retreat, Command::move };

        void getBestPathFormationPoint(UnitInfo &unit)
        {
            // WIP
            return;

            vector<TilePosition> tiles;
            auto currentDestTile = TilePosition(unit.getDestination());
            auto currentDest = unit.getDestination();

            TilePosition nextPathPoint = TilePosition(Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                return p.getDistance(unit.getPosition()) >= 96.0;
            }));
            Position next = Position(nextPathPoint) + Position(16, 16);

            for (int x = currentDestTile.x - 3; x <= currentDestTile.x + 3; x++) {
                for (int y = currentDestTile.y - 3; y <= currentDestTile.y + 3; y++) {
                    auto center = Position(TilePosition(x, y)) + Position(16, 16);
                    if (center.getDistance(unit.getPosition()) > center.getDistance(unit.getDestination()) && center.getDistance(next) < unit.getDestination().getDistance(next) + 48.0)
                        tiles.push_back(TilePosition(x, y));
                }
            }

            auto distBest = DBL_MAX;
            for (auto &tile : tiles) {
                auto center = Position(tile) + Position(16, 16);
                auto dist = unit.getPosition().getDistance(center) * (0.1 + (8 * count(occupiedTiles.begin(), occupiedTiles.end(), tile)));
                if (unit.unit()->isSelected())
                    Visuals::drawBox(tile, tile + TilePosition(1, 1), Colors::Purple);
                if (dist < distBest && BWEB::Map::isWalkable(tile, unit.getType())) {
                    unit.setDestination(center);
                    distBest = dist;
                }
            }

            auto start = TilePosition(unit.getDestination());
            for (int x = start.x - 1; x <= start.x + 1; x++) {
                for (int y = start.y - 1; y <= start.y + 1; y++) {
                    auto tile = TilePosition(x, y);
                    occupiedTiles.push_back(tile);
                }
            }
            occupiedTiles.push_back(TilePosition(unit.getDestination()));
            //Visuals::drawBox(TilePosition(unit.getDestination()), TilePosition(unit.getDestination()) + TilePosition(1, 1), Colors::Green);

        }

        void updateFormations() {
            groundFormations.clear();

            // Count how many units at this destination, create any new formations we need
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;
                if (unit.getRole() != Role::Combat)
                    continue;

                auto formationFound = false;
                for (auto &[pos, formation] : groundFormations) {
                    if (formation.center == unit.getFormation() && formation.range == unit.getGroundRange()) {
                        formation.count++;
                        formationFound = true;
                        break;
                    }
                }

                if (!formationFound)
                    groundFormations.emplace(unit.getFormation(), Formation(unit.getGroundRange(), unit.getType(), unit.getFormation(), unit.getPosition(), (BWEB::Map::getMainPosition() + unit.getFormation()) / 2));
            }

            // Squish formations together that should be together
            for (auto &[pos1, formation1] : groundFormations) {
                for (auto &[pos2, formation2] : groundFormations) {
                    if (formation1.center == formation2.center && formation1.type != formation2.type && formation1.count >= 6 && formation1.range > formation2.range) {
                        formation1.count += formation2.count;
                        formation2.count = 0;
                        formation2.range = 0.0;
                    }
                }
            }

            // Generate formation positions
            for (auto &[pos, formation] : groundFormations) {

                if (formation.range == 0.0
                    || !formation.center.isValid())
                    continue;

                // For now, we only make formations as defensive measures, so let's just use a chokepoint angle
                auto tempClosestChoke =  Util::getClosestChokepoint(formation.center);
                formation.angle = BWEB::Map::getAngle(make_pair(tempClosestChoke->Pos(tempClosestChoke->end1), tempClosestChoke->Pos(tempClosestChoke->end2)));

                // Calculate base radius
                formation.unitTangentSize = sqrt(pow(formation.type.width(), 2.0) + pow(formation.type.height(), 2.0)) + 8.0;
                formation.radius = clamp(double(formation.count) * (formation.unitTangentSize / 2.0), double(tempClosestChoke->Width() / 2), 256.0);

                // Get closest enemy station
                const auto closestEnemyStation = Stations::getClosestStationAir(PlayerState::Enemy, formation.center);
                const auto closestSelfStation = Stations::getClosestStationAir(PlayerState::Self, formation.center);

                // Rotate angle perpendicular to the choke, based on which direction we want
                auto posStartPosition = Util::clipPosition(pos + Position(int(64.0 * cos(formation.angle + 1.57)), int(64.0 * sin(formation.angle + 1.57))));
                auto negStartPosition = Util::clipPosition(pos + Position(int(64.0 * cos(formation.angle - 1.57)), int(64.0 * sin(formation.angle - 1.57))));
                //Visuals::drawLine(formation.center, posStartPosition, Colors::Cyan);
                //Visuals::drawLine(formation.center, negStartPosition, Colors::Yellow);

                if (mapBWEM.GetArea(TilePosition(posStartPosition)) == Terrain::getDefendArea())
                    formation.angle = formation.angle + 1.57;
                else if (mapBWEM.GetArea(TilePosition(negStartPosition)) == Terrain::getDefendArea())
                    formation.angle = formation.angle - 1.57;
                else if (closestEnemyStation && posStartPosition.isValid() && negStartPosition.isValid()) {
                    mapBWEM.GetPath(posStartPosition, closestEnemyStation->getBase()->Center()).size() > mapBWEM.GetPath(negStartPosition, closestEnemyStation->getBase()->Center()).size() ?
                        formation.angle = formation.angle + 1.57 :
                        formation.angle = formation.angle - 1.57;
                }
                defendPositions.clear();

                //Broodwar->drawTextMap(posStartPosition, "%d", mapBWEM.GetPath(posStartPosition, closestEnemyStation->getBase()->Center()).size());
                //Broodwar->drawTextMap(negStartPosition, "%d", mapBWEM.GetPath(negStartPosition, closestEnemyStation->getBase()->Center()).size());

                // Create start position
                formation.start = pos + Position(int(64.0 * cos(formation.angle)), int(64.0 * sin(formation.angle)));
                //Visuals::drawLine(formation.start, formation.center, Colors::Purple);

                // See if there's a defense, set radius to at least its distance
                auto closestDefense = Util::getClosestUnit(formation.center, PlayerState::Self, [&](auto &u) {
                    return u.getRole() == Role::Defender && u.isCompleted() && mapBWEM.GetArea(u.getTilePosition()) == mapBWEM.GetArea(TilePosition(formation.start));
                });
                if (closestSelfStation && closestSelfStation->isNatural() && mapBWEM.GetArea(TilePosition(formation.start)) == closestSelfStation->getBase()->GetArea())
                    formation.radius = closestSelfStation->getBase()->Center().getDistance(formation.center) + 32.0;
                if (closestDefense && !Players::ZvT())
                    formation.radius = closestDefense->getPosition().getDistance(formation.center) + 64.0;

                formation.baseRadius = formation.radius;

                //// Find edges of concave
                //vector<WalkPosition> directions ={ {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };
                //vector<WalkPosition> edges;
                //int cnt = 0;
                //bool oppositesFound = false;
                //while (!oppositesFound) {
                //    cnt++;
                //    for (auto dir : directions) {
                //        WalkPosition c = WalkPosition(formation.center);
                //        WalkPosition w = c + (dir * cnt);
                //        if (Grids::getMobility(w) <= 0 || w.x == 1 || w.y == 1 || w.x == Broodwar->mapWidth() * 4 - 1 || w.y == Broodwar->mapHeight() * 4 - 1) {
                //            if (find_if(edges.begin(), edges.end(), [&](auto &p) { return ((p.x < c.x && w.x > c.x) || (p.x > c.x && w.x < c.x)) && ((p.y < c.y && w.y > c.y) || (p.y > c.y && w.y < c.y)); }) != edges.end()) {
                //                oppositesFound = true;
                //                edges ={ w, c + WalkPosition(w.x - c.x, w.y - c.y) };
                //                break;
                //            }
                //            edges.push_back(w);
                //        }
                //    }
                //}

                //if (!edges.empty()) {
                //    auto angleSum = 0.0;
                //    for (auto &edge : edges) {
                //        auto edgeCenter = Position(edge) + Position(4, 4);
                //        angleSum += BWEB::Map::getAngle(make_pair(formation.center, edgeCenter));
                //        Visuals::drawLine(formation.center, edgeCenter, Colors::Blue);
                //    }
                //    formation.angle = angleSum / double(edges.size());

                //}



                // Adjust radius if a small formation exists inside it
                for (auto &[_, otherFormation] : groundFormations) {
                    if (otherFormation.center == formation.center && otherFormation.range < formation.range)
                        formation.radius = max(formation.radius, otherFormation.radius + formation.unitTangentSize * 2.0);
                }

                // Create some formations
                auto radsPerUnit = min(formation.radius / (formation.unitTangentSize * formation.count * 1.57), formation.unitTangentSize / (1.0 * formation.radius));
                auto wrapCount = 0;
                auto countPerRadius = 0;

                auto radsPositive = formation.angle;
                auto radsNegative = formation.angle;
                auto lastPosPosition = Positions::Invalid;
                auto lastNegPosition = Positions::Invalid;

                bool stopPositive = false;
                bool stopNegative = false;
                while (wrapCount < 4 && int(formation.positions.size()) < 5000) {

                    auto posPosition = pos + Position(int(formation.radius * cos(radsPositive)), int(formation.radius * sin(radsPositive)));
                    auto negPosition = pos + Position(int(formation.radius * cos(radsNegative)), int(formation.radius * sin(radsNegative)));

                    auto validPosition = [&](Position &p, Position &last) {
                        if (!p.isValid()
                            || !formation.start.isValid()
                            || Grids::getMobility(p) <= 4
                            || mapBWEM.GetArea(TilePosition(p)) != mapBWEM.GetArea(TilePosition(formation.start))
                            || Util::boxDistance(formation.type, p, formation.type, last) <= 2)
                            return false;
                        countPerRadius++;
                        formation.positions[wrapCount].push_back(p);
                        return true;
                    };

                    // Neutral position
                    if (radsPositive == radsNegative && validPosition(posPosition, lastPosPosition)) {
                        radsPositive += radsPerUnit;
                        radsNegative -= radsPerUnit;
                        lastPosPosition = posPosition;
                        lastNegPosition = negPosition;
                        //Visuals::drawCircle(posPosition, 3, Colors::Yellow);
                        continue;
                    }

                    // Positive position
                    if (!stopPositive && validPosition(posPosition, lastPosPosition)) {
                        radsPositive += radsPerUnit;
                        lastPosPosition = posPosition;
                        //Visuals::drawCircle(posPosition, 3, Colors::Green);
                    }
                    else
                        radsPositive += 3.14 / 180.0;

                    // Negative position
                    if (!stopNegative && validPosition(negPosition, lastNegPosition)) {
                        radsNegative -= radsPerUnit;
                        lastNegPosition = negPosition;
                        //Visuals::drawCircle(negPosition, 2, Colors::Red);
                    }
                    else
                        radsNegative -= 3.14 / 180.0;

                    if (radsPositive > formation.angle + 1.57)
                        stopPositive = true;
                    if (radsNegative < formation.angle - 1.57)
                        stopNegative = true;

                    if (stopPositive && stopNegative) {
                        stopPositive = false;
                        stopNegative = false;
                        wrapCount++;
                        formation.radius += formation.unitTangentSize;
                        radsPositive = formation.angle;
                        radsNegative = formation.angle;
                    }
                }
            }
        }

        void assignFormations()
        {
            for (auto &[_, formation] : groundFormations) {

                for (int i = 0; i < 4; i++) {

                    // HACK: Sort them at the natural to block runbys
                    auto formationArea = mapBWEM.GetArea(TilePosition(formation.start));
                    if (formationArea == BWEB::Map::getNaturalArea())
                        sort(formation.positions[0].begin(), formation.positions[0].end(), [&](auto &lhs, auto &rhs) {
                        return lhs.getDistance(Position(BWEB::Map::getMainChoke()->Center())) < rhs.getDistance(Position(BWEB::Map::getMainChoke()->Center()));
                    });

                    int fromCenter = 0;
                    for (auto &pos : formation.positions[i]) {
                        fromCenter++;

                        auto closestUnit = Util::getClosestUnit(pos, PlayerState::Self, [&](auto &u) {
                            return (u.getGroundRange() == formation.range || (formation.count > 6 && formation.range >= u.getGroundRange())) && u.getDestination() == formation.center && !u.concaveFlag;
                        });
                        auto closestDef = Util::getClosestUnit((pos + formation.center) / 2, PlayerState::Self, [&](auto &u) {
                            return u.getRole() == Role::Defender && mapBWEM.GetArea(u.getTilePosition()) == mapBWEM.GetArea(TilePosition(formation.start));
                        });

                        if (!closestUnit)
                            continue;

                        auto closestBuilder = Util::getClosestUnit(pos, PlayerState::Self, [&](auto &u) {
                            return u.getBuildPosition().isValid();
                        });
                        auto closestStuck = Util::getClosestUnit(pos, PlayerState::Self, [&](auto &u) {
                            return (u.getFormation() == formation.center && u.getPosition().getDistance(formation.center) < formation.baseRadius - 64.0);
                        });

                        if ((closestStuck && pos.getDistance(closestStuck->getPosition()) < 32.0)
                            || (closestUnit->hasTarget() && Broodwar->getGroundHeight(TilePosition(pos)) <= Broodwar->getGroundHeight(closestUnit->getTarget().getTilePosition()) && closestUnit->getTarget().getPosition().getDistance(pos) < 480.0 && closestDef && closestUnit->getTarget().getGroundRange() > 32.0 && pos.getDistance(closestUnit->getTarget().getPosition()) < closestDef->getPosition().getDistance(closestUnit->getTarget().getPosition()))
                            || (closestBuilder && pos.getDistance(closestBuilder->getPosition()) < 128.0)
                            || Planning::overlapsPlan(*closestUnit, pos)
                            || Actions::overlapsActions(closestUnit->unit(), pos, TechTypes::Spider_Mines, PlayerState::Enemy, 96)
                            || !Util::findWalkable(*closestUnit, pos))
                            continue;

                        if (closestDef) {
                            auto closestWall = BWEB::Walls::getClosestWall(closestDef->getTilePosition());
                            if (closestWall && closestWall->getDefenses().find(closestDef->getTilePosition()) != closestWall->getDefenses().end()) {
                                if (Terrain::getEnemyStartingPosition().isValid()) {
                                    auto path = mapBWEM.GetPath(closestDef->getPosition(), Terrain::getEnemyStartingPosition());
                                    if (!path.empty()) {
                                        //Visuals::drawLine(closestDef->getPosition(), Position(path.front()->Center()), Colors::Orange);
                                        if (pos.getDistance(Position(path.front()->Center())) < closestDef->getPosition().getDistance(Position(path.front()->Center())) - 48.0)
                                            continue;
                                    }
                                }
                                else {
                                    auto choke = Util::getClosestChokepoint(formation.center);
                                    if (choke) {
                                        //Visuals::drawLine(closestDef->getPosition(), Position(choke->Center()), Colors::Yellow);
                                        if (pos.getDistance(Position(choke->Center())) < closestDef->getPosition().getDistance(Position(choke->Center())) - 48.0)
                                            continue;
                                    }
                                }
                            }
                        }

                        closestUnit->setDestination(pos);
                        closestUnit->concaveFlag = true;
                        defendPositions.insert(pos);
                    }
                }
            }
        }

        void updateRole(UnitInfo& unit)
        {
            // Can't change role to combat if not a worker or we did one this frame
            if (!unit.getType().isWorker()
                || lastRoleChange == Broodwar->getFrameCount()
                || unit.getBuildType() != None
                || unit.getGoal().isValid()
                || unit.getBuildPosition().isValid())
                return;

            // Only proactively pull the closest worker to our defend position
            auto closestWorker = Util::getClosestUnit(Terrain::getDefendPosition(), PlayerState::Self, [&](auto &u) {
                return u.getRole() == Role::Worker && !unit.getGoal().isValid() && (!unit.hasResource() || !unit.getResource().getType().isRefinery()) && !unit.getBuildPosition().isValid();
            });

            auto combatCount = Units::getMyRoleCount(Role::Combat) - (unit.getRole() == Role::Combat ? 1 : 0);
            auto combatWorkersCount =  Units::getMyRoleCount(Role::Combat) - com(Protoss_Zealot) - com(Protoss_Dragoon) - com(Zerg_Zergling) - (unit.getRole() == Role::Combat ? 1 : 0);

            const auto healthyWorker = [&] {

                // Don't pull low shield probes
                if (unit.getType() == Protoss_Probe && unit.getShields() <= 4)
                    return false;

                // Don't pull low health drones
                if (unit.getType() == Zerg_Drone && unit.getHealth() < 20)
                    return false;
                return true;
            };

            // Proactive pulls will result in the worker defending
            const auto proactivePullWorker = [&]() {

                // If this isn't the closest mineral worker to the defend position, don't pull it
                if (unit.getRole() == Role::Worker && unit.shared_from_this() != closestWorker)
                    return false;

                // Protoss
                if (Broodwar->self()->getRace() == Races::Protoss) {
                    int completedDefenders = com(Protoss_Photon_Cannon) + com(Protoss_Zealot);
                    int visibleDefenders = vis(Protoss_Photon_Cannon) + vis(Protoss_Zealot);

                    // If trying to hide tech, pull 1 probe with a Zealot
                    if (!BuildOrder::isRush() && BuildOrder::isHideTech() && combatCount < 2 && completedDefenders > 0)
                        return true;

                    // If trying to FFE, pull based on Cannon/Zealot numbers, or lack of scouting information
                    if (BuildOrder::getCurrentBuild() == "FFE") {
                        if (Strategy::enemyRush() && Strategy::getEnemyOpener() == "4Pool" && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 1)
                            return true;
                        if (Strategy::enemyPressure() && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 2)
                            return true;
                        if (Strategy::enemyRush() && Strategy::getEnemyOpener() == "9Pool" && Util::getTime() > Time(3, 15) && completedDefenders < 3)
                            return combatWorkersCount < 3;
                        if (!Terrain::getEnemyStartingPosition().isValid() && Strategy::getEnemyBuild() == "Unknown" && combatCount < 2 && completedDefenders < 1 && visibleDefenders > 0)
                            return true;
                    }

                    // If trying to 2Gate at our natural, pull based on Zealot numbers
                    else if (BuildOrder::getCurrentBuild() == "2Gate" && BuildOrder::getCurrentOpener() == "Natural") {
                        if (Strategy::enemyRush() && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 1)
                            return true;
                        if (Strategy::enemyPressure() && combatCount < 8 - (2 * completedDefenders) && visibleDefenders >= 2)
                            return true;
                    }

                    // If trying to 1GateCore and scouted 2Gate late, pull workers to block choke when we are ready
                    else if (BuildOrder::getCurrentBuild() == "1GateCore" && Strategy::getEnemyBuild() == "2Gate" && BuildOrder::getCurrentTransition() != "Defensive" && holdChoke) {
                        if (Util::getTime() < Time(3, 30) && combatWorkersCount < 2)
                            return true;
                    }
                }

                // Terran

                // Zerg
                if (Broodwar->self()->getRace() == Races::Zerg) {
                    if (BuildOrder::getCurrentOpener() == "12Pool" && Strategy::getEnemyOpener() == "9Pool" && total(Zerg_Zergling) < 16 && int(Stations::getMyStations().size()) >= 2)
                        return combatWorkersCount < 3;
                }
                return false;
            };

            // Reactive pulls will cause the worker to attack aggresively
            const auto reactivePullWorker = [&]() {

                auto proxyDangerousBuilding = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u.isProxy() && u.getType().isBuilding() && u.canAttackGround();
                });
                auto proxyBuildingWorker = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u.getType().isWorker() && (u.isThreatening() || (proxyDangerousBuilding && u.getType().isWorker() && u.getPosition().getDistance(proxyDangerousBuilding->getPosition()) < 160.0));
                });

                // HACK: Don't pull workers reactively versus vultures
                if (Players::getVisibleCount(PlayerState::Enemy, Terran_Vulture) > 0)
                    return false;
                if (Strategy::getEnemyBuild() == "2Gate" && Strategy::enemyProxy())
                    return false;

                // If we have immediate threats
                if (Players::ZvT() && proxyDangerousBuilding && com(Zerg_Zergling) <= 2)
                    return combatWorkersCount < 6;
                if (Players::ZvP() && proxyDangerousBuilding && Strategy::getEnemyBuild() == "CannonRush" && com(Zerg_Zergling) <= 2)
                    return combatWorkersCount < (4 * Players::getVisibleCount(PlayerState::Enemy, proxyDangerousBuilding->getType()));
                if (Strategy::getEnemyTransition() == "WorkerRush" && com(Zerg_Spawning_Pool) == 0)
                    return Strategy::getWorkersNearUs() >= combatWorkersCount - 3;
                if (BuildOrder::getCurrentOpener() == "12Hatch" && Strategy::getEnemyOpener() == "8Rax" && com(Zerg_Zergling) < 2)
                    return combatWorkersCount <= com(Zerg_Drone) - 4;

                // If we're trying to make our expanding hatchery and the drone is being harassed
                if (vis(Zerg_Hatchery) == 1 && Util::getTime() < Time(3, 00) && BuildOrder::isOpener() && Units::getImmThreat() > 0.0f && Players::ZvP() && combatCount == 0)
                    return combatWorkersCount < 1;
                if (Players::ZvP() && Util::getTime() < Time(4, 00) && !Terrain::isShitMap() && int(Stations::getMyStations().size()) < 2 && BuildOrder::getBuildQueue()[Zerg_Hatchery] >= 2 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Probe) > 0)
                    return combatWorkersCount < 1;

                // If we suspect a cannon rush is coming
                if (Players::ZvP() && Strategy::enemyPossibleProxy() && Util::getTime() < Time(3, 00))
                    return combatWorkersCount < 1;
                return false;
            };

            // Check if workers should fight or work
            if (unit.getType().isWorker()) {
                auto react = reactivePullWorker();
                auto proact = proactivePullWorker();

                // Pull a worker if needed
                if (unit.getRole() == Role::Worker && !unit.unit()->isCarryingMinerals() && !unit.unit()->isCarryingGas() && healthyWorker() && (react || proact)) {
                    unit.setRole(Role::Combat);
                    unit.setBuildingType(None);
                    unit.setBuildPosition(TilePositions::Invalid);
                    lastRoleChange = Broodwar->getFrameCount();
                }

                // Return a worker if not needed
                else if (unit.getRole() == Role::Combat && ((!react && !proact) || !healthyWorker())) {
                    unit.setRole(Role::Worker);
                    lastRoleChange = Broodwar->getFrameCount();
                }

                // HACK: Check if this was a reactive pull, set worker to always engage
                if (unit.getRole() == Role::Combat) {
                    combatCount--;
                    combatWorkersCount--;
                    react = reactivePullWorker();
                    if (react) {
                        unit.setLocalState(LocalState::Attack);
                        unit.setGlobalState(GlobalState::Attack);
                    }
                }
            }
        }

        void updateClusters(UnitInfo& unit)
        {
            // Don't update clusters for fragile combat units
            if (unit.getType() == Protoss_High_Templar
                || unit.getType() == Protoss_Dark_Archon
                || unit.getType() == Protoss_Reaver
                || unit.getType() == Protoss_Interceptor
                || unit.getType() == Zerg_Defiler)
                return;

            // Figure out what type to make the center of our cluster around
            auto clusterAround = UnitTypes::None;
            if (Broodwar->self()->getRace() == Races::Protoss)
                clusterAround = vis(Protoss_Carrier) > 0 ? Protoss_Carrier : Protoss_Corsair;
            else if (Broodwar->self()->getRace() == Races::Zerg)
                clusterAround = vis(Zerg_Guardian) > 0 ? Zerg_Guardian : Zerg_Mutalisk;
            else if (Broodwar->self()->getRace() == Races::Terran)
                clusterAround = vis(Terran_Battlecruiser) > 0 ? Terran_Battlecruiser : Terran_Wraith;

            if (unit.isFlying() && unit.getType() == clusterAround) {
                if (Grids::getAAirCluster(unit.getWalkPosition()) > airCluster.first)
                    airCluster = make_pair(Grids::getAAirCluster(unit.getWalkPosition()), unit.getPosition());
            }
            else if (!unit.isFlying() && unit.getWalkPosition().isValid()) {
                const auto strength = Grids::getAGroundCluster(unit.getWalkPosition()) + Grids::getAAirCluster(unit.getWalkPosition());
                combatClusters.emplace(strength, unit.getPosition());
            }
        }

        void updateLocalState(UnitInfo& unit)
        {
            if (!unit.hasSimTarget()) {
                unit.setLocalState(LocalState::None);
                return;
            }
            const auto distSim = double(Util::boxDistance(unit.getType(), unit.getPosition(), unit.getSimTarget().getType(), unit.getSimTarget().getPosition()));
            const auto insideRetreatRadius = distSim < unit.getRetreatRadius();

            if (!unit.hasTarget()) {
                unit.setLocalState(insideRetreatRadius ? LocalState::Retreat : LocalState::None);
                return;
            }

            if (!checker.expired() && checker.lock() && unit.unit() == checker.lock()->unit()) {
                unit.setLocalState(LocalState::None);
                unit.setGlobalState(GlobalState::Attack);
                return;
            }

            const auto distTarget = double(Util::boxDistance(unit.getType(), unit.getPosition(), unit.getTarget().getType(), unit.getTarget().getPosition()));
            const auto insideEngageRadius = distTarget < unit.getEngageRadius() && unit.getGlobalState() == GlobalState::Attack;

            // HACK: Workers are sometimes forced to engage endlessly
            if (unit.getLocalState() != LocalState::None)
                return;

            const auto atHome = Terrain::isInAllyTerritory(unit.getTarget().getTilePosition()) && mapBWEM.GetArea(unit.getTilePosition()) == mapBWEM.GetArea(unit.getTarget().getTilePosition()) && !Players::ZvZ();
            const auto reAlign = (unit.getType() == Zerg_Mutalisk && unit.hasTarget() && unit.canStartAttack() && !unit.isWithinAngle(unit.getTarget()) && Util::boxDistance(unit.getType(), unit.getPosition(), unit.getTarget().getType(), unit.getTarget().getPosition()) <= 32.0);
            const auto reGroup = unit.getType() == Zerg_Mutalisk && Grids::getAAirCluster(unit.getPosition()) < 6.0f;
            const auto winningState = (!atHome || !BuildOrder::isPlayPassive()) && unit.getSimState() == SimState::Win;

            // Regardless of any decision, determine if Unit is in danger and needs to retreat
            if (Actions::isInDanger(unit, unit.getPosition())
                || (Actions::isInDanger(unit, unit.getEngagePosition()) && insideEngageRadius)
                || reAlign)
                unit.setLocalState(LocalState::Retreat);

            // Regardless of local decision, determine if Unit needs to attack or retreat
            else if (unit.globalEngage())
                unit.setLocalState(LocalState::Attack);
            else if (unit.globalRetreat())
                unit.setLocalState(LocalState::Retreat);

            // If within local decision range, determine if Unit needs to attack or retreat
            else if ((insideEngageRadius || atHome) && (unit.localEngage() || winningState))
                unit.setLocalState(LocalState::Attack);
            else if ((insideRetreatRadius || atHome) && (unit.localRetreat() || unit.getSimState() == SimState::Loss))
                unit.setLocalState(LocalState::Retreat);

            // Default state
            else
                unit.setLocalState(LocalState::None);
        }

        void updateGlobalState(UnitInfo& unit)
        {
            bool testingDefense = false;
            if (testingDefense) {
                holdChoke = true;
                unit.setGlobalState(GlobalState::Retreat);
                return;
            }

            if (unit.getGlobalState() != GlobalState::None)
                return;

            // Protoss
            if (Broodwar->self()->getRace() == Races::Protoss) {
                if ((!BuildOrder::takeNatural() && Strategy::enemyFastExpand())
                    || (Strategy::enemyProxy() && !Strategy::enemyRush())
                    || BuildOrder::isRush()
                    || unit.getType() == Protoss_Dark_Templar
                    || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Dark_Templar) > 0 && com(Protoss_Observer) == 0 && Broodwar->getFrameCount() < 15000))
                    unit.setGlobalState(GlobalState::Attack);

                else if (unit.getType().isWorker()
                    || (Broodwar->getFrameCount() < 15000 && BuildOrder::isPlayPassive())
                    || (unit.getType() == Protoss_Corsair && !BuildOrder::firstReady() && Players::getStrength(PlayerState::Enemy).airToAir > 0.0)
                    || (unit.getType() == Protoss_Carrier && com(Protoss_Interceptor) < 16 && !Strategy::enemyPressure()))
                    unit.setGlobalState(GlobalState::Retreat);
                else
                    unit.setGlobalState(GlobalState::Attack);
            }

            // Zerg
            else if (Broodwar->self()->getRace() == Races::Zerg) {

                // Check if we have units outside our base
                //auto closestStraggler = Util::getFurthestUnit(BWEB::Map::getMainPosition(), PlayerState::Self, [&](auto &u) {
                //    return u.getRole() == Role::Combat && !Terrain::isInAllyTerritory(u.getTilePosition()) && u.hasTarget() && u.getTarget().frameArrivesWhen() < u.frameArrivesWhen();
                //});

                if (BuildOrder::isRush()
                    || Broodwar->getGameType() == GameTypes::Use_Map_Settings)
                    unit.setGlobalState(GlobalState::Attack);
                else if ((Broodwar->getFrameCount() < 15000 && BuildOrder::isPlayPassive())
                    || (Players::ZvT() && Util::getTime() < Time(12, 00) && Util::getTime() > Time(3, 30) && unit.getType() == Zerg_Zergling && !Strategy::enemyGreedy() && (Strategy::getEnemyBuild() == "RaxFact" || Strategy::enemyWalled() || Players::getCompleteCount(PlayerState::Enemy, Terran_Vulture) > 0))
                    || (Players::ZvZ() && Util::getTime() < Time(10, 00) && unit.getType() == Zerg_Zergling && Players::getCompleteCount(PlayerState::Enemy, Zerg_Zergling) > com(Zerg_Zergling))
                    || (Players::ZvZ() && Players::getCompleteCount(PlayerState::Enemy, Zerg_Drone) > 0 && !Terrain::getEnemyStartingPosition().isValid() && Util::getTime() < Time(2, 45))
                    || (BuildOrder::isProxy() && BuildOrder::isPlayPassive())
                    || (BuildOrder::isProxy() && unit.getType() == Zerg_Hydralisk)
                    || (!Players::ZvZ() && unit.isLightAir() && com(Zerg_Mutalisk) < 5 && total(Zerg_Mutalisk) < 9))
                    unit.setGlobalState(GlobalState::Retreat);
                else
                    unit.setGlobalState(GlobalState::Attack);
            }

            // Terran
            else if (Broodwar->self()->getRace() == Races::Terran) {
                if (BuildOrder::isPlayPassive() || !BuildOrder::firstReady())
                    unit.setGlobalState(GlobalState::Retreat);
                else
                    unit.setGlobalState(GlobalState::Attack);
            }
        }

        void updateDestination(UnitInfo& unit)
        {
            // HACK: Proxy lurker stuff
            if (unit.getType() == Zerg_Lurker && BuildOrder::isProxy() && Util::getTime() < Time(12, 00)) {
                unit.setDestination(Terrain::getEnemyMain()->getResourceCentroid());
                return;
            }

            // If attacking and target is close, set as destination
            if (unit.getLocalState() == LocalState::Attack) {
                if (unit.getInterceptPosition().isValid() && unit.getInterceptPosition().getDistance(unit.getTarget().getPosition()) < unit.getInterceptPosition().getDistance(unit.getPosition()) - 16.0 && (Grids::getMobility(unit.getInterceptPosition()) > 0 || unit.isFlying()))
                    unit.setDestination(unit.getInterceptPosition());
                else if (unit.attemptingSurround())
                    unit.setDestination(unit.getSurroundPosition());
                else
                    unit.setDestination(unit.getEngagePosition());

                if (unit.getTargetPath().isReachable())
                    unit.setDestinationPath(unit.getTargetPath());
            }

            // If we're not ready to attack with a proxy yet
            else if (unit.getGlobalState() == GlobalState::Retreat && BuildOrder::isProxy()) {
                const auto closestProxy = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                    return u.getType() == Zerg_Hatchery || u.getType() == Zerg_Lair || u.getType() == Protoss_Gateway || u.getType() == Terran_Barracks;
                });
                if (closestProxy)
                    unit.setDestination(closestProxy->getPosition());
            }

            // If we're globally retreating, set defend position as destination
            else if (unit.getGlobalState() == GlobalState::Retreat && holdChoke) {
                if (unit.isLightAir() || !unit.isHealthy())
                    unit.setDestination(BWEB::Map::getMainPosition());
                else
                    unit.setDestination(Terrain::getDefendPosition());

                if (!unit.isLightAir())
                    unit.setFormation(unit.getDestination());
            }

            // If retreating, find closest retreat position
            else if (unit.getLocalState() == LocalState::Retreat || unit.getGlobalState() == GlobalState::Retreat) {
                const auto &retreat = getClosestRetreatPosition(unit);
                if (retreat.isValid() && (!unit.isLightAir() || Players::getStrength(PlayerState::Enemy).airToAir > 0.0))
                    unit.setDestination(retreat);
                else
                    unit.setDestination(BWEB::Map::getMainPosition());

                if (!unit.isLightAir())
                    unit.setFormation(unit.getDestination());
            }

            // If unit has a goal
            else if (unit.getGoal().isValid())
                unit.setDestination(unit.getGoal());

            // If this is a light air unit, defend any bases under heavy attack
            else if ((unit.isLightAir() || unit.getType() == Zerg_Scourge) && ((Units::getImmThreat() > 25.0 && Stations::getMyStations().size() >= 3 && Stations::getMyStations().size() > Stations::getEnemyStations().size()) || (Players::ZvZ() && Units::getImmThreat() > 5.0))) {
                auto &attacker = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u.isThreatening() && !u.isHidden();
                });
                if (attacker)
                    unit.setDestination(attacker->getPosition());
            }

            // If this is a light air unit, go to the air cluster first if far away
            else if ((unit.isLightAir() || unit.getType() == Zerg_Scourge) && airCommander.lock() && unit.getPosition().getDistance(airCommander.lock()->getPosition()) > 64.0)
                unit.setDestination(airCluster.second);

            // If this is a light air unit and we can harass
            else if (unit.attemptingHarass()) {
                unit.setDestination(Terrain::getHarassPosition());
                unit.setDestinationPath(airClusterPath);
            }

            // If unit has a target and a valid engagement position
            else if (unit.hasTarget()) {
                unit.setDestination(unit.getTarget().getPosition());
                unit.setDestinationPath(unit.getTargetPath());
            }

            // If attack position is valid
            else if (Terrain::getAttackPosition().isValid() && unit.canAttackGround())
                unit.setDestination(Terrain::getAttackPosition());

            // If no target and no enemy bases, move to a base location
            else if (!unit.hasTarget() || !unit.getTarget().getPosition().isValid() || unit.unit()->isIdle()) {

                // Finishing enemy off, find remaining bases we haven't scouted, if we haven't visited in 2 minutes
                if (Terrain::getEnemyStartingPosition().isValid()) {
                    auto best = DBL_MAX;
                    for (auto &area : mapBWEM.Areas()) {
                        for (auto &base : area.Bases()) {
                            if (area.AccessibleNeighbours().size() == 0
                                || Terrain::isInAllyTerritory(base.Location()))
                                continue;

                            int time = Grids::lastVisibleFrame(base.Location());
                            if (time < best) {
                                best = time;
                                unit.setDestination(Position(base.Location()));
                            }
                        }
                    }
                }

                // Need to scout in drone scouting order in ZvZ, ovie order in non ZvZ
                else {
                    if (!combatScoutOrder.empty()) {
                        for (auto &station : combatScoutOrder) {
                            if (!Stations::isBaseExplored(station)) {
                                unit.setDestination(station->getBase()->Center());
                                break;
                            }
                        }
                    }

                    if (combatScoutOrder.size() > 2 && !Stations::isBaseExplored(*(combatScoutOrder.begin() + 1))) {
                        combatScoutOrder.erase(combatScoutOrder.begin());
                    }
                }

                // Finish off positions that are old
                if (Util::getTime() > Time(8, 00)) {
                    if (unit.isFlying() && !airCleanupPositions.empty()) {
                        unit.setDestination(airCleanupPositions.begin()->second);
                        airCleanupPositions.erase(airCleanupPositions.begin());
                    }
                    else if (!unit.isFlying() && !groundCleanupPositions.empty()) {
                        unit.setDestination(groundCleanupPositions.begin()->second);
                        groundCleanupPositions.erase(groundCleanupPositions.begin());
                    }
                }
            }
        }

        void updateFormation(UnitInfo& unit)
        {

        }

        void updatePath(UnitInfo& unit)
        {
            auto simDistCurrent = unit.hasSimTarget() ? unit.getPosition().getApproxDistance(unit.getSimTarget().getPosition()) : 640.0;

            const auto flyerHeuristic = [&](const TilePosition &t) {
                const auto center = Position(t) + Position(16, 16);

                auto d = center.getApproxDistance(unit.getSimTarget().getPosition());
                for (auto &pos : lastSimPositions)
                    d = min(d, center.getApproxDistance(pos));

                auto dist = unit.getSimState() == SimState::Win ? 1.0 : max(0.01, double(d) - min(simDistCurrent, unit.getRetreatRadius() + 64.0));
                auto vis = unit.getSimState() == SimState::Win ? 1.0 : clamp(double(Broodwar->getFrameCount() - Grids::lastVisibleFrame(t)) / 960.0, 0.5, 3.0);
                return 1.00 / (vis * dist);
            };

            const auto flyerRegroup = [&](const TilePosition &t) {
                const auto center = Position(t) + Position(16, 16);
                auto threat = Grids::getEAirThreat(center);
                return threat;
            };

            // Generate a new path that obeys collision of terrain and buildings
            if (!unit.isFlying() && unit.getDestination().isValid() && unit.getDestinationPath() != unit.getTargetPath() && unit.getDestinationPath().getTarget() != TilePosition(unit.getDestination())) {

                // Create a pathpoint
                auto pathPoint = unit.getDestination();
                auto usedTile = BWEB::Map::isUsed(TilePosition(unit.getDestination()));
                if (!BWEB::Map::isWalkable(TilePosition(unit.getDestination()), unit.getType()) || usedTile != UnitTypes::None) {
                    auto dimensions = usedTile != UnitTypes::None ? usedTile.tileSize() : TilePosition(1, 1);
                    auto closest = DBL_MAX;
                    for (int x = TilePosition(unit.getDestination()).x - dimensions.x; x < TilePosition(unit.getDestination()).x + dimensions.x + 1; x++) {
                        for (int y = TilePosition(unit.getDestination()).y - dimensions.y; y < TilePosition(unit.getDestination()).y + dimensions.y + 1; y++) {
                            auto center = Position(TilePosition(x, y)) + Position(16, 16);
                            auto dist = center.getDistance(unit.getPosition());
                            if (dist < closest && BWEB::Map::isWalkable(TilePosition(x, y), unit.getType()) && BWEB::Map::isUsed(TilePosition(x, y)) == UnitTypes::None) {
                                closest = dist;
                                pathPoint = center;
                            }
                        }
                    }
                }

                if (!mapBWEM.GetArea(TilePosition(unit.getPosition())) || !mapBWEM.GetArea(TilePosition(pathPoint)) || mapBWEM.GetArea(TilePosition(unit.getPosition()))->AccessibleFrom(mapBWEM.GetArea(TilePosition(pathPoint)))) {
                    BWEB::Path newPath(unit.getPosition(), pathPoint, unit.getType());
                    newPath.generateJPS([&](const TilePosition &t) { return newPath.unitWalkable(t); });
                    unit.setDestinationPath(newPath);
                }
            }

            // Generate a flying path for harassing that obeys exploration and staying out of range of threats if possible
            auto inCluster = unit.getPosition().getDistance(airCluster.second) < 64.0;
            auto canHarass = unit.getLocalState() != LocalState::Retreat && unit.getDestination() == Terrain::getHarassPosition() && unit.hasSimTarget() && inCluster && unit.attemptingHarass();
            auto enemyPressure = unit.hasTarget() && Util::getTime() < Time(7, 00) && unit.getTarget().getPosition().getDistance(BWEB::Map::getMainPosition()) < unit.getTarget().getPosition().getDistance(Terrain::getEnemyStartingPosition());
            if (unit.isLightAir() && !unit.getGoal().isValid() && canHarass && !enemyPressure) {
                BWEB::Path newPath(unit.getPosition(), unit.getDestination(), unit.getType());
                newPath.generateAS(flyerHeuristic);
                unit.setDestinationPath(newPath);
                //Visuals::drawPath(newPath);
                //Visuals::drawLine(unit.getPosition(), unit.getDestination(), Colors::Green);
            }

            // Generate a flying path for regrouping
            if (unit.isLightAir() && !unit.globalRetreat() && !unit.getGoal().isValid() && !inCluster && airCluster.second.isValid()) {
                BWEB::Path newPath(unit.getPosition(), airCluster.second, unit.getType());
                newPath.generateAS(flyerRegroup);
                unit.setDestinationPath(newPath);
                //Visuals::drawPath(newPath);
            }

            // If path is reachable, find a point n pixels away to set as new destination
            if (unit.getDestinationPath().isReachable()) {
                auto newDestination = Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                    return p.getDistance(unit.getPosition()) >= 64.0;
                });

                if (newDestination.isValid())
                    unit.setDestination(newDestination);

                getBestPathFormationPoint(unit);
            }
        }

        void updateDecision(UnitInfo& unit)
        {
            if (!unit.unit() || !unit.unit()->exists()                                                                                          // Prevent crashes            
                || unit.unit()->isLoaded()
                || unit.unit()->isLockedDown() || unit.unit()->isMaelstrommed() || unit.unit()->isStasised() || !unit.unit()->isCompleted())    // If the unit is locked down, maelstrommed, stassised, or not completed
                return;

            // Convert our commands to strings to display what the unit is doing for debugging
            map<int, string> commandNames{
                make_pair(0, "Misc"),
                make_pair(1, "Special"),
                make_pair(2, "Attack"),
                make_pair(3, "Approach"),
                make_pair(4, "Kite"),
                make_pair(5, "Defend"),
                make_pair(6, "Explore"),
                make_pair(7, "Escort"),
                make_pair(8, "Retreat"),
                make_pair(9, "Move")
            };

            // Iterate commands, if one is executed then don't try to execute other commands
            int height = unit.getType().height() / 2;
            int width = unit.getType().width() / 2;
            int i = Util::iterateCommands(commands, unit);
            auto startText = unit.getPosition() + Position(-4 * int(commandNames[i].length() / 2), height);
            Broodwar->drawTextMap(startText, "%c%s", Text::White, commandNames[i].c_str());
        }

        void updateCleanup()
        {
            groundCleanupPositions.clear();
            airCleanupPositions.clear();

            if (Util::getTime() < Time(6, 00) || !Stations::getEnemyStations().empty())
                return;

            // Look at every TilePosition and sort by furthest oldest
            auto best = 0.0;
            for (int x = 0; x < Broodwar->mapWidth(); x++) {
                for (int y = 0; y < Broodwar->mapHeight(); y++) {
                    auto t = TilePosition(x, y);
                    auto p = Position(t) + Position(16, 16);

                    if (!Broodwar->isBuildable(t))
                        continue;

                    auto frameDiff = (Broodwar->getFrameCount() - Grids::lastVisibleFrame(t));
                    auto dist = p.getDistance(BWEB::Map::getMainPosition());

                    if (mapBWEM.GetArea(t) && mapBWEM.GetArea(t)->AccessibleFrom(BWEB::Map::getMainArea()))
                        groundCleanupPositions.emplace(make_pair(1.0 / (frameDiff * dist), p));
                    else
                        airCleanupPositions.emplace(make_pair(1.0 / (frameDiff * dist), p));
                }
            }
        }

        void updateArmyChecker()
        {
            // Determine if we need to create a new checking unit to try and detect the enemy build
            const auto needEnemyCheck = !Players::ZvZ() && !Strategy::enemyRush() && Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) <= 0 && Strategy::getEnemyTransition() == "Unknown" && Terrain::getEnemyStartingPosition().isValid() && Util::getTime() < Time(6, 00) && Broodwar->getFrameCount() - lastCheckFrame > 240;

            if (needEnemyCheck && checker.expired()) {
                checker = Util::getClosestUnit(Units::getEnemyArmyCenter(), PlayerState::Self, [&](auto &u) {
                    return u.getRole() == Role::Combat && !u.getGoal().isValid() && (u.getType() == Zerg_Zergling || u.getType() == Protoss_Zealot || u.getType() == Terran_Marine || u.getType() == Terran_Vulture);
                });
            }

            // If the checking unit exists and has seen something, it can be released
            else if (checker.lock()) {
                auto sawArmy = Units::getEnemyArmyCenter().isValid() && Broodwar->getFrameCount() - Grids::lastVisibleFrame(TilePosition(Units::getEnemyArmyCenter())) < 120;
                auto sawTarget = checker.lock()->hasTarget() && !checker.lock()->getTarget().getType().isWorker() && (checker.lock()->getTarget().unit()->exists() || checker.lock()->getTarget().getType().isBuilding());
                if (sawArmy || sawTarget || checker.lock()->getGoal().isValid()) {
                    checker.reset();
                    lastCheckFrame = Broodwar->getFrameCount();
                }
            }
        }

        void updateAirCommander()
        {
            // Get an air commander if new one needed
            if (airCommander.expired() || airCommander.lock()->globalRetreat() || airCommander.lock()->localRetreat() || (airCluster.second.isValid() && airCommander.lock()->getPosition().getDistance(airCluster.second) > 64.0)) {
                airCommander = Util::getClosestUnit(airCluster.second, PlayerState::Self, [&](auto &u) {
                    return u.isLightAir() && !u.localRetreat() && !u.globalRetreat() && !u.getGoal().isValid();
                });
            }

            // If we have an air commander
            if (airCommander.lock()) {
                if (airCommander.lock()->hasSimTarget() && find(lastSimPositions.begin(), lastSimPositions.end(), airCommander.lock()->getSimTarget().getPosition()) == lastSimPositions.end()) {
                    if (lastSimPositions.size() >= 5)
                        lastSimPositions.pop_back();
                    lastSimPositions.insert(lastSimPositions.begin(), airCommander.lock()->getSimTarget().getPosition());
                }

                // Execute the air commanders commands
                Horizon::simulate(*airCommander.lock());
                updateDestination(*airCommander.lock());
                updatePath(*airCommander.lock());
                updateDecision(*airCommander.lock());

                // Setup air commander commands for other units to follow
                airClusterPath = airCommander.lock()->getDestinationPath();
                airCommanderCommand = make_pair(airCommander.lock()->getCommandType(), airCommander.lock()->getCommandPosition());
            }
        }

        void sortCombatUnits()
        {
            // Sort units by distance to destination
            airCluster.first = 0.0;
            airCluster.second = Positions::Invalid;
            combatClusters.clear();
            combatUnitsByDistance.clear();
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;

                // Don't update if
                if (!unit.unit()->isCompleted()
                    || unit.getType() == Terran_Vulture_Spider_Mine
                    || unit.getType() == Protoss_Scarab
                    || unit.getType() == Protoss_Interceptor
                    || unit.getType().isSpell())
                    continue;

                // Check if we need to pull/push workers to/from combat role
                if (unit.getType().isWorker())
                    updateRole(unit);

                // Update combat role units states and sort by distance to destination
                if (unit.getRole() == Role::Combat) {
                    updateClusters(unit);
                    updateGlobalState(unit);
                    updateLocalState(unit);
                    Horizon::simulate(unit);
                    updateDestination(unit);
                    updateFormation(unit);
                    combatUnitsByDistance.emplace(unit.getPosition().getDistance(unit.getDestination()), unit);
                }
            }
        }

        void updateUnits() {
            combatScoutOrder = Scouts::getScoutOrder(Zerg_Zergling);
            updateArmyChecker();
            sortCombatUnits();
            updateAirCommander();
            assignFormations();

            // Execute commands ordered by ascending distance
            for (auto &u : combatUnitsByDistance) {
                auto &unit = u.second;

                if (airCommander.lock() && unit == *airCommander.lock())
                    continue;

                // Light air close to the air cluster use the same command of the air commander
                if (airCommander.lock() && !unit.localRetreat() && !unit.globalRetreat() && unit.isLightAir() && !airCommander.lock()->isNearSplash() && !unit.isNearSplash() && !airCommander.lock()->isNearSuicide() && !unit.isNearSuicide() && unit.getPosition().getDistance(airCommander.lock()->getPosition()) <= 96.0) {

                    Horizon::simulate(unit);
                    updateDestination(unit);

                    auto percentMoveCloser = clamp(unit.getPosition().getDistance(airCommander.lock()->getPosition()) / 96.0, 0.0, 1.0);
                    if (unit.hasTarget() && (airCommanderCommand.first == UnitCommandTypes::Attack_Unit || (Broodwar->getFrameCount() - airCommander.lock()->getLastAttackFrame() < 8 && unit.canStartAttack())))
                        unit.command(UnitCommandTypes::Attack_Unit, unit.getTarget());
                    else if (airCommanderCommand.first == UnitCommandTypes::Move && !unit.isTargetedBySplash()) {
                        auto positionx = (airCommanderCommand.second.x * (1.0 - percentMoveCloser)) + (airCommander.lock()->getPosition().x * percentMoveCloser);
                        auto positiony = (airCommanderCommand.second.y * (1.0 - percentMoveCloser)) + (airCommander.lock()->getPosition().y * percentMoveCloser);
                        unit.command(UnitCommandTypes::Move, Position(positionx, positiony));
                    }
                    else if (airCommanderCommand.first == UnitCommandTypes::Right_Click_Position && !unit.isTargetedBySplash())
                        unit.command(UnitCommandTypes::Right_Click_Position, airCommanderCommand.second);
                    else
                        updateDecision(unit);
                }

                // Combat unit decisions
                else if (unit.getRole() == Role::Combat) {
                    updatePath(unit);
                    updateDecision(unit);
                }
            }
        }

        void updateRetreatPositions()
        {
            retreatPositions.clear();

            if (Terrain::getDefendChoke() == BWEB::Map::getMainChoke()) {
                retreatPositions.insert(Terrain::getMyMain()->getResourceCentroid());
                return;
            }

            for (auto &station : Stations::getMyStations()) {

                auto wallDefending = false;
                if (station->getChokepoint()) {
                    auto wall = BWEB::Walls::getWall(station->getChokepoint());
                    if (wall && wall->getGroundDefenseCount() >= 2)
                        wallDefending = true;
                }

                // A non main station cannot be a retreat point if an enemy is within reach
                if (!station->isMain() && !wallDefending && Stations::getGroundDefenseCount(station) < 2) {
                    const auto closestEnemy = Util::getClosestUnitGround(station->getBase()->Center(), PlayerState::Enemy, [&](auto &u) {
                        return u.canAttackGround();
                    });

                    if (closestEnemy && closestEnemy->getPosition().getDistance(station->getBase()->Center()) < closestEnemy->getGroundReach() * 2.0)
                        continue;
                }

                // Store the defending position for this station
                auto defendPosition = Stations::getDefendPosition(station);
                if (defendPosition.isValid())
                    retreatPositions.insert(defendPosition);

            }
        }

        void updateDefenders()
        {
            // Update all my buildings
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;

                if (unit.getRole() == Role::Defender)
                    updateDecision(unit);
            }
        }

        void checkHoldChoke()
        {
            auto defensiveAdvantage = (Players::ZvZ() && BuildOrder::getCurrentOpener() == Strategy::getEnemyOpener()) || (Players::ZvZ() && BuildOrder::getCurrentOpener() == "12Pool" && Strategy::getEnemyOpener() == "9Pool");

            // UMS Setting
            if (Broodwar->getGameType() == BWAPI::GameTypes::Use_Map_Settings) {
                holdChoke = true;
                return;
            }

            // Protoss
            if (Broodwar->self()->getRace() == Races::Protoss && Players::getSupply(PlayerState::Self, Races::None) > 40) {
                holdChoke = BuildOrder::takeNatural()
                    || vis(Protoss_Dragoon) > 0
                    || com(Protoss_Shield_Battery) > 0
                    || BuildOrder::isWallNat()
                    || (BuildOrder::isHideTech() && !Strategy::enemyRush())
                    || Players::getSupply(PlayerState::Self, Races::None) > 60
                    || Players::vT();
            }

            // Terran
            if (Broodwar->self()->getRace() == Races::Terran && Players::getSupply(PlayerState::Self, Races::None) > 40)
                holdChoke = true;

            // Zerg
            if (Broodwar->self()->getRace() == Races::Zerg) {
                holdChoke = (!Players::ZvZ() && (Strategy::getEnemyBuild() != "2Gate" || !Strategy::enemyProxy()))
                    || (defensiveAdvantage && !Strategy::enemyPressure() && vis(Zerg_Sunken_Colony) == 0 && com(Zerg_Mutalisk) < 3 && Util::getTime() > Time(3, 30))
                    || (BuildOrder::takeNatural() && total(Zerg_Zergling) >= 10)
                    || Players::getSupply(PlayerState::Self, Races::None) > 60;
            }
        }
    }

    void onStart()
    {
        if (!BWEB::Map::getMainChoke())
            return;

        const auto createCache = [&](const BWEM::ChokePoint * chokePoint, const BWEM::Area * area) {
            auto center = chokePoint->Center();
            for (int x = center.x - 50; x <= center.x + 50; x++) {
                for (int y = center.y - 50; y <= center.y + 50; y++) {
                    WalkPosition w(x, y);
                    const auto p = Position(w) + Position(4, 4);

                    if (!p.isValid()
                        || (area && mapBWEM.GetArea(w) != area)
                        || Grids::getMobility(w) < 6)
                        continue;

                    auto closest = Util::getClosestChokepoint(p);
                    if (closest != chokePoint && p.getDistance(Position(closest->Center())) < 96.0 && (closest == BWEB::Map::getMainChoke() || closest == BWEB::Map::getNaturalChoke()))
                        continue;

                    concaveCache[chokePoint].push_back(w);
                }
            }
        };

        // Main area for defending sometimes is wrong like Andromeda and Polaris Rhapsody
        const BWEM::Area * defendArea = nullptr;
        auto &[a1, a2] = BWEB::Map::getMainChoke()->GetAreas();
        if (a1 && Terrain::isInAllyTerritory(a1))
            defendArea = a1;
        if (a2 && Terrain::isInAllyTerritory(a2))
            defendArea = a2;

        createCache(BWEB::Map::getMainChoke(), defendArea);
        createCache(BWEB::Map::getMainChoke(), BWEB::Map::getMainArea());

        // Natural area should always be correct
        createCache(BWEB::Map::getNaturalChoke(), BWEB::Map::getNaturalArea());
    }

    void onFrame() {
        Visuals::startPerfTest();

        checkHoldChoke();
        updateCleanup();
        updateUnits();
        updateDefenders();
        updateRetreatPositions();
        updateFormations();
        Visuals::endPerfTest("Combat");
    }

    Position getClosestRetreatPosition(UnitInfo& unit)
    {
        auto distBest = DBL_MAX;
        auto posBest = BWEB::Map::getMainPosition();
        for (auto &position : retreatPositions) {

            // Check if anything targeting this unit is withing reach
            bool withinTargeterReach = false;
            for (auto &t : unit.getTargetedBy()) {
                if (auto targeter = t.lock()) {
                    auto reach = unit.isFlying() ? targeter->getAirReach() : targeter->getGroundReach();
                    if (targeter->getPosition().getDistance(position) < reach)
                        withinTargeterReach = true;
                    if (targeter->getPosition().getDistance(position) < unit.getPosition().getDistance(position))
                        withinTargeterReach = true;
                }
            }

            auto dist = position.getDistance(unit.getPosition());
            if (dist < distBest && !withinTargeterReach) {
                posBest = position;
                distBest = dist;
            }
        }
        return posBest;
    }

    bool defendChoke() { return holdChoke; }
    set<Position>& getDefendPositions() { return defendPositions; }
    multimap<double, Position>& getCombatClusters() { return combatClusters; }
    Position getAirClusterCenter() { return airCluster.second; }
}