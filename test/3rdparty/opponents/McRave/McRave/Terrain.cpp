#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace BWEM;
using namespace UnitTypes;

namespace McRave::Terrain {

    namespace {
        set<const Area*> allyTerritory;
        set<const Area*> enemyTerritory;
        set<const Base *> allBases;
        BWEB::Station * enemyNatural = nullptr;
        BWEB::Station * enemyMain = nullptr;
        BWEB::Station * myNatural = nullptr;
        BWEB::Station * myMain = nullptr;
        Position enemyStartingPosition = Positions::Invalid;
        Position defendPosition = Positions::Invalid;
        Position attackPosition = Positions::Invalid;
        Position harassPosition = Positions::Invalid;
        TilePosition enemyStartingTilePosition = TilePositions::Invalid;
        TilePosition enemyExpand = TilePositions::Invalid;
        const ChokePoint * defendChoke = nullptr;
        const Area * defendArea = nullptr;
        map<BWEB::Station *, Position> safeSpots;

        bool shitMap = false;
        bool islandMap = false;
        bool reverseRamp = false;
        bool flatRamp = false;
        bool narrowNatural = false;
        bool defendNatural = false;
        bool abandonNatural = false;
        int timeHarassingHere = 1500;

        void findEnemy()
        {
            if (enemyStartingPosition.isValid()) {
                if (Broodwar->isExplored(enemyStartingTilePosition) && Util::getTime() < Time(8, 00) && BWEB::Map::isUsed(enemyStartingTilePosition) == UnitTypes::None) {
                    enemyStartingPosition = Positions::Invalid;
                    enemyStartingTilePosition = TilePositions::Invalid;
                    enemyNatural = nullptr;
                    enemyMain = nullptr;
                    Stations::getEnemyStations().clear();
                }
                else
                    return;
            }

            // Find closest enemy building
            for (auto &u : Units::getUnits(PlayerState::Enemy)) {
                UnitInfo &unit = *u;
                if (!unit.getType().isBuilding()
                    || !unit.getTilePosition().isValid()
                    || unit.isFlying()
                    || unit.isProxy())
                    continue;

                const auto closestMain = BWEB::Stations::getClosestMainStation(unit.getTilePosition());

                // Set start if valid
                if (closestMain && closestMain != myMain) {
                    enemyStartingTilePosition = closestMain->getBase()->Location();
                    enemyStartingPosition = Position(enemyStartingTilePosition) + Position(64, 48);
                }
            }

            // Infer based on enemy Overlord
            if (Players::vZ() && Util::getTime() < Time(2, 15)) {
                for (auto &u : Units::getUnits(PlayerState::Enemy)) {
                    UnitInfo &unit = *u;
                    auto frameDiffBest = DBL_MAX;
                    auto tileBest = TilePositions::Invalid;

                    if (unit.getType() != Zerg_Overlord)
                        continue;

                    for (auto &start : Broodwar->getStartLocations()) {
                        auto startCenter = Position(start) + Position(64, 48);
                        auto frameDiff = abs(Broodwar->getFrameCount() - (unit.getPosition().getDistance(startCenter) / unit.getSpeed()));

                        if (frameDiff < 240 && frameDiff < frameDiffBest) {
                            frameDiffBest = frameDiff;
                            tileBest = start;
                        }
                    }

                    // Set start if valid
                    if (tileBest.isValid() && tileBest != BWEB::Map::getMainTile()) {
                        enemyStartingPosition = Position(tileBest) + Position(64, 48);
                        enemyStartingTilePosition = tileBest;
                    }
                }
            }

            // Assume based on only 1 remaining location
            vector<TilePosition> unexploredStarts;
            if (!enemyStartingPosition.isValid()) {
                for (auto &topLeft : mapBWEM.StartingLocations()) {
                    auto botRight = topLeft + TilePosition(3, 2);
                    if (!Broodwar->isExplored(botRight) || !Broodwar->isExplored(topLeft))
                        unexploredStarts.push_back(topLeft);
                }

                if (int(unexploredStarts.size()) == 1) {
                    enemyStartingPosition = Position(unexploredStarts.front()) + Position(64, 48);
                    enemyStartingTilePosition = unexploredStarts.front();
                }
            }

            // Locate Stations
            if (enemyStartingPosition.isValid()) {
                enemyMain = BWEB::Stations::getClosestMainStation(enemyStartingTilePosition);
                enemyNatural = BWEB::Stations::getClosestNaturalStation(enemyStartingTilePosition);

                if (enemyMain) {
                    enemyTerritory.insert(enemyMain->getBase()->GetArea());
                    Stations::getEnemyStations().push_back(enemyMain);
                }
            }
        }

