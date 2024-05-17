#include "McRave.h"
#include "Events.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Buildings {
    namespace {
        map <UnitType, int> morphedThisFrame;
        set<Position> larvaPositions;
        set<TilePosition> unpoweredPositions;

        bool willDieToAttacks(UnitInfo& building) {
            auto possibleDamage = 0;
            for (auto &a : building.getUnitsTargetingThis()) {
                if (auto attacker = a.lock()) {
                    if (attacker->isWithinRange(building))
                        possibleDamage+= int(attacker->getGroundDamage());
                }
            }

            return possibleDamage > building.getHealth() + building.getShields();
        };

        void updateInformation(UnitInfo& building)
        {
            // If a building is unpowered, get a pylon placement ready
            if (building.getType().requiresPsi() && !Pylons::hasPowerSoon(building.getTilePosition(), building.getType()))
                unpoweredPositions.insert(building.getTilePosition());

            // If this is a defensive building and is no longer planned here, mark it for suicide
            if (building.getRole() == Role::Defender && Planning::overlapsPlan(building, building.getPosition())) {

                for (auto &[_, wall] : BWEB::Walls::getWalls()) {
                    for (auto &defTile : wall.getDefenses()) {
                        if (defTile == building.getTilePosition())
                            building.setMarkForDeath(true);
                    }
                }
                for (auto &station : BWEB::Stations::getStations()) {
                    for (auto &defTile : station.getDefenses()) {
                        if (defTile == building.getTilePosition())
                            building.setMarkForDeath(true);
                    }
                }
            }

            if (Util::getTime() > Time(6, 00)) {
                auto range = max({ building.getGroundRange(), building.getAirRange(), double(building.getType().sightRange()) });
                Zones::addZone(building.getPosition(), ZoneType::Defend, 1, range);
            }
        }

        void updateLarvaEncroachment(UnitInfo& building)
        {
            // Updates a list of larva positions that can block planning
            if (building.getType() == Zerg_Larva && !Production::larvaTrickRequired(building)) {
                for (auto &[_, pos] : building.getPositionHistory())
                    larvaPositions.insert(pos);
            }
            if (building.getType() == Zerg_Egg || building.getType() == Zerg_Lurker_Egg)
                larvaPositions.insert(building.getPosition());
        }

        void cancel(UnitInfo& building)
        {
            auto isStation = BWEB::Stations::getClosestStation(building.getTilePosition())->getBase()->Location() == building.getTilePosition();

            // Cancelling refineries for our gas trick
            if (BuildOrder::isGasTrick() && building.getType().isRefinery() && !building.unit()->isCompleted() && BuildOrder::buildCount(building.getType()) < vis(building.getType())) {
                building.unit()->cancelMorph();
                BWEB::Map::removeUsed(building.getTilePosition(), 4, 2);
            }

            // Cancelling refineries we don't want
            if (building.getType().isRefinery() && Spy::enemyRush() && vis(building.getType()) == 1 && building.getType().getRace() != Races::Zerg && BuildOrder::buildCount(building.getType()) < vis(building.getType())) {
                building.unit()->cancelConstruction();
            }

            // Cancelling buildings that are being built/morphed but will be dead
            if (!building.unit()->isCompleted() && willDieToAttacks(building) && building.getType() != Zerg_Sunken_Colony && building.getType() != Zerg_Spore_Colony) {
                building.unit()->cancelConstruction();
                Events::onUnitCancelBecauseBWAPISucks(building);
            }

            // Cancelling hatcheries if we're being proxy 2gated
            if (building.getType() == Zerg_Hatchery && isStation && Terrain::getMyNatural() && building.getTilePosition() == Terrain::getMyNatural()->getBase()->Location() && Util::getTime() < Time(4, 00) && Spy::getEnemyBuild() == "2Gate" && Spy::enemyProxy()) {
                building.unit()->cancelConstruction();
                Events::onUnitCancelBecauseBWAPISucks(building);
            }

            // Cancelling 3rds if we're being 1 based
            if (building.getType() == Zerg_Hatchery && isStation && Terrain::getMyNatural() && Terrain::getMyMain() && building.getTilePosition() != Terrain::getMyNatural()->getBase()->Location() && building.getTilePosition() != Terrain::getMyMain()->getBase()->Location() && Util::getTime() < Time(4, 00) && !BuildOrder::takeThird()) {
                building.unit()->cancelConstruction();
                Events::onUnitCancelBecauseBWAPISucks(building);
            }

            // Cancelling colonies we don't need now
            if (building.getType() == Zerg_Creep_Colony) {

            }
        }

        void morph(UnitInfo& building)
        {
            auto needLarvaSpending = vis(Zerg_Larva) > 3 && Broodwar->self()->supplyUsed() < Broodwar->self()->supplyTotal() && BuildOrder::getUnitReservation(Zerg_Mutalisk) == 0 && Util::getTime() < Time(4, 30) && com(Zerg_Sunken_Colony) > 2;
            auto morphType = UnitTypes::None;
            auto station = BWEB::Stations::getClosestStation(building.getTilePosition());
            auto wall = BWEB::Walls::getClosestWall(building.getTilePosition());
            auto plannedType = Planning::whatPlannedHere(building.getTilePosition());

            // Lair morphing
            if (building.getType() == Zerg_Hatchery && !willDieToAttacks(building) && BuildOrder::buildCount(Zerg_Lair) > vis(Zerg_Lair) + vis(Zerg_Hive) + morphedThisFrame[Zerg_Lair] + morphedThisFrame[Zerg_Hive]) {
                auto morphTile = BWEB::Map::getMainTile();
                const auto closestScout = Util::getClosestUnitGround(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u->getType().isWorker();
                });
                if (closestScout && Stations::getStations(PlayerState::Self).size() >= 2 && mapBWEM.GetArea(closestScout->getTilePosition()) == BWEB::Map::getMainArea())
                    morphTile = BWEB::Map::getNaturalTile();

                // Extra larva timings (main): 3:02, 3:31, 4:00
                if (building.getTilePosition() == morphTile) {
                    if (morphTile == BWEB::Map::getMainTile()) {
                        if (Util::getTime() >= Time(3, 02)
                            || (Util::getTime() >= Time(3, 31) && BuildOrder::getCurrentTransition().find("2Hatch") != string::npos)
                            || (Util::getTime() >= Time(4, 00) && BuildOrder::getCurrentTransition().find("3Hatch") != string::npos)
                            || Players::ZvZ())
                            morphType = Zerg_Lair;
                    }
                    else
                        morphType = Zerg_Lair;
                }
            }

            // Hive morphing
            else if (building.getType() == Zerg_Lair && !willDieToAttacks(building) && BuildOrder::buildCount(Zerg_Hive) > vis(Zerg_Hive) + morphedThisFrame[Zerg_Hive])
                morphType = Zerg_Hive;

            // Greater Spire morphing
            else if (building.getType() == Zerg_Spire && !willDieToAttacks(building) && BuildOrder::buildCount(Zerg_Greater_Spire) > vis(Zerg_Greater_Spire) + morphedThisFrame[Zerg_Greater_Spire])
                morphType = Zerg_Greater_Spire;

            // Sunken / Spore morphing
            else if (building.getType() == Zerg_Creep_Colony && !willDieToAttacks(building) && !needLarvaSpending) {
                auto wallDefense = wall && wall->getDefenses().find(building.getTilePosition()) != wall->getDefenses().end();
                auto stationDefense = station && station->getDefenses().find(building.getTilePosition()) != station->getDefenses().end();

                // If we planned already
                if (plannedType == Zerg_Sunken_Colony)
                    morphType = Zerg_Sunken_Colony;
                else if (plannedType == Zerg_Spore_Colony)
                    morphType = Zerg_Spore_Colony;

                // If this is a Station defense
                else if (stationDefense && Stations::needGroundDefenses(station) > morphedThisFrame[Zerg_Sunken_Colony] && com(Zerg_Spawning_Pool) > 0)
                    morphType = Zerg_Sunken_Colony;
                else if (stationDefense && Stations::needAirDefenses(station) > morphedThisFrame[Zerg_Spore_Colony] && com(Zerg_Evolution_Chamber) > 0)
                    morphType = Zerg_Spore_Colony;

                // If this is a Wall defense
                else if (wallDefense && Walls::needAirDefenses(*wall) > morphedThisFrame[Zerg_Spore_Colony] && plannedType == Zerg_Spore_Colony && com(Zerg_Evolution_Chamber) > 0)
                    morphType = Zerg_Spore_Colony;
                else if (wallDefense && Walls::needGroundDefenses(*wall) > morphedThisFrame[Zerg_Sunken_Colony] && plannedType == Zerg_Sunken_Colony && com(Zerg_Spawning_Pool) > 0)
                    morphType = Zerg_Sunken_Colony;
            }

            // Morph
            if (morphType.isValid() && building.isCompleted()) {
                building.unit()->morph(morphType);
                morphedThisFrame[morphType]++;
            }
        }

        void reBuild(UnitInfo& building)
        {
            // Terran building needs new scv
            if (building.getType().getRace() == Races::Terran && !building.unit()->isCompleted() && !building.unit()->getBuildUnit()) {
                auto builder = Util::getClosestUnit(building.getPosition(), PlayerState::Self, [&](auto &u) {
                    return u->getType().isWorker() && u->getBuildType() == None;
                });

                if (builder)
                    builder->unit()->rightClick(building.unit());
            }
        }

        void updateCommands(UnitInfo& building)
        {
            morph(building);
            cancel(building);
            reBuild(building);

            // Comsat scans - Move to special manager
            if (building.getType() == Terran_Comsat_Station && building.hasTarget()) {
                auto buildingTarget = building.getTarget().lock();
                if (buildingTarget->unit()->exists() && !Actions::overlapsDetection(building.unit(), buildingTarget->getPosition(), PlayerState::Enemy)) {
                    building.unit()->useTech(TechTypes::Scanner_Sweep, buildingTarget->getPosition());
                    Actions::addAction(building.unit(), buildingTarget->getPosition(), TechTypes::Scanner_Sweep, PlayerState::Self);
                }
            }
        }
    }

    void onFrame()
    {
        // Reset counters
        larvaPositions.clear();
        morphedThisFrame.clear();
        unpoweredPositions.clear();

        // Update all my buildings
        for (auto &u : Units::getUnits(PlayerState::Self)) {
            auto &unit = *u;

            if (unit.getRole() == Role::Production || unit.getRole() == Role::Defender) {
                updateLarvaEncroachment(unit);
                updateInformation(unit);
                updateCommands(unit);
            }
        }
    }

    set<TilePosition>& getUnpoweredPositions() { return unpoweredPositions; }
    set<Position>& getLarvaPositions() { return larvaPositions; }
}