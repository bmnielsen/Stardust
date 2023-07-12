#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Combat::Destination {

    namespace {

        vector<BWEB::Station*> combatScoutOrder;
        multimap<double, Position> groundCleanupPositions;
        multimap<double, Position> airCleanupPositions;

        void updateCleanup()
        {
            groundCleanupPositions.clear();
            airCleanupPositions.clear();

            if (Util::getTime() < Time(6, 00) || !Stations::getStations(PlayerState::Enemy).empty())
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

        void getCleanupPosition(UnitInfo& unit)
        {
            // Finishing enemy off, find remaining bases we haven't scouted, if we haven't visited in 2 minutes
            if (Terrain::getEnemyStartingPosition().isValid()) {
                auto best = DBL_MAX;
                for (auto &area : mapBWEM.Areas()) {
                    for (auto &base : area.Bases()) {
                        if (area.AccessibleNeighbours().size() == 0
                            || Terrain::inTerritory(PlayerState::Self, base.Center()))
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
                combatScoutOrder = Scouts::getScoutOrder(Zerg_Zergling);
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

    void updateDestination(UnitInfo& unit)
    {
        // If attacking and target is close, set as destination
        if (unit.getLocalState() == LocalState::Attack) {
            if (unit.attemptingRunby()) {
                unit.setDestination(unit.getEngagePosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Red);
            }
            else if (unit.getInterceptPosition().isValid()) {
                unit.setDestination(unit.getInterceptPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Orange);
            }
            else if (unit.getSurroundPosition().isValid()) {
                unit.setDestination(unit.getSurroundPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Yellow);
            }
            else if (unit.getEngagePosition().isValid()) {
                unit.setDestination(unit.getEngagePosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Green);
            }
            else if (unit.hasTarget()) {
                unit.setDestination(unit.getTarget().lock()->getPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Blue);
            }
        }
        else if (unit.getLocalState() == LocalState::Retreat || unit.getGlobalState() == GlobalState::Retreat) {
            const auto &retreat = Stations::getClosestRetreatStation(unit);

            if (!unit.globalRetreat() && unit.attemptingRegroup()) {
                unit.setDestination(unit.getCommander().lock()->getPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Red);
            }
            else if (retreat && !unit.isFlying() && retreat == Terrain::getMyNatural() && Broodwar->mapFileName().find("MatchPoint") != string::npos && Terrain::getMyMain()->getBase()->Location() == TilePosition(100, 14)) {
                unit.setDestination(Position(3369, 1690));
            }
            else if (retreat && unit.isFlying()) {
                unit.setDestination(retreat->getBase()->Center());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Orange);
            }
            else if (retreat) {
                unit.setDestination(Stations::getDefendPosition(retreat));
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Orange);
            }
            else {
                unit.setDestination(Position(BWEB::Map::getMainChoke()->Center()));
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Yellow);
            }
        }
        else {
            if (unit.getGoal().isValid()) {
                unit.setDestination(unit.getGoal());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Red);
            }
            else if (unit.attemptingRegroup()) {
                unit.setDestination(unit.getCommander().lock()->getPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Orange);
            }
            else if (unit.attemptingHarass()) {
                unit.setDestination(Terrain::getHarassPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Yellow);
            }
            else if (unit.hasTarget()) {
                unit.setDestination(unit.getTarget().lock()->getPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Green);
            }
            else if (Terrain::getAttackPosition().isValid() && unit.canAttackGround()) {
                unit.setDestination(Terrain::getAttackPosition());
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Blue);
            }
            else {
                getCleanupPosition(unit);
                //Broodwar->drawLineMap(unit.getPosition(), unit.getDestination(), Colors::Purple);
            }
        }
    }

    void update(UnitInfo& unit)
    {
        updateDestination(unit);
    }
}