        void findEnemyNextExpand()
        {
            if (!enemyStartingPosition.isValid())
                return;

            double best = 0.0;
            for (auto &station : BWEB::Stations::getStations()) {

                // If station is used
                if (BWEB::Map::isUsed(station.getBase()->Location()) != None
                    || enemyStartingTilePosition == station.getBase()->Location()
                    || !station.getBase()->GetArea()->AccessibleFrom(BWEB::Map::getMainArea())
                    || station == enemyNatural)
                    continue;

                // Get value of the expansion
                double value = 0.0;
                for (auto &mineral : station.getBase()->Minerals())
                    value += double(mineral->Amount());
                for (auto &gas : station.getBase()->Geysers())
                    value += double(gas->Amount());
                if (station.getBase()->Geysers().size() == 0)
                    value = value / 10.0;

                // Get distance of the expansion
                double distance;
                if (!station.getBase()->GetArea()->AccessibleFrom(BWEB::Map::getMainArea()))
                    distance = log(station.getBase()->Center().getDistance(enemyStartingPosition));
                else
                    distance = BWEB::Map::getGroundDistance(enemyStartingPosition, station.getBase()->Center()) / (BWEB::Map::getGroundDistance(BWEB::Map::getMainPosition(), station.getBase()->Center()));

                double score = value / distance;

                if (score > best) {
                    best = score;
                    enemyExpand = (TilePosition)station.getBase()->Center();
                }
            }
        }

        void findAttackPosition()
        {
            // Attack furthest enemy station
            attackPosition = enemyStartingPosition;
            auto distBest = 0.0;
            auto posBest = Positions::Invalid;

            for (auto &station : Stations::getEnemyStations()) {
                const auto dist = enemyStartingPosition.getDistance(station->getBase()->Center());
                if (dist < distBest
                    || BWEB::Map::isUsed(station->getBase()->Location()) == UnitTypes::None)
                    continue;

                const auto stationWalkable = [&](const TilePosition &t) {
                    return Util::rectangleIntersect(Position(station->getBase()->Location()), Position(station->getBase()->Location()) + Position(128, 96), Position(t));
                };

                BWEB::Path path(Position(BWEB::Map::getNaturalChoke()->Center()), station->getBase()->Center(), Zerg_Ultralisk, true, true);
                path.generateJPS([&](auto &t) { return path.unitWalkable(t) || stationWalkable(t); });

                if (path.isReachable()) {
                    distBest = dist;
                    posBest = station->getResourceCentroid();
                }
            }
            attackPosition = posBest;
        }

