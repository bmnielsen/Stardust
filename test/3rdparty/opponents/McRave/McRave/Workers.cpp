#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Workers {

    namespace {

        int minWorkers = 0;
        int gasWorkers = 0;
        int boulderWorkers = 0;
        map<Position, double> distChecker;

        vector<BWEB::Station*> getSafeStations(UnitInfo& unit)
        {
            // If this station already has defenses, it's likely safe, just need to hide there
            vector<BWEB::Station *> safeStations;
            if (unit.hasResource() && Stations::getGroundDefenseCount(unit.getResource().lock()->getStation()) > 0 && Util::getTime() > Time(6, 00))
                safeStations.push_back(unit.getResource().lock()->getStation());

            // Find safe stations to mine resources from
            for (auto &station : Stations::getStations(PlayerState::Self)) {
                auto closestNatural = BWEB::Stations::getClosestNaturalStation(station->getBase()->Location());
                auto closestMain = BWEB::Stations::getClosestMainStation(station->getBase()->Location());

                // If unit is close, it must be safe
                if (unit.getPosition().getDistance(station->getResourceCentroid()) < 320.0 || mapBWEM.GetArea(unit.getTilePosition()) == station->getBase()->GetArea())
                    safeStations.push_back(station);

                // Otherwise create a path to it to check if it's safe
                else {
                    BWEB::Path newPath(unit.getPosition(), station->getBase()->Center(), unit.getType());
                    newPath.generateJPS([&](const TilePosition &t) { return newPath.terrainWalkable(t) || Util::rectangleIntersect(Position(station->getBase()->Location()), Position(station->getBase()->Location()) + Position(96, 64), Position(t)); });
                    unit.setDestinationPath(newPath);

                    // If unit is far, we need to check if it's safe
                    auto threatPosition = Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                        return (unit.getType().isFlyer() ? Grids::getEAirThreat(p) > 0.0f : Grids::getEGroundThreat(p) > 0.0f) ||
                            (unit.hasTarget() && p.getDistance(unit.getTarget().lock()->getPosition()) < unit.getTarget().lock()->getGroundReach());
                    });

                    // If JPS path is safe
                    if (!threatPosition.isValid())
                        safeStations.push_back(station);
                }
            }
            return safeStations;
        }

        BWEB::Station * getTransferStation(UnitInfo& unit)
        {
            // Allow some drones to transfer early on - TODO: Move to StationManager
            if (Util::getTime() < Time(3, 30) && !Spy::enemyRush() && vis(UnitTypes::Zerg_Drone) >= 9) {
                for (auto &station : Stations::getStations(PlayerState::Self)) {
                    int droneCount = 0;

                    for (auto &mineral : Resources::getMyMinerals()) {
                        if (mineral->hasStation() && mineral->getStation() == station)
                            droneCount += mineral->getGathererCount();
                    }
                    if (droneCount >= 2)
                        continue;

                    // Check if worker arrives in time when hatch completes
                    auto closestHatch = Util::getClosestUnit(station->getBase()->Center(), PlayerState::Self, [&](auto &u) {
                        return u->getType().isResourceDepot();
                    });
                    auto framesToArrive = unit.getPosition().getDistance(station->getBase()->Center()) / unit.getSpeed();
                    auto frameCompletedAt = closestHatch->timeCompletesWhen().frames;

                    if (Broodwar->getFrameCount() + framesToArrive + 48 > frameCompletedAt)
                        return station;
                }
            }
            return nullptr;
        }

        bool isBuildingSafe(UnitInfo& unit)
        {
            auto buildCenter = Position(unit.getBuildPosition()) + Position(unit.getBuildType().tileWidth() * 16, unit.getBuildType().tileHeight() * 16);

            if (Util::getTime() < Time(4, 00))
                return true;

            for (auto &t : unit.getUnitsTargetingThis()) {
                if (auto targeter = t.lock()) {
                    if (targeter->isThreatening())
                        return false;
                }
            }

            // If around defenders
            auto aroundDefenders = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                if (u->getRole() != Role::Combat && u->getRole() != Role::Defender)
                    return false;

                return (unit.getPosition().getDistance(u->getPosition()) < u->getGroundReach() && u->getPosition().getDistance(buildCenter) < u->getGroundReach())
                    || (mapBWEM.GetArea(unit.getTilePosition()) == mapBWEM.GetArea(unit.getBuildPosition()) && u->getPosition().getDistance(buildCenter) < u->getGroundReach());
            });
            if (aroundDefenders)
                return true;

            // Generate a path that obeys refinery placement as well
            BWEB::Path newPath(unit.getPosition(), buildCenter, unit.getType());
            auto buildingWalkable = [&](const auto &t) {
                return newPath.unitWalkable(t) || (unit.getBuildType().isRefinery() && BWEB::Map::isUsed(t) == UnitTypes::Resource_Vespene_Geyser);
            };
            newPath.generateJPS([&](const TilePosition &t) { return buildingWalkable(t); });

            auto threatPosition = Util::findPointOnPath(newPath, [&](Position p) {
                return Grids::getEGroundThreat(p) > 0.0 && Broodwar->isVisible(TilePosition(p));
            });

            if (threatPosition && threatPosition.getDistance(unit.getPosition()) < 200.0 && Util::getTime() > Time(5, 00))
                return false;
            return true;
        }

        bool isResourceSafe(UnitInfo& unit)
        {
            // Creates a path to the resource and verifies the resource is safe
            if (unit.hasResource()) {
                BWEB::Path newPath(unit.getPosition(), unit.getResource().lock()->getStation()->getResourceCentroid(), unit.getType());
                const auto walkable = [&](const TilePosition &t) {
                    return newPath.terrainWalkable(t);
                };
                newPath.generateJPS(walkable);

                auto threatPosition = Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                    return (unit.getType().isFlyer() ? Grids::getEAirThreat(p) > 0.0f : Grids::getEGroundThreat(p) > 0.0f) ||
                        (unit.hasTarget() && p.getDistance(unit.getTarget().lock()->getPosition()) < unit.getTarget().lock()->getGroundReach());
                });

                if (threatPosition)
                    return false;
                else
                    unit.setDestinationPath(newPath);
            }
            return true;
        }

        bool isResourceFlooded(UnitInfo& unit)
        {
            // Determine if we have over the work cap assigned
            if (unit.hasResource() && unit.getPosition().getDistance(unit.getResource().lock()->getPosition()) < 64.0 && unit.getResource().lock()->getGathererCount() >= unit.getResource().lock()->getWorkerCap() + 1)
                return true;

            // Determine if we have excess assigned resources with openings available at the current Station
            if (unit.hasResource() && unit.getResource().lock()->getType().isMineralField()) {
                for (auto &mineral : Resources::getMyMinerals()) {
                    if (mineral->getStation() == unit.getResource().lock()->getStation() && mineral->getGathererCount() < unit.getResource().lock()->getGathererCount() - 1)
                        return true;
                }
            }
            return false;
        }

        bool isResourceThreatened(UnitInfo& unit)
        {
            return (unit.hasResource() && unit.getResource().lock()->isThreatened());
        }

        void updatePath(UnitInfo& unit)
        {
            // Create a path
            if (unit.getDestination().isValid() && (unit.getDestinationPath().getTarget() != TilePosition(unit.getDestination()) || !unit.getDestinationPath().isReachable()) && (!mapBWEM.GetArea(TilePosition(unit.getPosition())) || !mapBWEM.GetArea(TilePosition(unit.getDestination())) || mapBWEM.GetArea(TilePosition(unit.getPosition()))->AccessibleFrom(mapBWEM.GetArea(TilePosition(unit.getDestination()))))) {
                BWEB::Path newPath(unit.getPosition(), unit.getDestination(), unit.getType());
                auto resourceTile = unit.hasResource() ? unit.getResource().lock()->getTilePosition() : TilePositions::Invalid;
                auto resourceType = unit.hasResource() ? unit.getResource().lock()->getType() : UnitTypes::None;

                const auto resourceWalkable = [&](const TilePosition &tile) {
                    return (unit.hasResource() && !unit.getBuildPosition().isValid() && tile.x >= resourceTile.x && tile.x < resourceTile.x + resourceType.tileWidth() && tile.y >= resourceTile.y && tile.y < resourceTile.y + resourceType.tileHeight())
                        || newPath.unitWalkable(tile);
                };
                newPath.generateJPS(resourceWalkable);
                unit.setDestinationPath(newPath);
            }

            // Set destination to intermediate position along path
            unit.setNavigation(unit.getDestination());
            if (unit.getDestinationPath().getTarget() == TilePosition(unit.getDestination())) {
                auto newDestination = Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                    return p.getDistance(unit.getPosition()) >= 64.0;
                });

                if (newDestination.isValid())
                    unit.setNavigation(newDestination);
            }
            //Visuals::drawPath(unit.getDestinationPath());
        }

        void updateDestination(UnitInfo& unit)
        {
            // If unit has a building type and we are ready to build
            if (unit.getBuildType() != UnitTypes::None && shouldMoveToBuild(unit, unit.getBuildPosition(), unit.getBuildType()) && isBuildingSafe(unit)) {
                auto center = Position(unit.getBuildPosition()) + Position(unit.getBuildType().tileWidth() * 16, unit.getBuildType().tileHeight() * 16);

                // Probes wants to try to place offcenter so the Probe doesn't have to reset collision on placement
                if (unit.getType() == Protoss_Probe && unit.hasResource()) {
                    center.x += unit.getResource().lock()->getPosition().x > center.x ? unit.getBuildType().dimensionRight() : -unit.getBuildType().dimensionLeft();
                    center.y += unit.getResource().lock()->getPosition().y > center.y ? unit.getBuildType().dimensionDown() : -unit.getBuildType().dimensionUp();
                }

                // Drone wants to be slightly offcenter to prevent a long animation before placing
                if (unit.getType() == Zerg_Drone) {
                    center.x -= 0;
                    center.y -= 7;
                }

                unit.setDestination(center);
            }

            // If unit has a transport
            else if (unit.hasTransport())
                unit.setDestination(unit.getTransport().lock()->getPosition());

            // If unit has a goal
            else if (unit.getGoal().isValid())
                unit.setDestination(unit.getGoal());            

            // If unit has a resource
            else if (unit.hasResource() && unit.getResource().lock()->getResourceState() == ResourceState::Mineable && unit.getPosition().getDistance(unit.getResource().lock()->getPosition()) > 96.0)
                unit.setDestination(unit.getResource().lock()->getPosition());
        }

        void updateResource(UnitInfo& unit)
        {
            // Get some information of the workers current assignment
            const auto isGasunit =          unit.hasResource() && unit.getResource().lock()->getType().isRefinery();
            const auto isMineralunit =      unit.hasResource() && unit.getResource().lock()->getType().isMineralField();
            const auto resourceSafe =       isResourceSafe(unit);
            const auto threatened =         isResourceThreatened(unit);
            const auto excessAssigned =     isResourceFlooded(unit);
            const auto transferStation =    getTransferStation(unit);
            const auto closestStation =     Stations::getClosestStationAir(unit.getPosition(), PlayerState::Self);

            // Check if unit needs a re-assignment
            const auto needGas =            unit.unit()->isCarryingMinerals() && !Resources::isGasSaturated() && isMineralunit && gasWorkers < BuildOrder::gasWorkerLimit();
            const auto needMinerals =       unit.unit()->isCarryingGas() && !Resources::isMineralSaturated() && isGasunit && gasWorkers > BuildOrder::gasWorkerLimit();
            const auto needNewAssignment =  !unit.hasResource() || needGas || needMinerals || threatened || (excessAssigned && !threatened) || (isMineralunit && transferStation);
            auto distBest =                 threatened ? 0.0 : DBL_MAX;
            auto oldResource =              unit.hasResource() ? unit.getResource().lock() : nullptr;

            // Return if we dont need an assignment
            if (!needNewAssignment)
                return;

            // Get some information about the safe stations
            auto safeStations =             getSafeStations(unit);
            if (transferStation)
                safeStations.push_back(transferStation);            

            const auto resourceReady = [&](ResourceInfo& resource, int i) {
                if (!resource.unit()
                    || resource.getType() == UnitTypes::Resource_Vespene_Geyser
                    || (resource.unit()->exists() && !resource.unit()->isCompleted())
                    || resource.getGathererCount() >= i
                    || (resource.getResourceState() != ResourceState::Mineable && Stations::getMiningStationsCount() != 0))
                    return false;
                return true;
            };

            // Check if we need gas units
            if (!threatened && (needGas || !unit.hasResource() || excessAssigned)) {
                for (auto &r : Resources::getMyGas()) {
                    auto &resource = *r;

                    if (!resourceReady(resource, resource.getWorkerCap())
                        || (!safeStations.empty() && find(safeStations.begin(), safeStations.end(), resource.getStation()) == safeStations.end())
                        || resource.isThreatened())
                        continue;

                    auto closestunit = Util::getClosestUnit(resource.getPosition(), PlayerState::Self, [&](auto &u) {
                        return u->getRole() == Role::Worker && !u->getBuildPosition().isValid() && (!u->hasResource() || u->getResource().lock()->getType().isMineralField());
                    });
                    if (closestunit && unit != *closestunit)
                        continue;

                    auto dist = resource.getPosition().getDistance(unit.getPosition());
                    if ((dist < distBest && !threatened) || (dist > distBest && threatened)) {
                        unit.setResource(r.get());
                        distBest = dist;
                    }
                }
            }

            // Check if we need mineral units
            if (needMinerals || !unit.hasResource() || threatened || excessAssigned || transferStation) {
                for (int i = 1; i <= 2; i++) {
                    for (auto &r : Resources::getMyMinerals()) {
                        auto &resource = *r;
                        auto allowedGatherCount = threatened ? 50 : i;

                        if (!resourceReady(resource, allowedGatherCount)
                            || (!transferStation && !safeStations.empty() && find(safeStations.begin(), safeStations.end(), resource.getStation()) == safeStations.end())
                            || (!transferStation && closestStation && Stations::getSaturationRatio(closestStation) < 2.0 && resource.getStation() != closestStation)
                            || (transferStation && resource.getStation() != transferStation)
                            || resource.isThreatened())
                            continue;

                        auto stationDist = unit.getPosition().getDistance(resource.getStation()->getBase()->Center());
                        auto resourceDist = Util::boxDistance(UnitTypes::Resource_Mineral_Field, resource.getPosition(), Broodwar->self()->getRace().getResourceDepot(), resource.getStation()->getBase()->Center());
                        auto dist = stationDist * resourceDist;

                        if ((dist < distBest && !threatened) || (dist > distBest && threatened)) {
                            unit.setResource(r.get());
                            distBest = dist;
                        }
                    }

                    // Don't check again if we assigned one
                    if (unit.hasResource() && unit.getResource().lock() != oldResource && !threatened)
                        break;
                }
            }

            // Assign resource
            if (unit.hasResource()) {

                // Remove current assignemt
                if (oldResource) {
                    oldResource->getType().isMineralField() ? minWorkers-- : gasWorkers--;
                    oldResource->removeTargetedBy(unit.weak_from_this());
                }

                // Add next assignment
                unit.getResource().lock()->getType().isMineralField() ? minWorkers++ : gasWorkers++;
                unit.getResource().lock()->addTargetedBy(unit.weak_from_this());
                Resources::recheckSaturation();
            }
        }

        void updateBuilding(UnitInfo& unit)
        {
            if (unit.getBuildPosition() == TilePositions::None
                || unit.getBuildType() == UnitTypes::None)
                return;

            if (!canAssignToBuild(unit)) {
                unit.setBuildingType(UnitTypes::None);
                unit.setBuildPosition(TilePositions::Invalid);
            }
        }

        constexpr tuple commands{ Command::misc, Command::attack, Command::click, Command::burrow, Command::returnResource, Command::build, Command::clearNeutral, Command::move, Command::gather };
        void updateDecision(UnitInfo& unit)
        {
            // Convert our commands to strings to display what the unit is doing for debugging
            map<int, string> commandNames{
                make_pair(0, "Misc"),
                make_pair(1, "Attack"),
                make_pair(2, "Click"),
                make_pair(3, "Burrow"),
                make_pair(4, "Return"),
                make_pair(5, "Build"),
                make_pair(6, "Clear"),
                make_pair(7, "Move"),
                make_pair(8, "Gather")
            };

            // Iterate commands, if one is executed then don't try to execute other commands
            int width = unit.getType().isBuilding() ? -16 : unit.getType().width() / 2;
            int i = Util::iterateCommands(commands, unit);
            Broodwar->drawTextMap(unit.getPosition() + Position(width, 0), "%c%s", Text::White, commandNames[i].c_str());
        }

        void updateunits()
        {
            // Iterate workers and make decisions
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;
                if (unit.getRole() == Role::Worker) {
                    updateResource(unit);
                    updateBuilding(unit);
                    updateDestination(unit);
                    updatePath(unit);
                    updateDecision(unit);
                }
            }
        }
    }

    void onFrame() {
        Visuals::startPerfTest();
        boulderWorkers = 0; // HACK: Need a better solution to limit boulder units
        updateunits();
        Visuals::endPerfTest("Workers");
    }

    void removeUnit(UnitInfo& unit) {
        unit.getResource().lock()->getType().isRefinery() ? gasWorkers-- : minWorkers--;
        unit.getResource().lock()->removeTargetedBy(unit.weak_from_this());
        unit.setResource(nullptr);
    }

    bool canAssignToBuild(UnitInfo& unit)
    {
        if (unit.isBurrowed()
            || unit.getRole() != Role::Worker
            || !unit.isHealthy()
            || unit.isStuck()
            || unit.wasStuckRecently()
            || (unit.hasResource() && !unit.getResource().lock()->getType().isMineralField() && Util::getTime() < Time(10, 00))
            || (unit.hasResource() && unit.getResource().lock()->isPocket()))
            return false;
        return true;
    }

    bool shouldMoveToBuild(UnitInfo& unit, TilePosition tile, UnitType type) {
        if (!mapBWEM.GetArea(unit.getTilePosition()))
            return true;
        auto center = Position(tile) + Position(type.tileWidth() * 16, type.tileHeight() * 16);
        if (unit.getPosition().getDistance(center) < 64.0 && type.isResourceDepot())
            return true;

        auto mineralIncome = max(0.0, double(minWorkers - 1) * 0.045);
        auto gasIncome = max(0.0, double(gasWorkers - 1) * 0.07);
        auto speed = unit.getSpeed();
        auto dist = BWEB::Map::getGroundDistance(unit.getPosition(), center);
        auto time = (dist / speed);
        auto enoughGas = unit.getBuildType().gasPrice() > 0 ? Broodwar->self()->gas() + int(gasIncome * time) >= unit.getBuildType().gasPrice() * 0.8 : true;
        auto enoughMins = unit.getBuildType().mineralPrice() > 0 ? Broodwar->self()->minerals() + int(mineralIncome * time) >= unit.getBuildType().mineralPrice() * 0.8 : true;

        return (enoughGas && enoughMins);
    };

    int getMineralWorkers() { return minWorkers; }
    int getGasWorkers() { return gasWorkers; }
    int getBoulderWorkers() { return boulderWorkers; }
}