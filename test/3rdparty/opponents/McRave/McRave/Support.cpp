#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Support {

    namespace {

        constexpr tuple commands{ Command::misc, Command::special, Command::escort };

        set<Position> assignedOverlords;

        void updateCounters()
        {
            assignedOverlords.clear();
        }

        void updateDestination(UnitInfo& unit)
        {
            auto closestStation = Stations::getClosestStationAir(unit.getPosition(), PlayerState::Self);
            auto closestSpore = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                return u->getType() == Zerg_Spore_Colony;
            });
            auto enemyAir = Spy::getEnemyTransition() == "Corsair"
                || Spy::getEnemyTransition() == "2PortWraith"
                || Players::getStrength(PlayerState::Enemy).airToAir > 0.0;
            auto types ={ Zerg_Hydralisk, Zerg_Ultralisk, Protoss_Dragoon, Terran_Marine, Terran_Siege_Tank_Siege_Mode, Terran_Siege_Tank_Tank_Mode };
            auto followArmyPossible = any_of(types.begin(), types.end(), [&](auto &t) { return com(t) > 0; });

            if (Util::getTime() < Time(7, 00) && Stations::getStations(PlayerState::Self).size() >= 2 && !Players::ZvZ())
                closestStation = Terrain::getMyNatural();

            // Set goal as destination
            if (unit.getGoal().isValid())
                unit.setDestination(unit.getGoal());

            // Send Overlords to safety from enemy air
            else if (unit.getType() == Zerg_Overlord && closestSpore && (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) == 0 || !unit.isHealthy() || !followArmyPossible)) {
                if (closestSpore) {
                    unit.setDestination(closestSpore->getPosition());
                    for (int x = -1; x <= 1; x++) {
                        for (int y = -1; y <= 1; y++) {
                            auto center = Position(closestSpore->getTilePosition() + TilePosition(x, y)) + Position(16, 16);
                            auto closest = Util::getClosestUnit(center, PlayerState::Self, [&](auto &u) {
                                return u->getType() == Zerg_Overlord;
                            });
                            if (closest && unit == *closest) {
                                unit.setDestination(center);
                                goto endloop;
                            }
                        }
                    }
                }
            endloop:;
            }

            // Find the highest combat cluster that doesn't overlap a current support action of this UnitType
            else if (unit.getType() != Zerg_Overlord || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace)) {
                auto highestCluster = 0.0;               

                auto closestPartner = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                    return find(types.begin(), types.end(), u->getType()) != types.end() && assignedOverlords.find(u->getPosition()) == assignedOverlords.end();
                });

                if (closestPartner) {
                    assignedOverlords.insert(closestPartner->getPosition());
                    unit.setDestination(closestPartner->getPosition());
                }

                // Move detectors between target and unit vs Terran
                if (unit.getType().isDetector() && Players::PvT()) {
                    if (unit.hasTarget())
                        unit.setDestination((unit.getTarget().lock()->getPosition() + unit.getDestination()) / 2);
                    else {
                        auto closestEnemy = Util::getClosestUnit(unit.getDestination(), PlayerState::Enemy, [&](auto &u) {
                            return !u->getType().isWorker() && !u->getType().isBuilding();
                        });

                        if (closestEnemy)
                            unit.setDestination((closestEnemy->getPosition() + unit.getDestination()) / 2);
                    }
                }
            }

            // Space them out near the station
            else if (unit.getType() == Zerg_Overlord && closestStation && closestStation->getChokepoint() && (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) == 0 || !unit.isHealthy() || enemyAir)) {
                auto natDist = closestStation->getBase()->Center().getDistance(Position(closestStation->getChokepoint()->Center()));
                auto chokeCenter = Position(closestStation->getChokepoint()->Center());
                unit.setDestination(closestStation->getBase()->Center());
                for (auto x = -2; x < 2; x++) {
                    for (auto y = -2; y < 2; y++) {
                        auto position = closestStation->getBase()->Center() + Position(96 * x, 96 * y);
                        if (position.getDistance(chokeCenter) > natDist && !Actions::overlapsActions(unit.unit(), position, unit.getType(), PlayerState::Self, 32))
                            unit.setDestination(position);
                    }
                }
            }

            else if (Terrain::getMyNatural() && Stations::needAirDefenses(Terrain::getMyNatural()) > 0)
                unit.setDestination(Terrain::getMyNatural()->getBase()->Center());
            else if (closestStation)
                unit.setDestination(closestStation->getBase()->Center());

            if (!unit.getDestination().isValid())
                unit.setDestination(BWEB::Map::getMainPosition());
        }

        void updatePath(UnitInfo& unit)
        {
            // Check if we need a new path
            if (!unit.getDestination().isValid() || (!unit.getDestinationPath().getTiles().empty() && unit.getDestinationPath().getTarget() == TilePosition(unit.getDestination())))
                return;

            if (unit.getType() == Zerg_Overlord && (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) == 0 || unit.getGoal().isValid())) {
                const auto xMax = Broodwar->mapWidth() - 4;
                const auto yMax = Broodwar->mapHeight() - 4;
                const auto centerDist = min(unit.getDestination().getDistance(mapBWEM.Center()), unit.getPosition().getDistance(mapBWEM.Center()));
                BWEB::Path newPath(unit.getPosition(), unit.getDestination(), unit.getType());
                newPath.generateJPS([&](const TilePosition &t) { return !Terrain::inTerritory(PlayerState::Enemy, Position(t)) && t.isValid() && (t.x < 4 || t.y < 4 || t.x > xMax || t.y > yMax || Position(t).getDistance(mapBWEM.Center()) >= centerDist); });
                unit.setDestinationPath(newPath);
            }
            Visuals::drawPath(unit.getDestinationPath());
        }

        void updateNavigation(UnitInfo& unit)
        {
            // If path is reachable, find a point n pixels away to set as new destination
            unit.setNavigation(unit.getDestination());
            auto dist = unit.isFlying() ? 96.0 : 160.0;
            if (unit.getDestinationPath().isReachable() && unit.getPosition().getDistance(unit.getDestination()) > 96.0) {
                auto newDestination = Util::findPointOnPath(unit.getDestinationPath(), [&](Position p) {
                    return p.getDistance(unit.getPosition()) >= dist;
                });

                if (newDestination.isValid())
                    unit.setNavigation(newDestination);
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
                make_pair(2, "Escort")
            };

            // Iterate commands, if one is executed then don't try to execute other commands
            int width = unit.getType().isBuilding() ? -16 : unit.getType().width() / 2;
            int i = Util::iterateCommands(commands, unit);
            Broodwar->drawTextMap(unit.getPosition() + Position(width, 0), "%c%s", Text::White, commandNames[i].c_str());
        }

        void updateUnits()
        {
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                UnitInfo &unit = *u;
                if (unit.getRole() == Role::Support && unit.unit()->isCompleted()) {
                    updateDestination(unit);
                    updatePath(unit);
                    updateNavigation(unit);
                    updateDecision(unit);
                }
            }
        }
    }

    void onFrame()
    {
        updateCounters();
        updateUnits();
    }
}