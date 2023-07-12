#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Transports {

    namespace {

        constexpr tuple commands{ Command::transport, Command::escort, Command::retreat };

        void updateCargo(UnitInfo& unit)
        {
            auto cargoSize = 0;
            for (auto &c : unit.getAssignedCargo()) {
                if (auto &cargo = c.lock())
                    cargoSize += cargo->getType().spaceRequired();
            }

            // Check if we are ready to assign this worker to a transport
            const auto readyToAssignWorker = [&](UnitInfo& cargo) {
                if (cargoSize + cargo.getType().spaceRequired() > 8 || cargo.hasTransport())
                    return false;

                return false;

                auto buildDist = cargo.getBuildType().isValid() ? BWEB::Map::getGroundDistance((Position)cargo.getBuildPosition(), (Position)cargo.getTilePosition()) : 0.0;
                auto resourceDist = cargo.hasResource() ? BWEB::Map::getGroundDistance(cargo.getPosition(), cargo.getResource().lock()->getPosition()) : 0.0;

                if ((cargo.getBuildPosition().isValid() && buildDist == DBL_MAX) || (cargo.getBuildType().isResourceDepot() && Terrain::isIslandMap()))
                    return true;
                if (cargo.hasResource() && resourceDist == DBL_MAX)
                    return true;
                return false;
            };

            // Check if we are ready to assign this unit to a transport
            const auto readyToAssignUnit = [&](UnitInfo& cargo) {
                auto targetDist = BWEB::Map::getGroundDistance(cargo.getPosition(), cargo.getEngagePosition());

                if (cargo.getType() == Protoss_Dragoon || cargo.getType() == Protoss_High_Templar)
                    return true;
                if (Terrain::isIslandMap() && !cargo.getType().isFlyer() && targetDist > 640.0)
                    return true;
                return false;
            };

            // Check if we are ready to remove any units
            auto &cargoList = unit.getAssignedCargo();
            for (auto &c = cargoList.begin(); c != cargoList.end(); c++) {
                auto &cargo = *(c->lock());

                if (cargo.getRole() == Role::Combat && !readyToAssignUnit(cargo)) {
                    cargoList.erase(c);
                    cargo.setTransport(nullptr);
                    cargoSize -= cargo.getType().spaceRequired();
                    break;
                }

                if (cargo.getRole() == Role::Worker && !readyToAssignWorker(cargo)) {
                    cargoList.erase(c);
                    cargo.setTransport(nullptr);
                    cargoSize -= cargo.getType().spaceRequired();
                    break;
                }
            }

            // Check if we are ready to add any units
            if (cargoSize < 8) {
                for (auto &c : Units::getUnits(PlayerState::Self)) {
                    auto &cargo = *c;

                    if (cargoSize + cargo.getType().spaceRequired() > 8
                        || cargo.hasTransport()
                        || !cargo.unit()->isCompleted())
                        continue;

                    if (cargo.getRole() == Role::Combat && readyToAssignUnit(cargo)) {
                        cargo.setTransport(&unit);
                        unit.getAssignedCargo().push_back(c);
                        cargoSize += unit.getType().spaceRequired();
                    }

                    if (cargo.getRole() == Role::Worker && readyToAssignWorker(cargo)) {
                        cargo.setTransport(&unit);
                        unit.getAssignedCargo().push_back(c);
                        cargoSize += unit.getType().spaceRequired();
                    }
                }
            }
        }

        void updateTransportState(UnitInfo& unit)
        {
            shared_ptr<UnitInfo> closestCargo = nullptr;
            auto distBest = DBL_MAX;
            for (auto &c : unit.getAssignedCargo()) {
                auto &cargo = c.lock();

                //Broodwar->drawLineMap(unit.getPosition(), cargo->getPosition(), Colors::Cyan);

                auto dist = cargo->getPosition().getDistance(unit.getPosition());
                if (dist < distBest) {
                    distBest = dist;
                    closestCargo = cargo;
                }
            }

            auto closestThreat = Util::getClosestUnit(unit.getPosition(), PlayerState::Enemy, [&](auto &u) {
                return u->getAirDamage() > 0;
            });
            auto threatWithinRange = closestThreat ? closestThreat->isWithinRange(unit) : false;

            // Check if the transport is too close
            const auto tooClose = [&](UnitInfo& cargo) {
                if (cargo.getType() != Protoss_High_Templar && cargo.getType() != Protoss_Reaver)
                    return false;

                const auto range = cargo.getTarget().lock()->getType().isFlyer() ? cargo.getAirRange() : cargo.getGroundRange();
                //const auto targetDist = cargo.getType() == Protoss_High_Templar ? cargo.getPosition().getDistance(cargo.getTarget().getPosition()) : BWEB::Map::getGroundDistance(cargo.getPosition(), cargo.getTarget().getPosition());
                const auto targetDist = cargo.getPosition().getDistance(cargo.getTarget().lock()->getPosition());

                return targetDist == DBL_MAX || targetDist <= range;
            };

            // Check if the transport is too far
            const auto tooFar = [&](UnitInfo& cargo) {
                if (cargo.getType() != Protoss_High_Templar && cargo.getType() != Protoss_Reaver)
                    return false;

                const auto range = cargo.getTarget().lock()->getType().isFlyer() ? max(cargo.getTarget().lock()->getAirRange(), cargo.getAirRange()) : cargo.getGroundRange();
                //const auto targetDist = cargo.getType() == Protoss_High_Templar ? cargo.getPosition().getDistance(cargo.getTarget().getPosition()) : BWEB::Map::getGroundDistance(cargo.getPosition(), cargo.getTarget().getPosition());
                const auto targetDist = cargo.getPosition().getDistance(cargo.getTarget().lock()->getPosition());

                return targetDist >= range + 96.0;
            };

            // Check if this unit is ready to unload
            const auto readyToUnload = [&](UnitInfo& cargo) {
                auto cargoReady = cargo.getType() == Protoss_High_Templar ? (cargo.hasTarget() && cargo.canStartCast(TechTypes::Psionic_Storm, cargo.getTarget().lock()->getPosition()) && unit.getPosition().getDistance(cargo.getTarget().lock()->getPosition()) <= 400) : cargo.canStartAttack();

                return cargo.getLocalState() == LocalState::Attack && unit.isHealthy() && cargo.isHealthy() && cargoReady;
            };

            // Check if this unit is ready to be picked up
            const auto readyToLoad = [&](UnitInfo& cargo) {
                if (!cargo.hasTarget())
                    return true;

                return cargo.isRequestingPickup();
            };

            // Check if this worker is ready to mine
            const auto readyToMine = [&](UnitInfo& worker) {
                if (Terrain::isIslandMap() && worker.hasResource() && worker.getResource().lock()->getTilePosition().isValid() && mapBWEM.GetArea(unit.getTilePosition()) == mapBWEM.GetArea(worker.getResource().lock()->getTilePosition()))
                    return true;
                return false;
            };

            // Check if this worker is ready to build
            const auto readyToBuild = [&](UnitInfo& worker) {
                if (Terrain::isIslandMap() && worker.getBuildPosition().isValid() && mapBWEM.GetArea(worker.getTilePosition()) == mapBWEM.GetArea(worker.getBuildPosition()))
                    return true;
                return false;
            };

            const auto setState = [&](TransportState state) {
                if (state == TransportState::Monitoring || state == TransportState::Loading)
                    unit.setTransportState(state);
                if (state == TransportState::Retreating && unit.getTransportState() != TransportState::Monitoring && unit.getTransportState() != TransportState::Loading && state != TransportState::Engaging && !unit.isHealthy())
                    unit.setTransportState(state);
                if (state == TransportState::Engaging && unit.getTransportState() != TransportState::Monitoring && (unit.getTransportState() == TransportState::None || unit.isHealthy()))
                    unit.setTransportState(state);
            };

            // Check if we should be loading/unloading any cargo
            for (auto &c : unit.getAssignedCargo()) {
                if (!c.lock())
                    continue;
                auto &cargo = *(c.lock());

                // If the cargo is not loaded
                if (!cargo.unit()->isLoaded()) {

                    // If it's requesting a pickup, set load state to 1    
                    if (readyToLoad(cargo)) {
                        setState(TransportState::Loading);
                        unit.setDestination(cargo.getPosition());
                    }
                    else if (cargo.shared_from_this() == closestCargo) {
                        setState(TransportState::Monitoring);
                        unit.setDestination(cargo.getPosition());
                    }
                }

                // Else if the cargo is loaded
                else if (cargo.unit()->isLoaded()) {

                    if (cargo.hasTarget() && cargo.getEngagePosition().isValid()) {
                        if (readyToUnload(cargo)) {
                            unit.setDestination(cargo.getEngagePosition());

                            if (!tooFar(cargo))
                                unit.unit()->unload(cargo.unit());

                            if (tooFar(cargo))
                                setState(TransportState::Engaging);
                            else
                                setState(TransportState::Retreating);
                        }
                        else if (tooClose(cargo))
                            setState(TransportState::Retreating);
                        else if (tooFar(cargo) && unit.getGlobalState() == GlobalState::Attack)
                            setState(TransportState::Engaging);
                        else
                            setState(TransportState::Retreating);
                    }
                }
            }
        }

        void updateDestination(UnitInfo& unit)
        {
            if (unit.getTransportState() == TransportState::Loading || unit.getTransportState() == TransportState::Monitoring)
                return;

            // If we have no cargo, wait at nearest base
            if (unit.getAssignedCargo().empty()) {
                auto station = Stations::getClosestStationGround(unit.getPosition(), PlayerState::Self);
                if (station)
                    unit.setDestination(station->getBase()->Center());
            }

            // Resort to main, hopefully we still have it
            if (!unit.getDestination().isValid())
                unit.setDestination(BWEB::Map::getMainPosition());
        }

        void updateDecision(UnitInfo& unit)
        {
            if (!unit.unit() || !unit.unit()->exists()                                                                                            // Prevent crashes
                || unit.unit()->isLockedDown() || unit.unit()->isMaelstrommed() || unit.unit()->isStasised() || !unit.unit()->isCompleted())    // If the unit is locked down, maelstrommed, stassised, or not completed
                return;

            // Convert our commands to strings to display what the unit is doing for debugging
            map<int, string> commandNames{
                make_pair(0, "Transport"),
                make_pair(1, "Escort"),
                make_pair(2, "Retreat"),
            };

            // Iterate commands, if one is executed then don't try to execute other commands
            int width = unit.getType().isBuilding() ? -16 : unit.getType().width() / 2;
            int i = Util::iterateCommands(commands, unit);
            Broodwar->drawTextMap(unit.getPosition() + Position(width, 0), "%c%s", Text::White, commandNames[i].c_str());
        }

        void updateTransports()
        {
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;

                if (unit.getRole() == Role::Transport) {
                    updateCargo(unit);
                    updateTransportState(unit);
                    updateDestination(unit);
                    updateDecision(unit);
                }
            }
        }
    }

    void onFrame()
    {
        Visuals::startPerfTest();
        updateTransports();
        Visuals::endPerfTest("Transports");
    }

    void removeUnit(UnitInfo& unit)
    {
        // If unit has a transport, remove it from the transports cargo, then set transport to nullptr
        if (unit.hasTransport()) {
            auto &cargoList = unit.getTransport().lock()->getAssignedCargo();
            for (auto &cargo = cargoList.begin(); cargo != cargoList.end(); cargo++) {
                if (cargo->lock() && cargo->lock() == unit.shared_from_this()) {
                    cargoList.erase(cargo);
                    break;
                }
            }
            unit.setTransport(nullptr);
        }

        // If unit is a transport, set the transport of all cargo to nullptr
        for (auto &c : unit.getAssignedCargo()) {
            if (auto &cargo = c.lock())
                cargo->setTransport(nullptr);
        }
    }
}