        void findDefendPosition()
        {
            const auto baseType = Broodwar->self()->getRace().getResourceDepot();
            narrowNatural = BWEB::Map::getNaturalChoke() && BWEB::Map::getNaturalChoke()->Width() <= 64;
            defendNatural = BWEB::Map::getNaturalChoke() && !abandonNatural &&
                (BuildOrder::isWallNat()
                    || BuildOrder::buildCount(baseType) > (1 + !BuildOrder::takeNatural())
                    || Stations::getMyStations().size() >= 2
                    || (!Players::PvZ() && Players::getSupply(PlayerState::Self, Races::Protoss) > 140)
                    || (Broodwar->self()->getRace() != Races::Zerg && reverseRamp));

            auto oldPos = defendPosition;

            auto mainChoke = BWEB::Map::getMainChoke();
            if (shitMap && enemyStartingPosition.isValid())
                mainChoke = mapBWEM.GetPath(BWEB::Map::getMainPosition(), enemyStartingPosition).front();

            // On Alchemist just defend the choke we determine
            if (Terrain::isShitMap() && Walls::getNaturalWall()) {
                defendChoke = Walls::getNaturalWall()->getChokePoint();
                defendArea = Walls::getNaturalWall()->getArea();
                defendPosition = Position(defendChoke->Center());
                allyTerritory.insert(Walls::getNaturalWall()->getArea());
                return;
            }

            // See if a defense is in range of our main choke
            auto defenseCanStopRunyby = Players::ZvT();
            if (defendNatural) {
                auto closestDefense = Util::getClosestUnit(Position(BWEB::Map::getMainChoke()->Center()), PlayerState::Self, [&](auto &u) {
                    return u.getRole() == Role::Defender && u.canAttackGround();
                });
                if (closestDefense && closestDefense->getPosition().getDistance(Position(BWEB::Map::getMainChoke()->Center())) < closestDefense->getGroundRange() + 64.0)
                    defenseCanStopRunyby = true;
            }

            // If we want to defend our mineral line
            if (!Combat::defendChoke() || islandMap) {
                defendNatural = false;
                auto closestStation = BWEB::Stations::getClosestMainStation(BWEB::Map::getMainTile());
                auto closestUnit = Util::getClosestUnit(closestStation->getBase()->Center(), PlayerState::Self, [&](auto &u) {
                    return u.getRole() == Role::Combat;
                });

                if (closestUnit) {
                    auto distBest = DBL_MAX;
                    for (auto &defense : closestStation->getDefenses()) {
                        auto center = Position(defense) + Position(32, 32);
                        if (Planning::overlapsPlan(*closestUnit, center))
                            continue;
                        auto dist = center.getDistance(closestStation->getResourceCentroid());
                        if (dist < distBest) {
                            defendPosition = center;
                            distBest = dist;
                        }
                    }
                }
                else
                    defendPosition = closestStation ? (closestStation->getResourceCentroid() + closestStation->getBase()->Center()) / 2 : BWEB::Map::getMainPosition();
            }

            // If we want to prevent a runby
            else if (Combat::defendChoke() && !flatRamp && com(Zerg_Sunken_Colony) > 0 && !Strategy::enemyPressure() && (defenseCanStopRunyby || Players::ZvT())) {
                defendPosition = Position(mainChoke->Center()) + Position(4, 4);
                defendNatural = false;
            }

            // Natural defending position
            else if (defendNatural) {
                defendPosition = Stations::getDefendPosition(myNatural);
                allyTerritory.insert(BWEB::Map::getNaturalArea());
            }

            // Main defending position
            else {
                allyTerritory.insert(BWEB::Map::getMainArea());
                allyTerritory.erase(BWEB::Map::getNaturalArea()); // Erase just in case we dropped natural defense
                defendPosition = Position(mainChoke->Center()) + Position(4, 4);

                // Check to see if we have a wall
                if (Walls::getMainWall() && BuildOrder::isWallMain()) {
                    Position opening(Walls::getMainWall()->getOpening());
                    defendPosition = opening.isValid() ? opening : Walls::getMainWall()->getCentroid();
                }
            }

            // Natural defending area and choke
            if (defendNatural) {

                // Decide which area is within my territory, useful for maps with small adjoining areas like Andromeda
                auto &[a1, a2] = defendChoke->GetAreas();
                if (a1 && Terrain::isInAllyTerritory(a1))
                    defendArea = a1;
                if (a2 && Terrain::isInAllyTerritory(a2))
                    defendArea = a2;
                defendChoke = BWEB::Map::getNaturalChoke();
            }

            // Main defend area and choke
            else {
                defendChoke = mainChoke;
                defendArea = BWEB::Map::getMainArea();
            }
        }

