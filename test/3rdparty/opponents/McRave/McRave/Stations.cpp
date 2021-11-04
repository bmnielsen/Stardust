#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Stations {

    namespace {
        vector<BWEB::Station *> myStations, enemyStations;
        multimap<double, BWEB::Station *> stationsBySaturation;
        map<BWEB::Station *, int> airDefenseCount, groundDefenseCount;

        void updateSaturation() {

            // Sort stations by saturation and current larva count
            stationsBySaturation.clear();
            for (auto &station : Stations::getMyStations()) {
                auto workerCount = 0;
                auto resourceCount = 0;
                for (auto &mineral : Resources::getMyMinerals()) {
                    if (mineral->getStation() == station) {
                        resourceCount++;
                        workerCount+=mineral->targetedByWhat().size();
                    }
                }
                for (auto &gas : Resources::getMyGas()) {
                    if (gas->getStation() == station) {
                        resourceCount++;
                        workerCount+=gas->targetedByWhat().size();
                    }
                }

                // Order by lowest saturation first
                auto saturatedLevel = workerCount > 0 ? double(workerCount) / double(resourceCount) : 0.0;
                stationsBySaturation.emplace(saturatedLevel, station);
            }
        }

        void updateStationDefenses() {

            // Calculate defense counts
            airDefenseCount.clear();
            groundDefenseCount.clear();
            vector<PlayerState> states ={ PlayerState::Enemy, PlayerState::Self };

            for (auto &state : states) {
                for (auto &u : Units::getUnits(state)) {
                    auto &unit = *u;
                    if (unit.getType().isBuilding() && (unit.canAttackAir() || unit.canAttackGround())) {
                        auto closestStation = Stations::getClosestStationAir(state, unit.getPosition());
                        if (closestStation && (unit.getPosition().getDistance(closestStation->getBase()->Center()) < 256.0 || closestStation->getDefenses().find(unit.getTilePosition()) != closestStation->getDefenses().end())) {
                            if (unit.canAttackAir())
                                airDefenseCount[closestStation]++;
                            if (unit.canAttackGround())
                                groundDefenseCount[closestStation]++;
                        }
                    }
                }
            }
        }
    }

    void onFrame() {
        Visuals::startPerfTest();
        updateSaturation();
        updateStationDefenses();
        Visuals::endPerfTest("Stations");
    }

    void onStart() {

    }

    BWEB::Station * getClosestStationAir(PlayerState pState, Position here) {
        auto &list = pState == PlayerState::Self ? myStations : enemyStations;
        auto distBest = DBL_MAX;
        BWEB::Station * closestStation = nullptr;
        for (auto &station : list) {
            double dist = here.getDistance(station->getBase()->Center());
            if (dist < distBest) {
                closestStation = station;
                distBest = dist;
            }
        }
        return closestStation;
    }

    BWEB::Station * getClosestStationGround(PlayerState pState, Position here) {
        auto &list = pState == PlayerState::Self ? myStations : enemyStations;
        auto distBest = DBL_MAX;
        BWEB::Station * closestStation = nullptr;
        for (auto &station : list) {
            double dist = BWEB::Map::getGroundDistance(here, station->getBase()->Center());
            if (dist < distBest) {
                closestStation = station;
                distBest = dist;
            }
        }
        return closestStation;
    }

    void storeStation(Unit unit) {
        auto newStation = BWEB::Stations::getClosestStation(unit->getTilePosition());
        if (!newStation
            || !unit->getType().isResourceDepot()
            || unit->getTilePosition() != newStation->getBase()->Location())
            return;

        auto &list = (unit->getPlayer() == Broodwar->self()) ? myStations : enemyStations;
        auto &existing = find(list.begin(), list.end(), newStation);
        if (existing != list.end() && *existing == newStation)
            return;

        // Store station and set resource states if we own this station
        (unit->getPlayer() == Broodwar->self()) ? myStations.push_back(newStation) : enemyStations.push_back(newStation);
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

        // Add stations area to territory tracking
        if (unit->getTilePosition().isValid() && mapBWEM.GetArea(unit->getTilePosition())) {
            if (unit->getPlayer() == Broodwar->self())
                Terrain::getAllyTerritory().insert(mapBWEM.GetArea(unit->getTilePosition()));
            else
                Terrain::getEnemyTerritory().insert(mapBWEM.GetArea(unit->getTilePosition()));
        }
    }

    void removeStation(Unit unit) {
        auto newStation = BWEB::Stations::getClosestStation(unit->getTilePosition());
        if (!newStation)
            return;

        auto &list = unit->getPlayer() == Broodwar->self() ? myStations : enemyStations;
        auto &existing = find(list.begin(), list.end(), newStation);
        if (existing == list.end())
            return;

        list.erase(existing);

        // Remove workers from any resources on this station
        for (auto &mineral : Resources::getMyMinerals()) {
            if (mineral->getStation() == newStation)
                for (auto &worker : mineral->targetedByWhat()) {
                    if (!worker.expired()) {
                        worker.lock()->setResource(nullptr);
                        mineral->removeTargetedBy(worker);
                    }
                }
        }
        for (auto &gas : Resources::getMyGas()) {
            if (gas->getStation() == newStation)
                for (auto &worker : gas->targetedByWhat()) {
                    if (!worker.expired()) {
                        worker.lock()->setResource(nullptr);
                        gas->removeTargetedBy(worker);
                    }
                }
        }

        // Remove any territory it was in
        if (unit->getTilePosition().isValid() && mapBWEM.GetArea(unit->getTilePosition())) {
            if (unit->getPlayer() == Broodwar->self())
                Terrain::getAllyTerritory().erase(mapBWEM.GetArea(unit->getTilePosition()));
            else
                Terrain::getEnemyTerritory().erase(mapBWEM.GetArea(unit->getTilePosition()));
        }
    }

    int needGroundDefenses(BWEB::Station * station) {
        auto groundCount = getGroundDefenseCount(station);

        if (BuildOrder::isRush() || BuildOrder::isPressure())
            return 0;

        // In mirror matchup, we can delay sunkens if they make sunkens
        if (Players::ZvZ()) {
            if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Sunken_Colony) >= 2 && Util::getTime() < Time(4, 30))
                groundCount++;
        }

        // Grab total and current counts of minerals remaining for this base
        auto initial = 0;
        auto current = 0;
        for (auto &mineral : station->getBase()->Minerals()) {
            if (mineral && mineral->Unit()->exists()) {
                initial += mineral->InitialAmount();
                current += mineral->Amount();
            }
        }
        for (auto &gas : station->getBase()->Geysers()) {
            if (gas && gas->Unit()->exists()) {
                initial += gas->InitialAmount();
                current += gas->Amount();
            }
        }

        // Main defenses
        if (station->isMain()) {

            if (Broodwar->self()->getRace() == Races::Protoss) {
                if (Stations::needPower(station))
                    return 0;
                if (Players::PvP() && Strategy::enemyInvis())
                    return 1 - groundCount;
                if (Players::PvZ() && (Strategy::getEnemyTransition().find("Muta") != string::npos))
                    return 3 - groundCount;
            }
            if (Broodwar->self()->getRace() == Races::Zerg) {
                if (Players::ZvZ() && !BuildOrder::takeNatural() && int(Stations::getMyStations().size()) <= 1) {
                    if (BuildOrder::getCurrentTransition().find("Muta") == string::npos)
                        groundCount += 2;

                    if (Strategy::getEnemyOpener() == "7Pool" && BuildOrder::getCurrentOpener() == "12Pool")
                        return 1 - groundCount;

                    else if (Strategy::getEnemyTransition() == "2HatchSpeedling" && vis(Zerg_Spire) > 0 && !Strategy::enemyFastExpand())
                        return (Util::getTime() > Time(3, 15)) + (Util::getTime() > Time(3, 30)) + (Util::getTime() > Time(5, 00)) - groundCount;
                    else if (Strategy::getEnemyTransition() == "+1Ling")
                        return (Util::getTime() > Time(4, 15)) + (Util::getTime() > Time(4, 45)) - groundCount;
                    else if (Util::getTime() < Time(5, 30) && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 40)
                        return 6 - groundCount;
                    else if (Util::getTime() < Time(4, 45) && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 26)
                        return 4 - groundCount;
                    else if (Util::getTime() < Time(6, 00) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) >= 3)
                        return 4 - groundCount;
                    else if (Strategy::enemyPressure())
                        return (Util::getTime() > Time(3, 45)) + (vis(Zerg_Sunken_Colony) > 0) + (vis(Zerg_Drone) >= 8 && com(Zerg_Sunken_Colony) >= 2) - groundCount;
                    else if (Strategy::enemyRush() && (total(Zerg_Zergling) >= 12 || BuildOrder::getCurrentBuild() != "PoolLair"))
                        return 1 + (vis(Zerg_Sunken_Colony) > 0) + (vis(Zerg_Drone) >= 8 && com(Zerg_Sunken_Colony) >= 2) - groundCount;
                    else if (!Terrain::foundEnemy() && vis(Zerg_Spire) > 0 && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 16)
                        return 1 - groundCount;
                    else if (Strategy::getEnemyTransition().find("Muta") == string::npos && Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) >= 16)
                        return 1 - groundCount;
                    else if (Util::getTime() > Time(5, 00) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling) > 4 * vis(Zerg_Zergling))
                        return 1 - groundCount;
                }

                if (Players::ZvP()) {
                    if (Strategy::enemyProxy() && Strategy::getEnemyBuild() == "2Gate")
                        return (Util::getTime() > Time(2, 00)) + (Util::getTime() > Time(2, 30)) - groundCount;
                    if (BuildOrder::isProxy() && BuildOrder::getCurrentTransition() == "2HatchLurker")
                        return (Util::getTime() > Time(2, 45)) + (Util::getTime() > Time(3, 00)) + (Util::getTime() > Time(3, 30)) + (Util::getTime() > Time(4, 15)) - groundCount;
                }

                if (Players::ZvT()) {
                    if (Players::getTotalCount(PlayerState::Enemy, Terran_Dropship) > 0)
                        return (Util::getTime() > Time(11, 00)) + (Util::getTime() > Time(15, 00)) - groundCount;
                    if (Strategy::getEnemyTransition() == "2Fact" || Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0 || Strategy::getEnemyBuild() == "RaxFact")
                        return (Util::getTime() > Time(4, 15)) - groundCount;
                }
            }
            return 0;
        }

        // Natural defenses
        else if (station->isNatural()) {
            if (Players::PvP() && Strategy::enemyInvis())
                return 2 - groundCount;
            if (Players::PvZ() && (Strategy::getEnemyTransition() == "2HatchMuta" || Strategy::getEnemyTransition() == "3HatchMuta"))
                return 2 - groundCount;

            if (Broodwar->self()->getRace() == Races::Protoss) {
                const auto percentage = double(current) / double(initial);
                const auto desired = (percentage >= 0.75) + (percentage >= 0.5) + (percentage >= 0.25) - (Stations::getMyStations().size() <= 4) - (Stations::getMyStations().size() <= 5) + (Util::getTime() > Time(15, 0));
                return desired - groundCount;
            }
            return 0;
        }

        // Calculate percentage remaining and determine desired resources for this base
        else {
            if (Strategy::getEnemyTransition() == "Carriers")
                return 0;

            // 2 Hatch
            if (Players::ZvP() && BuildOrder::getCurrentTransition().find("2Hatch") != string::npos)
                return 1 - groundCount;

            // Other
            if (Players::ZvP() && BuildOrder::getCurrentTransition().find("2Hatch") == string::npos && Util::getTime() > Time(5, 00))
                return 2 - groundCount;

            if ((Players::ZvT() && Util::getTime() > Time(16, 00)) || Players::ZvP()) {
                auto chokeCount = max(2, int(station->getBase()->GetArea()->ChokePoints().size()));
                auto resourceCount = 0;
                auto droneCount = 0;
                for (auto &mineral : Resources::getMyMinerals()) {
                    if (mineral->getStation() == station) {
                        droneCount += int(mineral->targetedByWhat().size());
                        resourceCount++;
                    }
                }
                for (auto &gas : Resources::getMyGas()) {
                    if (gas->getStation() == station) {
                        droneCount += int(gas->targetedByWhat().size());
                        resourceCount++;
                    }
                }
                auto saturationRatio = resourceCount > 0 ? double(droneCount) / double(resourceCount) : 0.0;
                return (Util::getTime() > Time(7, 30)) + (Util::getTime() > Time(8, 00)) - groundCount;
            }
            if (Players::ZvT() && Util::getTime() > Time(4, 15) && (Strategy::getEnemyTransition() == "2Fact" || Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0 || Strategy::getEnemyBuild() == "RaxFact" || Strategy::enemyWalled()))
                return 1 - groundCount;

            if (Broodwar->self()->getRace() == Races::Protoss) {
                const auto percentage = double(current) / double(initial);
                const auto desired = max(2, (percentage >= 0.75) + (percentage >= 0.5) + (percentage >= 0.25) - (Stations::getMyStations().size() <= 4) - (Stations::getMyStations().size() <= 5) + (Util::getTime() > Time(15, 0)));
                return desired - groundCount;
            }
        }
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
            if (Players::ZvZ() && total(Zerg_Zergling) > Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) && com(Zerg_Spire) == 0 && Util::getTime() > Time(4, 30) && Strategy::getEnemyTransition() == "Unknown" && BuildOrder::getCurrentTransition() == "2HatchMuta")
                return 1 + (Util::getTime() > Time(5, 15)) - airCount;
            if (Players::ZvZ() && Util::getTime() > Time(4, 15) && Strategy::getEnemyTransition() == "1HatchMuta" && BuildOrder::getCurrentTransition() != "1HatchMuta")
                return 1 - airCount;
            if (Players::ZvP() && Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) <= 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) > 0 && vis(Zerg_Lair) == 0 && vis(Zerg_Hydralisk_Den) == 0)
                return 1 - airCount;
        }
        return 0;
    }

    int getGroundDefenseCount(BWEB::Station * station) {
        return groundDefenseCount[station];
    }

    int getAirDefenseCount(BWEB::Station * station) {
        return airDefenseCount[station];
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
            return u.getType().isResourceDepot();
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
        if (!station || BWEB::Map::isUsed(station->getBase()->Location(), 4, 3) == UnitTypes::None)
            return PlayerState::None;

        for (auto &s : myStations) {
            if (s == station)
                return PlayerState::Self;
        }
        for (auto &s : enemyStations) {
            if (s == station)
                return PlayerState::Enemy;
        }
        return PlayerState::None;
    }

    Position getDefendPosition(BWEB::Station * station)
    {
        auto defendPosition = Positions::Invalid;
        const BWEM::ChokePoint * defendChoke = nullptr;
        if (station->getChokepoint()) {
            defendPosition = Position(station->getChokepoint()->Center());
            defendChoke = station->getChokepoint();
        }
        else if (Terrain::getEnemyStartingPosition().isValid()) {
            auto path = mapBWEM.GetPath(station->getBase()->Center(), Terrain::getEnemyStartingPosition());
            if (!path.empty()) {
                defendPosition = Position(path.front()->Center());
                defendChoke = path.front();
            }
        }

        // If there are multiple chokepoints with the same area pair
        auto pathTowards = Terrain::getEnemyStartingPosition().isValid() ? Terrain::getEnemyStartingPosition() : mapBWEM.Center();
        if (station->getBase()->GetArea()->ChokePoints().size() >= 3) {
            defendPosition = Position(0, 0);
            int count = 0;

            for (auto &choke : station->getBase()->GetArea()->ChokePoints()) {
                if (choke->GetAreas() != defendChoke->GetAreas())
                    continue;

                if (Position(choke->Center()).getDistance(pathTowards) < station->getBase()->Center().getDistance(pathTowards)) {
                    defendPosition += Position(choke->Center());
                    count++;
                    Visuals::drawCircle(Position(choke->Center()), 4, Colors::Cyan, true);
                }
            }
            if (count > 0)
                defendPosition /= count;
            else
                defendPosition = Position(BWEB::Map::getNaturalChoke()->Center()) + Position(4, 4);
        }

        // If defend position isn't walkable, move it towards the closest base
        vector<WalkPosition> directions ={ {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };
        while (Grids::getMobility(defendPosition) <= 2) {
            auto best = DBL_MAX;
            auto start = WalkPosition(defendPosition);

            for (auto &dir : directions) {
                auto center = Position(start + dir) + Position(4, 4);
                auto dist = center.getDistance(BWEB::Map::getNaturalPosition());
                if (dist < best) {
                    defendPosition = center;
                    best = dist;
                }
            }
        }
        return defendPosition;
    }

    vector<BWEB::Station *>& getMyStations() { return myStations; };
    vector<BWEB::Station *>& getEnemyStations() { return enemyStations; }
    multimap<double, BWEB::Station *>& getStationsBySaturation() { return stationsBySaturation; }
}