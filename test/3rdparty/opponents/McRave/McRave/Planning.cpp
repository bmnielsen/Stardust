#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Planning {
    namespace {

        int plannedMineral, plannedGas;
        map<TilePosition, UnitType> buildingsPlanned;
        map<TilePosition, int> buildingTimer;
        set<TilePosition> validDefenses, plannedGround, plannedAir;
        bool expansionPlanned = false;
        BWEB::Station * currentExpansion = nullptr;
        BWEB::Station * nextExpansion = nullptr;
        vector<TilePosition> unreachablePositions;

        UnitInfo* getBuilder(UnitType building, Position here)
        {
            auto builder = Util::getClosestUnitGround(here, PlayerState::Self, [&](auto &u) {
                if (u->getType().getRace() != building.getRace()
                    || u->getBuildType() != None
                    || u->unit()->getOrder() == Orders::ConstructingBuilding
                    || !Workers::canAssignToBuild(*u))
                    return false;
                return true;
            });
            return builder;
        }

        bool isPathable(UnitType building, TilePosition here) { return find(unreachablePositions.begin(), unreachablePositions.end(), here) == unreachablePositions.end(); }

        bool creepOrPowerReadyOnArrival(UnitType building, TilePosition here, UnitInfo& builder)
        {
            const auto adjacentToHatch = [&](auto &hatch) {
                return (here.x - hatch.x >= -2 && here.x - hatch.x <= 4 && here.y - hatch.y >= -2 && here.y - hatch.y <= 3);
            };

            // TODO: Impl
            if (building.requiresPsi()) {

            }

            if (building.requiresCreep()) {

                // Check if a hatchery that is adjacent to this position will finish soon
                for (auto &u : Units::getUnits(PlayerState::Self)) {
                    auto &unit = *u;
                    if (unit.getType() == Zerg_Hatchery && adjacentToHatch(unit.getTilePosition())) {
                        auto builderArriveFrame = builder.getPosition().getDistance(unit.getPosition()) / builder.getSpeed();
                        if (builderArriveFrame > unit.frameCompletesWhen())
                            return true;
                    }
                }

                // Check if there is full creep on this position
                for (int x = here.x; x < here.x + building.tileWidth(); x++) {
                    for (int y = here.y; y < here.y + building.tileHeight(); y++) {
                        TilePosition t(x, y);
                        if (!Broodwar->hasCreep(t))
                            return false;
                    }
                }
                return true;
            }
            return false;
        }

        bool overlapsLarvaHistory(UnitType building, TilePosition here)
        {
            auto center = Position(here) + Position(building.tileWidth() * 16, building.tileHeight() * 16);
            for (auto &pos : Buildings::getLarvaPositions()) {
                if (!pos.isValid())
                    continue;
                if (Util::boxDistance(Zerg_Larva, pos, building, center) <= 0)
                    return true;
                if (Util::boxDistance(Zerg_Egg, pos, building, center) <= 0)
                    return true;
            }
            return false;
        }        

        bool isPlannable(UnitType building, TilePosition here)
        {
            // Check if we have specifically marked this position invalid
            if (isDefensiveType(building) && validDefenses.find(here) == validDefenses.end())
                return false;

            // Check if there's a building queued there already
            for (auto &queued : buildingsPlanned) {
                if (queued.first == here)
                    return false;
            }
            return true;
        }

        bool isBuildable(UnitType building, TilePosition here)
        {
            auto center = Position(here) + Position(building.tileWidth() * 16, building.tileHeight() * 16);
            auto creepFully = true;

            // See if it's being blocked
            auto closestEnemy = Util::getClosestUnit(center, PlayerState::Enemy, [&](auto &u) {
                return !u->isFlying() && u->getType() != Terran_Vulture_Spider_Mine;
            });
            if (closestEnemy && Util::boxDistance(closestEnemy->getType(), closestEnemy->getPosition(), building, center) < 32.0)
                return false;

            // Refinery only on Geysers
            if (building.isRefinery()) {
                for (auto &g : Resources::getMyGas()) {
                    ResourceInfo &gas = *g;

                    if (here == gas.getTilePosition() && gas.getType() != building)
                        return true;
                }
                return false;
            }

            // Used tile check
            for (int x = here.x; x < here.x + building.tileWidth(); x++) {
                for (int y = here.y; y < here.y + building.tileHeight(); y++) {
                    TilePosition t(x, y);
                    if (!t.isValid())
                        return false;
                    if (BWEB::Map::isUsed(t) != None)
                        return false;
                    if (!Broodwar->isBuildable(t))
                        return false;
                }
            }

            // Addon room check
            if (building.canBuildAddon()) {
                if (BWEB::Map::isUsed(here + TilePosition(4, 1)) != None)
                    return false;
            }

            // Psi check
            if (building.requiresPsi() && !Pylons::hasPowerSoon(here, building))
                return false;

            // Creep check
            if (building.requiresCreep()) {
                auto builder = getBuilder(building, center);
                return builder && creepOrPowerReadyOnArrival(building, here, *builder);
            }

            return true;
        }

        TilePosition returnFurthest(UnitType building, set<TilePosition> placements, Position desired, bool purelyFurthest = false) {
            auto tileBest = TilePositions::Invalid;
            auto distBest = 0.0;
            for (auto &placement : placements) {
                auto center = Position(placement) + Position(building.tileWidth() * 16, building.tileHeight() * 16);
                auto current = center.getDistance(desired);
                if (current > distBest && isPlannable(building, placement) && isBuildable(building, placement) && isPathable(building, placement)) {
                    distBest = current;
                    tileBest = placement;
                }
            }
            return tileBest;
        }

        TilePosition returnClosest(UnitType building, set<TilePosition> placements, Position desired, bool purelyClosest = false) {
            auto tileBest = TilePositions::Invalid;
            auto distBest = DBL_MAX;

            for (auto &placement : placements) {

                Visuals::drawBox(placement, placement + TilePosition(1, 1), Colors::Yellow);

                auto center = Position(placement) + Position(building.tileWidth() * 16, building.tileHeight() * 16);
                auto current = center.getDistance(desired);
                if (!purelyClosest && !Pylons::hasPowerNow(placement, building))
                    current = current * 32.0;

                if (overlapsLarvaHistory(building, placement)) {
                    if (Util::getTime() < Time(6, 00))
                        continue;
                    current = current * 4.0;
                }

                if (!isPathable(building, placement))
                    Visuals::drawBox(placement, placement + TilePosition(2, 2), Colors::Red);

                if (current < distBest && isPlannable(building, placement) && (isBuildable(building, placement) || purelyClosest) && isPathable(building, placement)) {
                    distBest = current;
                    tileBest = placement;
                }
            }
            return tileBest;
        }

        TilePosition furthestLocation(UnitType building, Position here)
        {
            // Arrange what set we need to check
            set<TilePosition> placements;
            for (auto &block : BWEB::Blocks::getBlocks()) {
                Position blockCenter = Position(block.getTilePosition()) + Position(block.width() * 16, block.height() * 16);
                if (!Terrain::inTerritory(PlayerState::Self, block.getCenter()))
                    continue;

                // Not sure what this is for
                if (Broodwar->self()->getRace() != Races::Zerg && int(block.getMediumTiles().size()) < 2)
                    continue;

                // Setup placements
                if (building.tileWidth() == 4)
                    placements.insert(block.getLargeTiles().begin(), block.getLargeTiles().end());
                else if (building.tileWidth() == 3)
                    placements.insert(block.getMediumTiles().begin(), block.getMediumTiles().end());
                else
                    placements.insert(block.getSmallTiles().begin(), block.getSmallTiles().end());
            }
            return returnFurthest(building, placements, here);
        }

        TilePosition closestLocation(UnitType building, Position here)
        {
            TilePosition tileBest = TilePositions::Invalid;
            double distBest = DBL_MAX;

            // Refineries are only built on my own gas resources
            if (building.isRefinery()) {
                for (auto &g : Resources::getMyGas()) {
                    auto &gas = *g;
                    auto dist = BWEB::Map::getGroundDistance(gas.getPosition(), here);

                    if (Stations::ownedBy(gas.getStation()) != PlayerState::Self
                        || !isPlannable(building, gas.getTilePosition())
                        || !isBuildable(building, gas.getTilePosition())
                        || dist >= distBest)
                        continue;

                    distBest = dist;
                    tileBest = gas.getTilePosition();
                }
                return tileBest;
            }

            // Check if any secondary locations are available here
            auto closestStation = Stations::getClosestStationAir(here, PlayerState::Self);
            if (closestStation && building == Zerg_Hatchery) {
                for (auto &location : closestStation->getSecondaryLocations()) {
                    if (isBuildable(building, location) && isPlannable(building, location) && isPathable(building, location)) {
                        tileBest = location;
                        return tileBest;
                    }
                }
            }

            // Generate a list of Blocks sorted by closest to here
            auto &list = BWEB::Blocks::getBlocks();
            multimap<double, BWEB::Block*> listByDist;
            for (auto &block : list) {
                if (!closestStation
                    || (!Terrain::inTerritory(PlayerState::Self, block.getCenter()) && !BuildOrder::isProxy()))
                    continue;

                // Zerg
                if (Broodwar->self()->getRace() == Races::Zerg) {
                    if ((!Terrain::isIslandMap() && Broodwar->getGroundHeight(TilePosition(closestStation->getBase()->Center())) != Broodwar->getGroundHeight(TilePosition(block.getCenter())))
                        || (closestStation->isNatural() && closestStation->getChokepoint() && Position(closestStation->getChokepoint()->Center()).getDistance(block.getCenter()) < Position(closestStation->getChokepoint()->Center()).getDistance(closestStation->getBase()->Center()))
                        || (closestStation->getBase()->GetArea() != mapBWEM.GetArea(TilePosition(block.getCenter()))))
                        continue;
                }

                // Protosss
                if (Broodwar->self()->getRace() == Races::Protoss) {
                    if (building == Protoss_Pylon) {
                        if ((Pylons::countPoweredPositions(Protoss_Gateway) < 2 && block.getLargeTiles().empty())
                            || (Pylons::countPoweredPositions(Protoss_Forge) < 2 && block.getMediumTiles().empty()))
                            continue;

                        if (!block.getPlacements(building).empty()) {
                            listByDist.emplace(make_pair(block.getCenter().getDistance(here) / (1 + block.getLargeTiles().size() + block.getMediumTiles().size()), &block));
                            continue;
                        }
                    }
                }

                if (!block.getPlacements(building).empty())
                    listByDist.emplace(make_pair(block.getCenter().getDistance(here), &block));
            }

            // Iterate sorted list and find a suitable block
            for (auto &[_, block] : listByDist) {
                tileBest = returnClosest(building, block->getPlacements(building), here);
                if (tileBest.isValid())
                    return tileBest;
            }
            return tileBest;
        }

        bool findResourceDepotLocation(UnitType building, TilePosition& placement)
        {
            const auto baseType = Broodwar->self()->getRace().getResourceDepot();
            const auto hatchCount = vis(Zerg_Hatchery) + vis(Zerg_Lair) + vis(Zerg_Hive);

            // If we are expanding, it must be on an expansion area and be when build order requests one
            const auto expand = Broodwar->self()->getRace() != Races::Zerg
                || BuildOrder::shouldExpand()
                || (int(Stations::getStations(PlayerState::Self).size()) <= 1 && BuildOrder::takeNatural() && !BuildOrder::shouldRamp())
                || (int(Stations::getStations(PlayerState::Self).size()) <= 2 && BuildOrder::takeThird() && !BuildOrder::shouldRamp());

            if (!building.isResourceDepot()
                || !expand
                || expansionPlanned
                || BuildOrder::isProxy())
                return false;

            // First expansion is always the Natural
            if (Stations::getStations(PlayerState::Self).size() == 1 && isBuildable(baseType, BWEB::Map::getNaturalTile()) && isPlannable(baseType, BWEB::Map::getNaturalTile()) && isPathable(building, BWEB::Map::getNaturalTile())) {
                placement = BWEB::Map::getNaturalTile();
                currentExpansion = Terrain::getMyNatural();
                expansionPlanned = true;
                return true;
            }

            // Consult expansion plan for next expansion
            if (currentExpansion) {
                placement = currentExpansion->getBase()->Location();
                expansionPlanned = true;
                return true;
            }
            return false;
        }

        bool findProductionLocation(UnitType building, TilePosition& placement)
        {
            if (!isProductionType(building))
                return false;

            // Figure out where we need to place a production building
            for (auto &[_, station] : Stations::getStationsByProduction()) {
                auto desiredCenter = station->getBase()->Center();
                placement = closestLocation(building, desiredCenter);
                if (placement.isValid())
                    return true;
            }
            return false;
        }

        bool findTechLocation(UnitType building, TilePosition& placement)
        {
            // Don't place if not tech building
            if (!isTechType(building))
                return false;

            // Hide tech if needed or against a rush
            if ((BuildOrder::isHideTech() && (building == Protoss_Citadel_of_Adun || building == Protoss_Templar_Archives))
                || (Spy::enemyRush() && (Players::PvZ() || Players::TvZ())))
                placement = furthestLocation(building, (Position)BWEB::Map::getMainChoke()->Center());

            // Try to place the tech building inside a main base
            for (auto &station : Stations::getStations(PlayerState::Self)) {
                if (station->isMain()) {
                    auto desiredCenter = Players::ZvZ() ? station->getResourceCentroid() : station->getBase()->Center();
                    placement = closestLocation(building, desiredCenter);
                    if (placement.isValid())
                        return true;
                }
            }

            // Try to place the tech building anywhere
            for (auto &station : Stations::getStations(PlayerState::Self)) {
                if (!station->isMain()) {
                    placement = closestLocation(building, station->getBase()->Center());
                    if (placement.isValid())
                        return true;
                }
            }

            return placement.isValid();
        }

        bool findDefenseLocation(UnitType building, TilePosition& placement)
        {
            // Don't place if not defensive building
            if (!isDefensiveType(building))
                return false;

            // Place spire as close to the Lair in case we're hiding it
            if (building == Zerg_Spire) {
                auto closestLair = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Self, [&](auto &u) {
                    return u->getType() == Zerg_Lair;
                });

                if (closestLair) {
                    auto closestStation = Stations::getClosestStationAir(closestLair->getPosition(), PlayerState::Self);
                    if (closestStation) {
                        placement = returnFurthest(building, closestStation->getDefenses(), Position(BWEB::Map::getMainChoke()->Center()));
                        if (placement.isValid())
                            return true;
                    }
                }

                placement = returnFurthest(building, Terrain::getMyMain()->getDefenses(), Position(BWEB::Map::getMainChoke()->Center()));
                if (placement.isValid())
                    return true;
            }

            // Defense placements near stations
            for (auto &station : Stations::getStations(PlayerState::Self)) {

                int colonies = 0;
                for (auto& tile : station->getDefenses()) {
                    if (BWEB::Map::isUsed(tile) == Zerg_Creep_Colony || buildingsPlanned.find(tile) != buildingsPlanned.end())
                        colonies++;
                }

                // If we need ground defenses
                if (Stations::needGroundDefenses(station) > colonies) {
                    if (station->isMain()) {
                        Position desiredCenter = Position(station->getResourceCentroid());
                        placement = returnClosest(building, station->getDefenses(), desiredCenter);
                        if (placement.isValid())
                            return true;
                    }
                    else {

                        // Sort Chokepoints by most vulnerable (closest to middle of map)
                        map<double, Position> chokesByDist;
                        for (auto &choke : station->getBase()->GetArea()->ChokePoints())
                            chokesByDist.emplace(Position(choke->Center()).getDistance(mapBWEM.Center()), Position(choke->Center()));

                        // Place a uniquely best position near each chokepoint if possible
                        for (auto &[_, choke] : chokesByDist) {
                            placement = returnClosest(building, station->getDefenses(), choke, true);
                            if (placement.isValid() && isBuildable(building, placement) && isPathable(building, placement))
                                return true;
                            placement = TilePositions::Invalid;
                        }

                        // Otherwise resort to closest to resource centroid
                        placement = returnClosest(building, station->getDefenses(), station->getResourceCentroid());
                        if (placement.isValid() && isBuildable(building, placement) && isPathable(building, placement))
                            return true;
                        placement = TilePositions::Invalid;
                    }
                }
                if (Stations::needAirDefenses(station) > colonies) {
                    placement = returnClosest(building, station->getDefenses(), Position(station->getResourceCentroid()));
                    if (placement.isValid())
                        return true;
                }
            }

            // Defense placements near walls
            for (auto &[_, wall] : BWEB::Walls::getWalls()) {
                auto desiredCenter = Position(wall.getChokePoint()->Center());
                auto closestMain = BWEB::Stations::getClosestMainStation(TilePosition(wall.getChokePoint()->Center()));

                // How to dictate first placement position
                if (wall.getGroundDefenseCount() < 2) {
                    if (closestMain && closestMain->getChokepoint() && wall.getStation() && Players::ZvT())
                        desiredCenter = (Position(closestMain->getChokepoint()->Center()) + Position(wall.getStation()->getBase()->Center())) / 2;
                    else if (wall.getStation())
                        desiredCenter = Position(wall.getStation()->getBase()->Center());
                }

                // How to dictate row order
                vector<int> desiredRowOrder ={ 3, 4, 2, 1 };

                auto closestDefense = Util::getClosestUnit(Position(closestMain->getChokepoint()->Center()), PlayerState::Self, [&](auto &u) {
                    return isDefensiveType(u->getType());
                });
                if (closestDefense) {
                    desiredCenter = closestDefense->getPosition();
                    for (int i = 4; i >= 1; i--) {
                        if (wall.getDefenses(i).find(closestDefense->getTilePosition()) != wall.getDefenses(i).end()) {
                            if (i == 1)
                                desiredRowOrder.insert(desiredRowOrder.begin(), { i, i + 1 });
                            else
                                desiredRowOrder.insert(desiredRowOrder.begin(), { i, i + 1, i - 1 });
                        }
                    }
                }

                int colonies = 0;
                for (auto& tile : wall.getDefenses()) {
                    if (BWEB::Map::isUsed(tile) == Zerg_Creep_Colony || buildingsPlanned.find(tile) != buildingsPlanned.end())
                        colonies++;
                }

                // If this wall needs defenses
                if (Walls::needGroundDefenses(wall) > colonies) {

                    // Try to place in adjacent rows as existing defenses
                    if (!desiredRowOrder.empty()) {
                        for (auto i : desiredRowOrder) {
                            placement = returnClosest(building, wall.getDefenses(i), desiredCenter);
                            if (placement.isValid()) {
                                plannedGround.insert(placement);
                                return true;
                            }
                        }
                    }

                    // Resort to just placing a defense in the wall
                    else {
                        placement = returnClosest(building, wall.getDefenses(0), desiredCenter);
                        if (placement.isValid()) {
                            plannedGround.insert(placement);
                            return true;
                        }
                    }
                }

                if (Walls::needAirDefenses(wall) > colonies) {

                    // Try to always place in middle rows first
                    for (int i = 2; i <= 3; i++) {
                        placement = returnClosest(building, wall.getDefenses(i), desiredCenter);
                        if (placement.isValid()) {
                            plannedAir.insert(placement);
                            return true;
                        }
                    }

                    // Resort to just placing a defense in the wall
                    placement = returnClosest(building, wall.getDefenses(0), desiredCenter);
                    if (placement.isValid()) {
                        plannedAir.insert(placement);
                        return true;
                    }
                }
            }
            return false;
        }

        bool findWallLocation(UnitType building, TilePosition& placement)
        {
            // Don't place if not wall building
            if (!isWallType(building))
                return false;

            // Don't wall if not needed
            if ((!BuildOrder::isWallNat() && !BuildOrder::isWallMain()) || (Spy::enemyBust() && BuildOrder::isOpener()))
                return false;

            // As Zerg, we have to place natural hatch before wall
            if (building == Zerg_Hatchery && isPlannable(building, BWEB::Map::getNaturalTile()) && BWEB::Map::isUsed(BWEB::Map::getNaturalTile()) == UnitTypes::None)
                return false;

            // Find a wall location
            set<TilePosition> placements;
            for (auto &[_, wall] : BWEB::Walls::getWalls()) {

                if (!Terrain::inTerritory(PlayerState::Self, wall.getArea()))
                    continue;

                // Setup placements
                if (building.tileWidth() == 4)
                    placements.insert(wall.getLargeTiles().begin(), wall.getLargeTiles().end());
                else if (building.tileWidth() == 3)
                    placements.insert(wall.getMediumTiles().begin(), wall.getMediumTiles().end());
                else
                    placements.insert(wall.getSmallTiles().begin(), wall.getSmallTiles().end());
            }

            // Get closest placement
            auto desired = !Stations::getStationsBySaturation().empty() ? Stations::getStationsBySaturation().begin()->second->getBase()->Center() : BWEB::Map::getMainPosition();
            placement = returnClosest(building, placements, BWEB::Map::getMainPosition());
            return placement.isValid();
        }

        bool findProxyLocation(UnitType building, TilePosition& placement)
        {
            if (!BuildOrder::isOpener() || !BuildOrder::isProxy())
                return false;

            // Find our proxy block
            for (auto &block : BWEB::Blocks::getBlocks()) {
                auto blockCenter = Position(block.getTilePosition() + TilePosition(block.width(), block.height()));
                if (block.isProxy()) {
                    placement = closestLocation(building, blockCenter);
                    return placement.isValid();
                }
            }

            // Otherwise try to find something close to the center and hopefully don't mess up - TODO: Don't mess up better
            placement = closestLocation(building, mapBWEM.Center());
            return placement.isValid();
        }

        bool findBatteryLocation(TilePosition& placement)
        {
            return false;
        }

        bool findPylonLocation(UnitType building, TilePosition& placement)
        {
            if (building != Protoss_Pylon)
                return false;

            // Pylon is separated because we want one unique best buildable position to check, rather than next best buildable position
            const auto stationNeedsPylon = [&](BWEB::Station * station) {
                auto distBest = DBL_MAX;
                auto tileBest = TilePositions::Invalid;
                auto natOrMain = station->isMain() || station->isNatural();

                // Check all defense locations for this station
                for (auto &defense : station->getDefenses()) {
                    double dist = Position(defense).getDistance(Position(station->getResourceCentroid()));
                    if (dist < distBest && (natOrMain || (defense.y <= station->getBase()->Location().y + 1 && defense.y >= station->getBase()->Location().y))) {
                        distBest = dist;
                        tileBest = defense;
                    }
                }
                if (isBuildable(building, tileBest) && isPlannable(building, tileBest) && isPathable(building, tileBest))
                    return tileBest;
                return TilePositions::Invalid;
            };

            // Check if any Nexus needs a Pylon for defense placement
            if (com(Protoss_Pylon) >= (Players::vT() ? 5 : 3) || Spy::getEnemyTransition() == "2HatchMuta" || Spy::getEnemyTransition() == "3HatchMuta") {
                for (auto &station : Stations::getStations(PlayerState::Self)) {
                    placement = stationNeedsPylon(station);
                    if (placement.isValid())
                        return true;
                }
            }

            // Check if this our second Pylon and we're hiding tech
            if (building == Protoss_Pylon && vis(Protoss_Pylon) == 2 && BuildOrder::isHideTech()) {
                placement = furthestLocation(building, (Position)BWEB::Map::getMainChoke()->Center());
                if (placement.isValid())
                    return true;
            }

            // Check if any buildings lost power
            if (!Buildings::getUnpoweredPositions().empty()) {
                for (auto &tile : Buildings::getUnpoweredPositions())
                    placement = closestLocation(Protoss_Pylon, Position(tile));
                if (placement.isValid())
                    return true;
            }

            // Check if our main choke should get a Pylon for a Shield Battery
            if (vis(Protoss_Pylon) == 1 && !BuildOrder::takeNatural() && (!Spy::enemyRush() || !Players::vZ())) {
                placement = closestLocation(Protoss_Pylon, (Position)BWEB::Map::getMainChoke()->Center());
                if (placement.isValid())
                    return true;
            }

            // Check if we are being busted, add an extra pylon to the defenses
            if (Spy::enemyBust() && Walls::getNaturalWall() && BuildOrder::isWallNat()) {
                int cnt = 0;
                TilePosition sum(0, 0);
                TilePosition center(0, 0);
                for (auto &defense : Walls::getNaturalWall()->getDefenses()) {
                    if (!Pylons::hasPowerNow(defense, Protoss_Photon_Cannon)) {
                        sum += defense;
                        cnt++;
                    }
                }

                if (cnt != 0) {
                    center = sum / cnt;
                    Position c(center);
                    double distBest = DBL_MAX;

                    // Find unique closest tile to center of defenses
                    for (auto &tile : Walls::getNaturalWall()->getDefenses()) {
                        Position defenseCenter = Position(tile) + Position(32, 32);
                        double dist = defenseCenter.getDistance(c);
                        if (dist < distBest && isPlannable(Protoss_Pylon, tile) && isBuildable(Protoss_Pylon, tile) && isPathable(building, tile)) {
                            distBest = dist;
                            placement = tile;
                        }
                    }
                }

                if (placement.isValid())
                    return true;
            }

            // Resort to finding a production location
            placement = closestLocation(Protoss_Pylon, BWEB::Map::getMainPosition());
            return placement.isValid();
        }

        bool findRefineryLocation(UnitType building, TilePosition& placement)
        {
            if (!building.isRefinery())
                return false;

            placement = closestLocation(building, BWEB::Map::getMainPosition());
            return placement.isValid();
        }

        TilePosition getBuildLocation(UnitType building)
        {
            auto placement = TilePositions::Invalid;
            const auto finders ={ findWallLocation, findDefenseLocation, findProxyLocation, findResourceDepotLocation, findPylonLocation, findProductionLocation, findTechLocation, findRefineryLocation };
            for (auto &finder : finders) {
                if (finder(building, placement))
                    return placement;
            }

            // HACK: Try to get a placement if we are being horror gated
            if (Spy::enemyProxy() && Util::getTime() < Time(5, 00) && !isDefensiveType(building) && !building.isResourceDepot())
                placement = Broodwar->getBuildLocation(building, BWEB::Map::getMainTile(), 16);

            return placement;
        }

        void updateNextExpand()
        {
            // Determine what our current/next expansion is
            auto baseType = Broodwar->self()->getRace().getResourceDepot();
            for (auto &station : Expansion::getExpandOrder()) {
                if (isBuildable(baseType, station->getBase()->Location())) {
                    currentExpansion = station;
                    break;
                }
            }
            for (auto &station : Expansion::getExpandOrder()) {
                if (station != currentExpansion && isBuildable(baseType, station->getBase()->Location())) {
                    nextExpansion = station;
                    break;
                }
            }
        }

        void updatePlan()
        {
            // Wipe clean all queued buildings, store a cache of old buildings
            map<TilePosition, UnitInfo*> oldBuilders;
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;
                if (unit.getBuildPosition().isValid()) {
                    oldBuilders[unit.getBuildPosition()] = &unit;
                    unit.setBuildingType(UnitTypes::None);
                    unit.setBuildPosition(TilePositions::Invalid);
                }
            }
            buildingsPlanned.clear();

            // Add up how many more buildings of each type we need
            for (auto &[building, count] : BuildOrder::getBuildQueue()) {
                int queuedCount = 0;

                auto morphed = !building.whatBuilds().first.isWorker();
                auto addon = building.isAddon();

                if (building.isAddon()
                    || !building.isBuilding())
                    continue;

                // If the building morphed from another building type, add the visible amount of child type to the parent type
                int morphOffset = 0;
                if (building == Zerg_Creep_Colony)
                    morphOffset = vis(Zerg_Sunken_Colony) + vis(Zerg_Spore_Colony);
                if (building == Zerg_Hatchery)
                    morphOffset = vis(Zerg_Lair) + vis(Zerg_Hive);
                if (building == Zerg_Lair)
                    morphOffset = vis(Zerg_Hive);

                // Queue the cost of any morphs or building
                if (count > vis(building) + morphOffset && (Workers::getMineralWorkers() > 0 || building.mineralPrice() == 0 || Broodwar->self()->minerals() >= building.mineralPrice()) && (Workers::getGasWorkers() > 0 || building.gasPrice() == 0 || Broodwar->self()->gas() >= building.gasPrice())) {
                    plannedMineral += building.mineralPrice() * (count - vis(building) - morphOffset);
                    plannedGas += building.gasPrice() * (count - vis(building) - morphOffset);
                }

                if (morphed)
                    continue;

                // Queue building if our actual count is higher than our visible count
                while (count > queuedCount + vis(building) + morphOffset) {
                    queuedCount++;
                    auto here = getBuildLocation(building);
                    auto center = Position(here) + Position(building.tileWidth() * 16, building.tileHeight() * 16);
                    if (!here.isValid())
                        continue;

                    auto builder = getBuilder(building, center);

                    // Expired building attempt on current builder
                    if (buildingTimer[here] <= Broodwar->getFrameCount()) {
                        buildingTimer.erase(here);
                    }

                    // Use old builder if we're not early game, as long as it's not stuck or was stuck recently
                    else {
                        for (auto &[oldHere, oldBuilder] : oldBuilders) {
                            if (oldHere == here && oldBuilder && Workers::canAssignToBuild(*oldBuilder))
                                builder = oldBuilder;
                        }
                    }

                    if (!builder)
                        Visuals::drawBox(Position(here) + Position(4, 4), Position(here + building.tileSize()) - Position(4, 4), Colors::Red);

                    if (here.isValid() && builder && Workers::shouldMoveToBuild(*builder, here, building)) {
                        Visuals::drawBox(Position(here) + Position(4, 4), Position(here + building.tileSize()) - Position(4, 4), Colors::White);
                        Visuals::drawLine(builder->getPosition(), center, Colors::White);
                        builder->setBuildingType(building);
                        builder->setBuildPosition(here);
                        buildingsPlanned[here] = building;

                        if (buildingTimer.find(here) == buildingTimer.end())
                            buildingTimer[here] = Broodwar->getFrameCount() + int(BWEB::Map::getGroundDistance(builder->getPosition(), center) / builder->getSpeed()) + 200;
                    }
                }
            }
        }

        void updateReachable()
        {
            if (!BWEB::Map::getMainChoke()->Center() || !unreachablePositions.empty())
                return;
            auto start = Position(BWEB::Map::getMainChoke()->Center());

            const auto reachable = [&](auto &pos) {
                auto newPath = BWEB::Path(start, pos, Zerg_Drone);
                newPath.generateJPS([&](const TilePosition &t) { return newPath.unitWalkable(t); });
                return newPath.isReachable();
            };

            for (auto &block : BWEB::Blocks::getBlocks()) {
                if (Terrain::inTerritory(PlayerState::Self, block.getCenter())) {
                    if (!reachable(block.getCenter())) {
                        for_each(block.getLargeTiles().begin(), block.getLargeTiles().end(), [&](auto &t) { unreachablePositions.push_back(t); });
                        for_each(block.getMediumTiles().begin(), block.getMediumTiles().end(), [&](auto &t) { unreachablePositions.push_back(t); });
                        for_each(block.getSmallTiles().begin(), block.getSmallTiles().end(), [&](auto &t) { unreachablePositions.push_back(t); });
                    }
                }
            }
            for (auto &wall : BWEB::Walls::getWalls()) {
                for (auto &def : wall.second.getDefenses()) {
                    if (!reachable(Position(def)))
                        unreachablePositions.push_back(def);                    
                }
            }
        }

        void validateDefenses()
        {
            validDefenses.clear();

            const auto checkDefense = [&](TilePosition tile, TilePosition pathTile) {
                if (tile == pathTile
                    || tile + TilePosition(0, 1) == pathTile
                    || tile + TilePosition(1, 0) == pathTile
                    || tile + TilePosition(1, 1) == pathTile)
                    validDefenses.erase(tile);
                return;
            };

            // Create valid defenses based on needing to allow a path or not
            for (auto &[_, wall] : BWEB::Walls::getWalls()) {
                for (auto &defTile : wall.getDefenses())
                    validDefenses.insert(defTile);
            }
            for (auto &station : BWEB::Stations::getStations()) {
                for (auto &defTile : station.getDefenses()) {
                    validDefenses.insert(defTile);
                }
            }

            // Erase positions that are blockers
            if (Util::getTime() > Time(12, 00)) {
                if (total(Protoss_Dragoon) > 0
                    || total(Zerg_Ultralisk) > 0
                    || total(Zerg_Lurker) > 0
                    || (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Grooved_Spines) > 0 && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Muscular_Augments) > 0)) {
                    for (auto &[_, wall] : BWEB::Walls::getWalls()) {
                        for (auto &pathTile : wall.getPath().getTiles()) {
                            for (auto &defTile : wall.getDefenses())
                                checkDefense(defTile, pathTile);

                            if (wall.getStation()) {
                                for (auto &defTile : wall.getStation()->getDefenses())
                                    checkDefense(defTile, pathTile);
                            }
                        }
                    }
                }
            }
        }
    }

    void onUnitDestroy(Unit unit)
    {
        if (unit->getType().isBuilding())
            unreachablePositions.clear();
    }

    void onUnitDiscover(Unit unit)
    {
        if (unit->getType().isBuilding())
            unreachablePositions.clear();
    }

    void onFrame()
    {
        plannedMineral = 0;
        plannedGas = 0;
        expansionPlanned = false;

        updateReachable();
        validateDefenses();
        updateNextExpand();
        updatePlan();
    }

    void onStart()
    {
        BWEB::Blocks::findBlocks();
    }

    UnitType whatPlannedHere(TilePosition here)
    {
        auto itr = buildingsPlanned.find(here);
        if (itr != buildingsPlanned.end())
            return itr->second;

        // Since Zerg buildings can morph from a colony, we need to differentiate which type we planned for when placing
        if (plannedGround.find(here) != plannedGround.end())
            return Zerg_Sunken_Colony;
        if (plannedAir.find(here) != plannedAir.end())
            return Zerg_Spore_Colony;

        return None;
    }

    bool overlapsPlan(UnitInfo& unit, Position here)
    {
        // If this is a defensive building and we don't have a plan for it here anymore
        if (isDefensiveType(unit.getType()) && unit.getPlayer() == Broodwar->self() && validDefenses.find(unit.getTilePosition()) == validDefenses.end())
            return true;

        // Check if there's a building queued there already
        for (auto &[tile, building] : buildingsPlanned) {

            if (Broodwar->self()->minerals() < building.mineralPrice() * 0.8
                || Broodwar->self()->gas() < building.gasPrice() * 0.8
                || unit.getBuildPosition() == tile)
                continue;

            auto center = Position(tile) + Position(building.tileWidth() * 16, building.tileHeight() * 16);
            if (Util::boxDistance(unit.getType(), here, building, center) <= 4)
                return true;
            if (unit.getType() == Zerg_Larva && Util::boxDistance(Zerg_Egg, here, building, center) <= 4)
                return true;
        }
        return false;
    }

    bool isDefensiveType(UnitType building)
    {
        return building == Protoss_Photon_Cannon
            || building == Protoss_Shield_Battery
            || building == Zerg_Creep_Colony
            || building == Zerg_Sunken_Colony
            || building == Zerg_Spore_Colony
            || building == Zerg_Spire
            || building == Terran_Missile_Turret
            || building == Terran_Bunker;
    }

    bool isProductionType(UnitType building)
    {
        return building == Protoss_Gateway
            || building == Protoss_Robotics_Facility
            || building == Protoss_Stargate
            || building == Terran_Barracks
            || building == Terran_Factory
            || building == Terran_Starport
            || building == Zerg_Hatchery
            || building == Zerg_Lair
            || building == Zerg_Hive;
    }

    bool isTechType(UnitType building)
    {
        return building == Protoss_Forge
            || building == Protoss_Cybernetics_Core
            || building == Protoss_Robotics_Support_Bay
            || building == Protoss_Observatory
            || building == Protoss_Citadel_of_Adun
            || building == Protoss_Templar_Archives
            || building == Protoss_Fleet_Beacon
            || building == Protoss_Arbiter_Tribunal
            || building == Terran_Supply_Depot
            || building == Terran_Engineering_Bay
            || building == Terran_Academy
            || building == Terran_Armory
            || building == Terran_Science_Facility
            || building == Zerg_Spawning_Pool
            || building == Zerg_Evolution_Chamber
            || building == Zerg_Hydralisk_Den
            || building == Zerg_Spire
            || building == Zerg_Queens_Nest
            || building == Zerg_Defiler_Mound
            || building == Zerg_Ultralisk_Cavern;
    }

    bool isWallType(UnitType building)
    {
        return building == Protoss_Forge
            || building == Protoss_Gateway
            || building == Protoss_Pylon
            || building == Terran_Barracks
            || building == Terran_Supply_Depot
            || building == Zerg_Hydralisk_Den
            || building == Zerg_Evolution_Chamber
            || building == Zerg_Hatchery;
    }

    int getPlannedMineral() { return plannedMineral; }
    int getPlannedGas() { return plannedGas; }
    BWEB::Station * getCurrentExpansion() { return currentExpansion; }
}