        void findHarassPosition()
        {
            if (!enemyMain)
                return;

            const auto commanderInRange = [&](Position here) {
                auto commander = Util::getClosestUnit(here, PlayerState::Self, [&](auto &u) {
                    return u.getType() == Zerg_Mutalisk;
                });
                return commander && commander->getPosition().getDistance(here) < 160.0;
            };

            // Check if enemy lost all bases
            auto lostAll = true;
            for (auto &station : Stations::getEnemyStations()) {
                if (!Stations::isBaseExplored(station) || BWEB::Map::isUsed(station->getBase()->Location()) != UnitTypes::None)
                    lostAll = false;
            }
            if (lostAll) {
                harassPosition = Positions::Invalid;
                return;
            }

            // Some hardcoded ZvT stuff
            if (Players::ZvT() && Util::getTime() < Time(7, 00) && enemyMain) {
                harassPosition = enemyMain->getBase()->Center();
                return;
            }
            if (Players::ZvT() && Util::getTime() > Time(10, 00)) {
                auto closestEnemy = Util::getClosestUnit(BWEB::Map::getMainPosition(), PlayerState::Enemy, [&](auto &u) {
                    return u.isThreatening() || (u.getPosition().getDistance(BWEB::Map::getMainPosition()) < u.getPosition().getDistance(Terrain::getEnemyStartingPosition()) && !u.isHidden());
                });
                if (closestEnemy) {
                    harassPosition = closestEnemy->getPosition();
                    return;
                }
            }

            // Switch positions
            auto switchPosition = timeHarassingHere > 2000 || !harassPosition.isValid() || commanderInRange(harassPosition);
            if (switchPosition) {
                timeHarassingHere = 0;
                harassPosition = Positions::Invalid;
            }
            else
                timeHarassingHere++;

            // Can harass if commander not in range and has defenses <= i
            const auto suitableStationToHarass = [&](BWEB::Station * station, int i) {
                return station && !commanderInRange(station->getResourceCentroid()) && Stations::getAirDefenseCount(station) == i && (!Stations::isBaseExplored(station) || BWEB::Map::isUsed(station->getBase()->Location()) != UnitTypes::None);
            };

            // Can check if we haven't visited recently, but we did explore it
            const auto suitableStationToCheck = [&](BWEB::Station * station) {
                return (Stations::isBaseExplored(station) && Stations::lastVisible(station) < Broodwar->getFrameCount() - 720);
            };

            // Create a list of valid positions to harass/check
            set<Position> checkPositions;
            for (int i = 0; i < 4; i++) {

                // Check for harassing
                if (suitableStationToHarass(Terrain::getEnemyMain(), i))
                    checkPositions.insert(Terrain::getEnemyMain()->getResourceCentroid());
                if (suitableStationToHarass(Terrain::getEnemyNatural(), i))
                    checkPositions.insert(Terrain::getEnemyNatural()->getResourceCentroid());
                for (auto &station : Stations::getEnemyStations()) {
                    if (suitableStationToHarass(station, i))
                        checkPositions.insert(station->getResourceCentroid());
                }

                // Check natural periodically
                if (Util::getTime() > Time(6, 00) && suitableStationToCheck(Terrain::getEnemyNatural()))
                    checkPositions.insert(Terrain::getEnemyNatural()->getResourceCentroid());

                if (!checkPositions.empty())
                    break;
            }

            // Set enemy army if our air size is large enough to break contains (need to expand)
            if (vis(Zerg_Mutalisk) >= 36 && Units::getEnemyArmyCenter().isValid() && Units::getEnemyArmyCenter().getDistance(BWEB::Map::getMainPosition()) < Units::getEnemyArmyCenter().getDistance(Terrain::getEnemyStartingPosition()) && BuildOrder::shouldExpand())
                harassPosition = (Units::getEnemyArmyCenter());

            // Harass all stations by last visited
            else if (switchPosition) {
                auto best = -1;
                for (auto &pos : checkPositions) {
                    auto score = Broodwar->getFrameCount() - Grids::lastVisibleFrame(TilePosition(pos));
                    Visuals::drawCircle(pos, 10, Colors::Cyan);
                    if (score > best) {
                        score = best;
                        harassPosition = pos;
                    }
                }
            }
        }

        void findSafeSpots()
        {
            const auto neighborCheck = [&](auto t) {
                for (int x = -3; x <= 3; x++) {
                    for (int y = -3; y <= 3; y++) {
                        TilePosition tile = t + TilePosition(x, y);
                        if (tile.isValid() && BWEB::Map::isWalkable(tile) && (mapBWEM.GetTile(tile).MinAltitude() > 0 || BWEB::Map::isUsed(tile) != UnitTypes::None))
                            return false;
                    }
                }
                return true;
            };

            const auto findBestSafeSpot = [&](BWEB::Station& station, TilePosition start, double &distBest) {
                for (auto x = start.x - 12; x < start.x + 16; x++) {
                    for (auto y = start.y - 12; y < start.y + 15; y++) {
                        const TilePosition tile(x, y);
                        const auto center = Position(tile) + Position(16, 16);

                        if (!tile.isValid()
                            || mapBWEM.GetArea(tile) == station.getBase()->GetArea()
                            || mapBWEM.GetTile(tile).MinAltitude() > 0
                            || Util::boxDistance(Zerg_Overlord, center, Zerg_Hatchery, station.getBase()->Center()) > 320
                            //|| !neighborCheck(tile)
                            )
                            continue;

                        //Visuals::tileBox(tile, Colors::Green);

                        auto dist = center.getDistance(BWEB::Map::getMainPosition()) * center.getDistance(getClosestMapEdge(center)) / center.getDistance(Terrain::getEnemyStartingPosition());
                        //Broodwar->drawTextMap(center, "%.2f", dist);
                        if (dist < distBest) {
                            safeSpots[&station] = center;
                            distBest = dist;
                        }
                    }
                }
            };

            // Create safe spots at each station
            for (auto &station : BWEB::Stations::getStations()) {

                auto distBest = DBL_MAX;
                findBestSafeSpot(station, station.getBase()->Location(), distBest);

                if (station.getChokepoint())
                    findBestSafeSpot(station, TilePosition(station.getChokepoint()->Center()), distBest);
            }

            // Draw
            //for (auto &[station, pos] : safeSpots)
                //Broodwar->drawLineMap(station->getBase()->Center(), pos, Colors::Cyan);
        }

