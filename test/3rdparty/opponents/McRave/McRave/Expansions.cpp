#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Expansion {
    namespace {
        vector<BWEB::Station*> expansionOrder, dangerousStations, islandStations;
        map<BWEB::Station *, map<BWEB::Station*, BWEB::Path>> expansionNetwork;
        map<BWEB::Station*, vector<UnitInfo*>> blockingNeutrals;
        bool blockersExists = false;

        void updateDangerousStations()
        {
            // Check if any base goes through enemy territory currently
            dangerousStations.clear();
            for (auto &station : BWEB::Stations::getStations()) {
                auto& path = expansionNetwork[Terrain::getMyMain()][&station];
                if (!path.getTiles().empty()) {
                    auto danger = Util::findPointOnPath(path, [&](auto &p) {
                        return Terrain::inTerritory(PlayerState::Enemy, p);
                    });

                    if (danger.isValid())
                        dangerousStations.push_back(&station);
                }
            }
        }

        void updateIslandStations()
        {
            // Check if any base is an island separated by resources
            islandStations.clear();
            for (auto &station : BWEB::Stations::getStations()) {
                for (auto &blocker : blockingNeutrals[&station]) {
                    if (blocker) {
                        if (blocker->getType().isMineralField() || blocker->getType().isRefinery())
                            islandStations.push_back(&station);                        
                    }
                }
            }
        }

        void updateExpandBlockers()
        {
            // Create a map of blocking neutrals per station
            blockingNeutrals.clear();
            for (auto &[station, path] : expansionNetwork[Terrain::getMyMain()]) {
                Visuals::drawPath(path);
                Util::testPointOnPath(path, [&](Position &p) {
                    auto type = BWEB::Map::isUsed(TilePosition(p));
                    if (type != UnitTypes::None) {
                        const auto closestNeutral = Util::getClosestUnit(p, PlayerState::Neutral, [&](auto &u) {
                            return u->getType() == type;
                        });
                        if (closestNeutral && closestNeutral->getPosition().getDistance(p) < 96.0) {
                            blockingNeutrals[station].push_back(closestNeutral);
                            Broodwar->drawCircleMap(closestNeutral->getPosition(), 4, Colors::Cyan);
                            return true;
                        }
                    }
                    return false;
                });
            }

            // Check if a blocker exists
            blockersExists = false;
            if (Planning::getCurrentExpansion() && Util::getTime() > Time(8, 00)) {

                // Blockers exist if our current expansion has blockers, not our next one
                blockersExists = !blockingNeutrals[Planning::getCurrentExpansion()].empty();

                // Mark the blockers for the next expansion for death, so we can kill it before we need an expansion
                for (auto &neutral : blockingNeutrals[Planning::getCurrentExpansion()]) {
                    if (neutral)
                        neutral->setMarkForDeath(true);
                }
            }
        }

        void updateExpandPlan()
        {
            if (Broodwar->getFrameCount() < 5)
                return;

            updateExpandBlockers();
            updateDangerousStations();
            updateIslandStations();

            // Establish an initial parent station as our natural (most of our units rally through this)
            auto parentStation = Terrain::getMyNatural();
            auto enemyStation = Terrain::getEnemyNatural();

            // TODO: BWEB has failed, we can't expand
            if (!parentStation)
                return;


            // Check if we need a gas expansion
            auto geysersOwned = 0;
            for (auto &resource : Resources::getMyGas()) {
                if (resource->getResourceState() == ResourceState::Mineable && resource->getRemainingResources() > 200)
                    geysersOwned++;
            }

            // Score each station
            auto allowedFirstMineralBase = (Players::vT() || Players::ZvZ() || Players::ZvP()) ? 4 : 3;
            expansionOrder.clear();

            if (Terrain::getMyMain())
                expansionOrder.push_back(Terrain::getMyMain());
            if (Terrain::getMyNatural())
                expansionOrder.push_back(Terrain::getMyNatural());

            for (int i = 0; i < int(BWEB::Stations::getStations().size()); i++) {
                auto costBest = DBL_MAX;
                BWEB::Station * stationBest = nullptr;
                auto home = BWEB::Map::getNaturalChoke() ? Position(BWEB::Map::getNaturalChoke()->Center()) : BWEB::Map::getMainPosition();

                for (auto &station : BWEB::Stations::getStations()) {
                    auto grdParent = log(expansionNetwork[parentStation][&station].getDistance());
                    auto grdHome = expansionNetwork[Terrain::getMyMain()][&station].getDistance();
                    auto airParent = station.getBase()->Center().getDistance(parentStation->getBase()->Center());
                    auto airHome = station.getBase()->Center().getDistance(home);
                    auto airCenter = station.getBase()->Center().getDistance(mapBWEM.Center());
                    auto grdEnemy = enemyStation ? expansionNetwork[enemyStation][&station].getDistance() : 1.0;
                    auto airEnemy = enemyStation ? station.getBase()->Center().getDistance(enemyStation->getBase()->Center()) : 1.0;

                    if ((station.getBase()->GetArea() == mapBWEM.GetArea(TilePosition(mapBWEM.Center())) && expansionOrder.size() < 4)
                        || (find_if(expansionOrder.begin(), expansionOrder.end(), [&](auto &s) { return s == &station; }) != expansionOrder.end())
                        || station == Terrain::getMyMain()
                        || station == Terrain::getMyNatural()
                        || (Terrain::getEnemyMain() && station == Terrain::getEnemyMain())
                        || (Terrain::getEnemyNatural() && station == Terrain::getEnemyNatural())
                        || (station.getBase()->Geysers().empty() && int(expansionOrder.size()) < allowedFirstMineralBase)
                        || (station.getBase()->Geysers().empty() && geysersOwned < allowedFirstMineralBase)
                        || (mapBWEM.GetPath(BWEB::Map::getMainPosition(), station.getBase()->Center()).empty() && expansionOrder.size() < 3)
                        || (Terrain::inTerritory(PlayerState::Enemy, station.getBase()->GetArea()))
                        || (find_if(islandStations.begin(), islandStations.end(), [&](auto &s) { return s == &station; }) != islandStations.end()))
                        continue;

                    // Check if it's a dangerous/blocked station
                    if (find_if(dangerousStations.begin(), dangerousStations.end(), [&](auto &s) { return s == &station; }) != dangerousStations.end()) {
                        grdEnemy = sqrt(1.0 + grdEnemy);
                        airEnemy = sqrt(1.0 + airEnemy);
                    }
                    if (!blockingNeutrals[&station].empty()) {
                        grdHome *= double(blockingNeutrals[&station].size());
                        airHome *= double(blockingNeutrals[&station].size());
                    }
                    auto dist = log(grdParent * grdHome * airParent * airHome) / (grdEnemy * airEnemy * airCenter);

                    // Check for a blocking neutral
                    auto blockerCost = 0.0;
                    for (auto &blocker : blockingNeutrals[&station])
                        blockerCost += double(blocker->getHealth()) / 1000.0;
                    if (blockerCost > 0)
                        dist = blockerCost;

                    // Add in remaining resources
                    auto percentMinerals = (double(1 + Stations::getMineralsRemaining(&station)) / double(1 + Stations::getMineralsInitial(&station)));
                    auto percentGas = (double(1 + Stations::getGasRemaining(&station)) / double(1 + Stations::getGasInitial(&station)));
                    auto cost = dist / (percentMinerals * percentGas);

                    if (cost < costBest) {
                        costBest = cost;
                        stationBest = &station;
                    }
                }

                if (stationBest) {
                    expansionOrder.push_back(stationBest);
                    parentStation = stationBest;
                }
            }
        }        
    }

    void onFrame()
    {
        updateExpandPlan();
    }

    void onStart()
    {
        // Create a path to each station that only obeys terrain
        for (auto &station : BWEB::Stations::getStations()) {
            for (auto &otherStation : BWEB::Stations::getStations()) {
                if (station == otherStation)
                    continue;

                BWEB::Path unitPath(station.getBase()->Center(), otherStation.getBase()->Center(), Protoss_Dragoon, true, false);
                unitPath.generateJPS([&](auto &t) { return unitPath.unitWalkable(t); });

                if (unitPath.isReachable())
                    expansionNetwork[&station][&otherStation] = unitPath;
                else {
                    BWEB::Path terrainPath(station.getBase()->Center(), otherStation.getBase()->Center(), Protoss_Dragoon, true, false);
                    terrainPath.generateJPS([&](auto &t) { return terrainPath.terrainWalkable(t); });
                    expansionNetwork[&station][&otherStation] = terrainPath;
                }
            }
        }
    }

    bool expansionBlockersExists() { return blockersExists; }
    vector<BWEB::Station*> getExpandOrder() { return expansionOrder; }
}