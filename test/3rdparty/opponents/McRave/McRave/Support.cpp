#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Support {

    namespace {

        constexpr tuple commands{ Command::misc, Command::special, Command::escort };

        void updateCounters()
        {
        }

        void updateDestination(UnitInfo& unit)
        {
            auto closestStation = Stations::getClosestStationAir(PlayerState::Self, unit.getPosition());
            auto closestSpore = Util::getClosestUnit(unit.getPosition(), PlayerState::Self, [&](auto &u) {
                return u.getType() == Zerg_Spore_Colony;
            });

            // Send Overlords to safety from enemy air
            if (unit.getType() == Zerg_Overlord && closestSpore && (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) == 0 || !unit.isHealthy())) {
                if (closestSpore) {
                    unit.setDestination(closestSpore->getPosition());
                    for (int x = -1; x <= 1; x++) {
                        for (int y = -1; y <= 1; y++) {
                            auto center = Position(closestSpore->getTilePosition() + TilePosition(x, y)) + Position(16, 16);
                            auto closest = Util::getClosestUnit(center, PlayerState::Self, [&](auto &u) {
                                return u.getType() == Zerg_Overlord;
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

            else if (unit.getType() == Zerg_Overlord && !closestSpore && Strategy::getEnemyOpener() == "Corsair" && (Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) == 0 || !unit.isHealthy()))
                unit.setDestination(Terrain::getMyNatural()->getBase()->Center());

            // Set goal as destination
            else if (unit.getGoal().isValid())
                unit.setDestination(unit.getGoal());

            // Find the highest combat cluster that doesn't overlap a current support action of this UnitType
            else if (unit.getType() != Zerg_Overlord || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace)) {
                auto highestCluster = 0.0;
                for (auto &[cluster, position] : Combat::getCombatClusters()) {
                    const auto score = cluster / (position.getDistance(Terrain::getAttackPosition()) * position.getDistance(unit.getPosition()));
                    if (score > highestCluster && !Actions::overlapsActions(unit.unit(), position, unit.getType(), PlayerState::Self, 64)) {
                        highestCluster = score;
                        unit.setDestination(position);
                    }
                }

                // Move detectors between target and unit vs Terran
                if (unit.getType().isDetector() && Players::vT()) {
                    if (unit.hasTarget())
                        unit.setDestination((unit.getTarget().getPosition() + unit.getDestination()) / 2);
                    else {
                        auto closestEnemy = Util::getClosestUnit(unit.getDestination(), PlayerState::Enemy, [&](auto &u) {
                            return !u.getType().isWorker() && !u.getType().isBuilding();
                        });

                        if (closestEnemy)
                            unit.setDestination((closestEnemy->getPosition() + unit.getDestination()) / 2);
                    }
                }
            }
            else if (Terrain::getMyNatural() && Stations::needAirDefenses(Terrain::getMyNatural()) > 0)
                unit.setDestination(Terrain::getMyNatural()->getBase()->Center());
            else if (closestStation)
                unit.setDestination(closestStation->getBase()->Center());

            if (!unit.getDestination().isValid())
                unit.setDestination(BWEB::Map::getMainPosition());

            Visuals::drawLine(unit.getPosition(), unit.getDestination(), Colors::Red);
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