        void updateAreas()
        {
            // Squish areas
            if (BWEB::Map::getNaturalArea()) {

                // Add to territory if chokes are shared
                if (BWEB::Map::getMainChoke() == BWEB::Map::getNaturalChoke() || islandMap)
                    allyTerritory.insert(BWEB::Map::getNaturalArea());

                // Add natural if we plan to take it
                if (BuildOrder::isOpener() && BuildOrder::takeNatural())
                    allyTerritory.insert(BWEB::Map::getNaturalArea());
            }
        }
    }

    Position getClosestMapCorner(Position here)
    {
        vector<Position> mapCorners ={
    {0, 0},
    {0, Broodwar->mapHeight() * 32 },
    {Broodwar->mapWidth() * 32, 0},
    {Broodwar->mapWidth() * 32, Broodwar->mapHeight() * 32}
        };

        auto closestCorner = Positions::Invalid;
        auto closestDist = DBL_MAX;
        for (auto &corner : mapCorners) {
            auto dist = corner.getDistance(here);
            if (dist < closestDist) {
                closestDist = dist;
                closestCorner = corner;
            }
        }
        return closestCorner;
    }

    Position getClosestMapEdge(Position here)
    {
        vector<Position> mapEdges ={
    {here.x, 0},
    {here.x, Broodwar->mapHeight() * 32 },
    {0, here.y},
    {Broodwar->mapWidth() * 32, here.y}
        };

        auto closestCorner = Positions::Invalid;
        auto closestDist = DBL_MAX;
        for (auto &corner : mapEdges) {
            auto dist = corner.getDistance(here);
            if (dist < closestDist) {
                closestDist = dist;
                closestCorner = corner;
            }
        }
        return closestCorner;
    }

    Position getOldestPosition(const Area * area)
    {
        auto oldest = DBL_MAX;
        auto oldestTile = TilePositions::Invalid;
        auto start = BWEB::Map::getMainArea()->TopLeft();
        auto end = BWEB::Map::getMainArea()->BottomRight();
        auto closestStation = Stations::getClosestStationGround(PlayerState::Self, Position(area->Top()));

        for (int x = start.x; x < end.x; x++) {
            for (int y = start.y; y < end.y; y++) {
                auto t = TilePosition(x, y);
                auto p = Position(t) + Position(16, 16);
                if (!t.isValid()
                    || mapBWEM.GetArea(t) != area
                    || Broodwar->isVisible(t)
                    || (Broodwar->getFrameCount() - Grids::lastVisibleFrame(t) < 480)
                    || !Broodwar->isBuildable(t + TilePosition(-1, 0))
                    || !Broodwar->isBuildable(t + TilePosition(1, 0))
                    || !Broodwar->isBuildable(t + TilePosition(0, -1))
                    || !Broodwar->isBuildable(t + TilePosition(0, 1)))
                    continue;

                auto visible = closestStation ? Grids::lastVisibleFrame(t) / p.getDistance(closestStation->getBase()->Center()) : Grids::lastVisibleFrame(t);
                if (visible < oldest) {
                    oldest = visible;
                    oldestTile = t;
                }
            }
        }
        return Position(oldestTile) + Position(16, 16);
    }

    Position getSafeSpot(BWEB::Station * station) {
        if (station && safeSpots.find(station) != safeSpots.end())
            return safeSpots[station];
        return Positions::Invalid;
    }

    bool isInAllyTerritory(TilePosition here)
    {
        if (here.isValid() && mapBWEM.GetArea(here))
            return isInAllyTerritory(mapBWEM.GetArea(here));
        return false;
    }

    bool isInAllyTerritory(const Area * area)
    {
        if (allyTerritory.find(area) != allyTerritory.end())
            return true;
        return false;
    }

