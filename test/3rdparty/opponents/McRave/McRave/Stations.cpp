#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Stations
{
    namespace {
        map<BWEB::Station*, PlayerState> stations;
        map<BWEB::Station*, Position> defendPositions;
        multimap<double, BWEB::Station *> stationsBySaturation;
        multimap<double, BWEB::Station *> stationsByProduction;
        map<BWEB::Station *, int> airDefenseCount, groundDefenseCount, remainingMinerals, remainingGas, initialMinerals, initialGas;
        set<BWEB::Station*> retreatPositions;
        int miningStations = 0, gasingStations = 0;

        void updateSaturation() {

            // Sort stations by saturation and current larva count
            remainingMinerals.clear();
            remainingGas.clear();
            stationsBySaturation.clear();
            auto mineralsLeftTotal = 0, gasLeftTotal = 0;
            for (auto &station : getStations(PlayerState::Self)) {
                auto workerCount = 0;
                auto resourceCount = 0;
                for (auto &mineral : Resources::getMyMinerals()) {
                    if (mineral->getStation() == station) {
                        resourceCount++;
                        remainingMinerals[station]+=mineral->unit()->getResources();
                        mineralsLeftTotal+=mineral->unit()->getResources();
                        workerCount+=mineral->targetedByWhat().size();
                    }
                }
                for (auto &gas : Resources::getMyGas()) {
                    if (gas->getStation() == station) {
                        resourceCount++;
                        remainingGas[station]+=gas->unit()->getResources();
                        gasLeftTotal+=gas->unit()->getResources();
                        workerCount+=gas->targetedByWhat().size();
                    }
                }

                // Order by lowest saturation first
                auto saturatedLevel = workerCount > 0 ? double(workerCount) / double(resourceCount) : 0.0;
                stationsBySaturation.emplace(saturatedLevel, station);
            }

            // Determine how many mining and gasing stations we have
            miningStations = int(ceil(double(mineralsLeftTotal) / 1500.0));
            gasingStations = int(ceil(double(gasLeftTotal) / 5000.0));
        }

        void updateProduction()
        {
            // Sort stations by production capabilities
            stationsByProduction.clear();
            for (auto &station : getStations(PlayerState::Self)) {
                int production = 0;
                for (auto &unit : Units::getUnits(PlayerState::Self)) {
                    if (Planning::isProductionType(unit->getType())) {
                        auto closestStation = getClosestStationAir(unit->getPosition(), PlayerState::Self, [&](auto station) {
                            return station->isMain() || Broodwar->self()->getRace() == Races::Zerg;
                        });
                        if (closestStation == station)
                            production++;
                    }
                }
                stationsByProduction.emplace(production, station);
            }
        }

        void updateStationDefenses() {

            // Calculate defense counts
            airDefenseCount.clear();
            groundDefenseCount.clear();
            vector<PlayerState> states ={ PlayerState::Enemy, PlayerState::Self };

            for (auto &[station, state] : stations) {
                Broodwar->drawTextMap(station->getBase()->Center(), "%d", state);
            }

            for (auto &state : states) {
                for (auto &u : Units::getUnits(state)) {
                    auto &unit = *u;
                    if (unit.getType().isBuilding() && (unit.canAttackAir() || unit.canAttackGround())) {
                        auto closestStation = getClosestStationAir(unit.getPosition(), state);
                        if (closestStation && (unit.getPosition().getDistance(closestStation->getBase()->Center()) < 256.0 || closestStation->getDefenses().find(unit.getTilePosition()) != closestStation->getDefenses().end())) {
                            if (unit.canAttackAir())
                                airDefenseCount[closestStation]++;
                            if (unit.canAttackGround())
                                groundDefenseCount[closestStation]++;
                        }
                    }
                }
            }

            for (auto &[station, cnt] : airDefenseCount) {
                Broodwar->drawTextMap(station->getBase()->Center(), "%d", cnt);
            }
        }

        void updateRetreatPositions()
        {
            retreatPositions.clear();

            if (Terrain::getDefendChoke() == BWEB::Map::getMainChoke() && Stations::getStations(PlayerState::Self).size() <= 2) {
                retreatPositions.insert(Terrain::getMyMain());
                return;
            }

            for (auto &station : getStations(PlayerState::Self)) {
                retreatPositions.insert(station);
            }
        }

        void updateDefendPositions()
        {
            for (auto &station : BWEB::Stations::getStations()) {
                auto defendPosition = station.getBase()->Center();
                const BWEM::ChokePoint * defendChoke = nullptr;

                if (station.getChokepoint()) {
                    defendPosition = Position(station.getChokepoint()->Center());
                    defendChoke = station.getChokepoint();
                }
                else if (Terrain::getEnemyStartingPosition().isValid()) {
                    auto path = mapBWEM.GetPath(station.getBase()->Center(), Terrain::getEnemyStartingPosition());
                    if (!path.empty()) {
                        defendPosition = Position(path.front()->Center());
                        defendChoke = path.front();
                    }
                }

                // If there are multiple chokepoints with the same area pair
                auto pathTowards = Terrain::getEnemyStartingPosition().isValid() ? Terrain::getEnemyStartingPosition() : mapBWEM.Center();
                if (station.getBase()->GetArea()->ChokePoints().size() >= 3) {
                    defendPosition = Position(0, 0);
                    int count = 0;

                    for (auto &choke : station.getBase()->GetArea()->ChokePoints()) {
                        if (defendChoke && choke->GetAreas() != defendChoke->GetAreas())
                            continue;

                        if (Position(choke->Center()).getDistance(pathTowards) < station.getBase()->Center().getDistance(pathTowards)) {
                            defendPosition += Position(choke->Center());
                            count++;
                            Visuals::drawCircle(Position(choke->Center()), 4, Colors::Cyan, true);
                        }
                    }
                    if (count > 0)
                        defendPosition /= count;
                    else
                        defendPosition = Position(BWEB::Map::getNaturalChoke()->Center());
                }

                // If defend position isn't walkable, move it towards the closest base
                vector<WalkPosition> directions ={ {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };
                while (Grids::getMobility(defendPosition) <= 2) {
                    auto best = DBL_MAX;
                    auto start = WalkPosition(defendPosition);

                    for (auto &dir : directions) {
                        auto center = Position(start + dir);
                        auto dist = center.getDistance(BWEB::Map::getNaturalPosition());
                        if (dist < best) {
                            defendPosition = center;
                            best = dist;
                        }
                    }
                }
                defendPositions[&station] = defendPosition;
            }
        }

        int calcGroundDefPvP(BWEB::Station * station)
        {
            auto groundCount = getGroundDefenseCount(station);

            if (station->isMain()) {
                if (Spy::enemyInvis())
                    return 1 - groundCount;
            }
            else if (station->isNatural()) {
                if (Spy::enemyInvis())
                    return 2 - groundCount;
            }
            else {
            }
            return 0;
        }

        int calcGroundDefPvT(BWEB::Station * station)
        {
            if (station->isMain()) {
            }
            else if (station->isNatural()) {
            }
            else {
            }
            return 0;
        }

        int calcGroundDefPvZ(BWEB::Station * station)
        {
            auto groundCount = getGroundDefenseCount(station);

            if (station->isMain()) {
                if (Spy::getEnemyTransition().find("Muta") != string::npos)
                    return 3 - groundCount;
            }
            else if (station->isNatural()) {
                if (Spy::getEnemyTransition().find("Muta") != string::npos)
                    return 2 - groundCount;
            }
            else {
            }
            return 0;
        }

        int calcGroundDefZvP(BWEB::Station * station)
        {
            auto groundCount = getGroundDefenseCount(station);

            if (station->isMain()) {
                if (Spy::enemyProxy() && Spy::getEnemyBuild() == "2Gate")
                    return (2 * (Util::getTime() > Time(2, 20))) - groundCount;
                if (BuildOrder::isProxy() && BuildOrder::getCurrentTransition() == "2HatchLurker")
                    return (Util::getTime() > Time(2, 45)) + (Util::getTime() > Time(3, 00)) + (Util::getTime() > Time(3, 30)) + (Util::getTime() > Time(4, 15)) - groundCount;
            }
            else if (station->isNatural()) {
            }
            else {
                if (BuildOrder::getCurrentTransition().find("2Hatch") != string::npos)
                    return 1 - groundCount;
                if (BuildOrder::getCurrentTransition().find("2Hatch") == string::npos)
                    return (Util::getTime() > Time(6, 00)) + (Util::getTime() > Time(7, 00)) - groundCount;
                return (Util::getTime() > Time(7, 30)) + (Util::getTime() > Time(8, 00)) - groundCount;
            }
            return 0;
        }

        int calcGroundDefZvT(BWEB::Station * station)
        {
            auto groundCount = getGroundDefenseCount(station);

            if (station->isMain()) {
                if (Players::getTotalCount(PlayerState::Enemy, Terran_Dropship) > 0)
                    return (Util::getTime() > Time(11, 00)) + (Util::getTime() > Time(15, 00)) - groundCount;
            }
            else if (station->isNatural()) {
            }
            else {
                if (Util::getTime() > Time(4, 15) && (Spy::getEnemyTransition() == "2Fact" || Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0 || Spy::getEnemyBuild() == "RaxFact" || Spy::enemyWalled()))
                    return 1 - groundCount;
                return (Util::getTime() > Time(7, 30)) + (Util::getTime() > Time(8, 00)) - groundCount;
            }
            return 0;
        }

        int calcGroundDefZvZ(BWEB::Station * station)
        {
            auto groundCount = getGroundDefenseCount(station);
            auto desiredDefenses = 0;

            if (station->isMain()) {
                if (BuildOrder::takeNatural() || getStations(PlayerState::Self).size() > 1)
                    return 0;

                if (Util::getTime() > Time(4, 30))
                    groundCount += Players::getVisibleCount(PlayerState::Enemy, Zerg_Sunken_Colony);

                // 4 Pool
                if (Spy::getEnemyOpener() == "4Pool")
                    desiredDefenses = max(desiredDefenses, 1 + (vis(Zerg_Drone) >= 8 && com(Zerg_Sunken_Colony) >= 1));

                // 7 Pool
                if (Spy::getEnemyOpener() == "7Pool" && Spy::getEnemyTransition() != "1HatchMuta")
                    desiredDefenses = max(desiredDefenses, 1);

                // 12 Pool
                if (Spy::getEnemyOpener() == "12Pool" && Spy::getEnemyTransition() != "1HatchMuta")
                    desiredDefenses = max(desiredDefenses, int(Util::getTime() > Time(4, 00)));

                // 10 Hatch or speedling all-in
                if ((Spy::getEnemyOpener() == "10Hatch" || Spy::getEnemyTransition() == "2HatchSpeedling") && vis(Zerg_Spire) > 0)
                    desiredDefenses = max(desiredDefenses, (Util::getTime() > Time(3, 30)) + (Util::getTime() > Time(4, 30)));

                // +1Ling
                if (Spy::getEnemyTransition() == "+1Ling")
                    desiredDefenses = max(desiredDefenses, (Util::getTime() > Time(4, 15)) + (Util::getTime() > Time(4, 45)));

                // 3 Hatch
                if (Util::getTime() < Time(6, 00) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) >= 3)
                    desiredDefenses = max(desiredDefenses, (Util::getTime() > Time(4, 00)) + (Util::getTime() > Time(5, 00)) + (Util::getTime() > Time(6, 00)));

                // Unknown
                if (!Terrain::foundEnemy() && vis(Zerg_Spire) > 0 && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 16)
                    desiredDefenses = max(desiredDefenses, 1);

                // Unknown and lots of lings
                if (Spy::getEnemyTransition().find("Muta") == string::npos && Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) >= 16)
                    desiredDefenses = max(desiredDefenses, 1);

                // Unknown and lots of lings
                if (Util::getTime() > Time(5, 00) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) > 4 * vis(Zerg_Zergling))
                    desiredDefenses = max(desiredDefenses, 1);
            }
            else if (station->isNatural()) {
            }
            else {
            }
            return desiredDefenses - groundCount;
        }

        int calcGroundDefZvFFA(BWEB::Station * station)
        {
            auto groundCount = getGroundDefenseCount(station);

            if (Players::ZvFFA() && !station->isMain() && !station->isNatural())
                return 2 - groundCount;
        }
    }

    void onFrame() {
        Visuals::startPerfTest();
        updateSaturation();
        updateProduction();
        updateStationDefenses();
        updateRetreatPositions();
        updateDefendPositions();
        Visuals::endPerfTest("Stations");
    }

    void onStart() {
        for (auto &station : BWEB::Stations::getStations()) {
            for (auto &mineral : station.getBase()->Minerals())
                initialMinerals[&station] += mineral->InitialAmount();
            for (auto &gas : station.getBase()->Geysers())
                initialGas[&station] += gas->InitialAmount();
            stations[&station] = PlayerState::None;
        }
    }

    void storeStation(Unit unit) {
        auto newStation = BWEB::Stations::getClosestStation(unit->getTilePosition());
        if (!newStation
            || !unit->getType().isResourceDepot()
            || unit->getTilePosition() != newStation->getBase()->Location()
            || stations[newStation] != PlayerState::None)
            return;

        // Store station and set resource states if we own this station
        (unit->getPlayer() == Broodwar->self()) ? stations[newStation] = PlayerState::Self : stations[newStation] = PlayerState::Enemy;
        if (unit->getPlayer() == Broodwar->self()) {

            // Store minerals that still exist
            for (auto &mineral : newStation->getBase()->Minerals()) {
                if (!mineral || !mineral->Unit())
                    continue;
                if (Broodwar->getFrameCount() == 0)
                    Resources::storeResource(mineral->Unit());
            }

            // Store geysers that still exist
            for (auto &geyser : newStation->getBase()->Geysers()) {
                if (!geyser || !geyser->Unit())
                    continue;
                if (Broodwar->getFrameCount() == 0)
                    Resources::storeResource(geyser->Unit());
            }
        }

        // Add any territory it is in
        Terrain::addTerritory(unit->getPlayer() == Broodwar->self() ? PlayerState::Self : PlayerState::Enemy, newStation);
    }

    void removeStation(Unit unit) {
        auto newStation = BWEB::Stations::getClosestStation(unit->getTilePosition());
        if (!newStation || stations.find(newStation) == stations.end())
            return;
        stations[newStation] = PlayerState::None;

        // Remove workers from any resources on this station
        for (auto &mineral : Resources::getMyMinerals()) {
            if (mineral->getStation() == newStation)
                for (auto &w : mineral->targetedByWhat()) {
                    if (auto worker = w.lock()) {
                        worker->setResource(nullptr);
                        mineral->removeTargetedBy(worker);
                    }
                }
        }
        for (auto &gas : Resources::getMyGas()) {
            if (gas->getStation() == newStation)
                for (auto &w : gas->targetedByWhat()) {
                    if (auto worker = w.lock()) {
                        worker->setResource(nullptr);
                        gas->removeTargetedBy(worker);
                    }
                }
        }

        // Remove any territory it was in
        Terrain::removeTerritory(unit->getPlayer() == Broodwar->self() ? PlayerState::Self : PlayerState::Enemy, newStation);
    }

    int needGroundDefenses(BWEB::Station * station) {

        if (BuildOrder::isRush()
            || BuildOrder::isPressure()
            || Spy::getEnemyTransition() == "Carriers")
            return 0;

        if (Players::PvP())
            return calcGroundDefPvP(station);
        if (Players::PvT())
            return calcGroundDefPvT(station);
        if (Players::PvZ())
            return calcGroundDefPvZ(station);
        if (Players::ZvP())
            return calcGroundDefZvP(station);
        if (Players::ZvT())
            return calcGroundDefZvT(station);
        if (Players::ZvZ())
            return calcGroundDefZvZ(station);
        return 0;
    }

    int needAirDefenses(BWEB::Station * station) {
        auto airCount = getAirDefenseCount(station);
        const auto enemyAir = Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) > 0
            || Players::getTotalCount(PlayerState::Enemy, Protoss_Scout) > 0
            || Players::getTotalCount(PlayerState::Enemy, Protoss_Stargate) > 0
            || Players::getTotalCount(PlayerState::Enemy, Terran_Wraith) > 0
            || Players::getTotalCount(PlayerState::Enemy, Terran_Valkyrie) > 0
            || Players::getTotalCount(PlayerState::Enemy, Zerg_Mutalisk) > 0
            || (Players::getTotalCount(PlayerState::Enemy, Zerg_Spire) > 0 && Util::getTime() > Time(4, 45));

        if (Broodwar->self()->getRace() == Races::Zerg) {
            if (Players::ZvZ() && total(Zerg_Zergling) > Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) && com(Zerg_Spire) == 0 && Util::getTime() > Time(4, 30) && Spy::getEnemyTransition() == "Unknown" && BuildOrder::getCurrentTransition() == "2HatchMuta")
                return 1 + (Util::getTime() > Time(5, 15)) - airCount;
            if (Players::ZvZ() && Util::getTime() > Time(4, 15) && Spy::getEnemyTransition() == "1HatchMuta" && BuildOrder::getCurrentTransition() != "1HatchMuta")
                return 1 - airCount;
            if (Players::ZvP() && Util::getTime() > Time(4, 35) && !station->isMain() && Spy::getEnemyBuild() == "1GateCore" && Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) == 0 && (Spy::getEnemyTransition() == "Corsair" || (Spy::getEnemyTransition() == "Unknown"&& Players::getTotalCount(PlayerState::Enemy, Protoss_Gateway) <= 1)))
                return 1 - airCount;
            if (Players::ZvP() && Util::getTime() > Time(5, 00) && !station->isMain() && Spy::getEnemyBuild() == "2Gate" && Spy::getEnemyTransition() == "Corsair" && BuildOrder::getCurrentTransition() == "3HatchMuta")
                return 1 - airCount;
        }
        return 0;
    }

    bool needPower(BWEB::Station * station) {
        auto count = 0;
        for (auto &defense : station->getDefenses()) {
            if (Pylons::hasPowerSoon(defense, UnitTypes::Protoss_Photon_Cannon))
                count++;
        }
        return count < 2;
    }

    bool isBaseExplored(BWEB::Station * station) {
        auto botRight = station->getBase()->Location() + TilePosition(3, 2);
        return (Broodwar->isExplored(station->getBase()->Location()) && Broodwar->isExplored(botRight));
    }

    bool isGeyserExplored(BWEB::Station * station) {
        for (auto &geyser : Resources::getMyGas()) {
            if (!Broodwar->isExplored(geyser->getTilePosition()) || !Broodwar->isExplored(geyser->getTilePosition() + TilePosition(3, 1)))
                return false;
        }
        return true;
    }

    bool isCompleted(BWEB::Station * station)
    {
        // TODO: This is really slow
        const auto base = Util::getClosestUnit(station->getBase()->Center(), PlayerState::Self, [&](auto &u) {
            return u->getType().isResourceDepot();
        });
        return base && base->unit()->isCompleted();
    }

    int lastVisible(BWEB::Station * station) {
        auto botRight = station->getBase()->Location() + TilePosition(4, 3);
        return min(Grids::lastVisibleFrame(station->getBase()->Location()), Grids::lastVisibleFrame(botRight));
    }

    double getSaturationRatio(BWEB::Station * station)
    {
        for (auto &[r, s] : stationsBySaturation) {
            if (s == station)
                return r;
        }
        return 0.0;
    }

    PlayerState ownedBy(BWEB::Station * station)
    {
        return stations[station];
    }

    BWEB::Station * getClosestRetreatStation(UnitInfo& unit)
    {
        auto distBest = DBL_MAX;
        auto bestStation = Terrain::getMyMain();
        for (auto &station : retreatPositions) {
            auto position = Stations::getDefendPosition(station);

            // If this is a main, check if we own a natural that isn't under attack
            // TODO: Check if under attack
            if (station->isMain()) {
                const auto closestNatural = BWEB::Stations::getClosestNaturalStation(station->getBase()->Location());
                if (closestNatural && Stations::ownedBy(closestNatural) == PlayerState::Self)
                    continue;
            }

            // Check if enemies are closer
            if (unit.hasSimTarget() && !unit.isFlying() && !Terrain::inTerritory(PlayerState::Self, unit.getPosition()) && unit.getSimTarget().lock()->getPosition().getDistance(position) < unit.getPosition().getDistance(position))
                continue;

            auto dist = position.getDistance(unit.getPosition());
            if (dist < distBest) {
                bestStation = station;
                distBest = dist;
            }
        }
        return bestStation;
    }

    std::vector<BWEB::Station*> getStations(PlayerState player) {
        return getStations(player, [](auto) {
            return true;
        });
    }

    template<typename F>
    std::vector<BWEB::Station*> getStations(PlayerState player, F &&pred) {
        vector<BWEB::Station*> returnVector;
        for (auto &[station, stationPlayer] : stations) {
            if (stationPlayer == player && pred(station))
                returnVector.push_back(station);
        }
        return returnVector;
    }

    Position getDefendPosition(BWEB::Station * station) { return defendPositions[station]; }
    multimap<double, BWEB::Station *>& getStationsBySaturation() { return stationsBySaturation; }
    multimap<double, BWEB::Station *>& getStationsByProduction() { return stationsByProduction; }
    int getGasingStationsCount() { return gasingStations; }
    int getMiningStationsCount() { return miningStations; }
    int getGroundDefenseCount(BWEB::Station * station) { return groundDefenseCount[station]; }
    int getAirDefenseCount(BWEB::Station * station) { return airDefenseCount[station]; }
    int getMineralsRemaining(BWEB::Station * station) { return remainingMinerals[station]; }
    int getGasRemaining(BWEB::Station * station) { return remainingGas[station]; }
    int getMineralsInitial(BWEB::Station * station) { return initialMinerals[station]; }
    int getGasInitial(BWEB::Station * station) { return initialGas[station]; }
}