    bool isInEnemyTerritory(TilePosition here)
    {
        if (here.isValid() && mapBWEM.GetArea(here) && enemyTerritory.find(mapBWEM.GetArea(here)) != enemyTerritory.end())
            return true;
        return false;
    }

    bool isInEnemyTerritory(const Area * area)
    {
        if (enemyTerritory.find(area) != enemyTerritory.end())
            return true;
        return false;
    }

    bool isStartingBase(TilePosition here)
    {
        for (auto tile : Broodwar->getStartLocations()) {
            if (here.getDistance(tile) < 4)
                return true;
        }
        return false;
    }

    void onStart()
    {
        // Initialize BWEM and BWEB
        mapBWEM.Initialize();
        mapBWEM.EnableAutomaticPathAnalysis();
        mapBWEM.FindBasesForStartingLocations();
        BWEB::Map::onStart();
        BWEB::Stations::findStations();

        // Check if the map is an island map
        for (auto &start : mapBWEM.StartingLocations()) {
            if (!mapBWEM.GetArea(start)->AccessibleFrom(mapBWEM.GetArea(BWEB::Map::getMainTile())))
                islandMap = true;
        }

        // HACK: Play Plasma as an island map
        if (Broodwar->mapFileName().find("Plasma") != string::npos)
            islandMap = true;

        // HACK: Alchemist is a shit map (no seriously, if you're reading this you have no idea)
        if (Broodwar->mapFileName().find("Alchemist") != string::npos)
            shitMap = true;

        // Store non island bases
        for (auto &area : mapBWEM.Areas()) {
            if (!islandMap && area.AccessibleNeighbours().size() == 0)
                continue;
            for (auto &base : area.Bases())
                allBases.insert(&base);
        }

        // Add "empty" areas (ie. Andromeda areas around main)
        for (auto &area : BWEB::Map::getNaturalArea()->AccessibleNeighbours()) {

            if (area->ChokePoints().size() > 2 || shitMap)
                continue;

            for (auto &choke : area->ChokePoints()) {
                if (choke->Center() == BWEB::Map::getMainChoke()->Center()) {
                    allyTerritory.insert(area);
                }
            }
        }

        findSafeSpots();
        reverseRamp = Broodwar->getGroundHeight(BWEB::Map::getMainTile()) < Broodwar->getGroundHeight(BWEB::Map::getNaturalTile());
        flatRamp = Broodwar->getGroundHeight(BWEB::Map::getMainTile()) == Broodwar->getGroundHeight(BWEB::Map::getNaturalTile());
        myMain = BWEB::Stations::getClosestMainStation(BWEB::Map::getMainTile());
        myNatural = BWEB::Stations::getClosestNaturalStation(BWEB::Map::getNaturalTile());
    }

    void onFrame()
    {
        findEnemy();

        findEnemyNextExpand();
        findAttackPosition();
        findDefendPosition();
        findHarassPosition();

        updateAreas();

        findSafeSpots();
    }

    set<const Area*>& getAllyTerritory() { return allyTerritory; }
    set<const Area*>& getEnemyTerritory() { return enemyTerritory; }
    set<const Base*>& getAllBases() { return allBases; }
    Position getAttackPosition() { return attackPosition; }
    Position getDefendPosition() { return defendPosition; }
    Position getHarassPosition() { return harassPosition; }
    Position getEnemyStartingPosition() { return enemyStartingPosition; }
    BWEB::Station * getEnemyMain() { return enemyMain; }
    BWEB::Station * getEnemyNatural() { return enemyNatural; }
    BWEB::Station * getMyMain() { return myMain; }
    BWEB::Station * getMyNatural() { return myNatural; }
    TilePosition getEnemyExpand() { return enemyExpand; }
    TilePosition getEnemyStartingTilePosition() { return enemyStartingTilePosition; }
    const ChokePoint * getDefendChoke() { return defendChoke; }
    const Area * getDefendArea() { return defendArea; }
    bool isShitMap() { return shitMap; }
    bool isIslandMap() { return islandMap; }
    bool isReverseRamp() { return reverseRamp; }
    bool isFlatRamp() { return flatRamp; }
    bool isNarrowNatural() { return narrowNatural; }
    bool isDefendNatural() { return defendNatural; }
    bool foundEnemy() { return enemyStartingPosition.isValid() && Broodwar->isExplored(TilePosition(enemyStartingPosition)); }
}