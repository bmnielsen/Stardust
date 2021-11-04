#include "BWEB.h"

using namespace std;
using namespace BWAPI;

namespace BWEB::Map
{
    namespace {

        Position mainPosition = Positions::Invalid;
        Position naturalPosition = Positions::Invalid;
        TilePosition mainTile = TilePositions::Invalid;
        TilePosition naturalTile = TilePositions::Invalid;
        const BWEM::Area * naturalArea = nullptr;
        const BWEM::Area * mainArea = nullptr;
        const BWEM::ChokePoint * naturalChoke = nullptr;
        const BWEM::ChokePoint * mainChoke = nullptr;

        bool drawReserveOverlap, drawUsed, drawWalk, drawArea;

        map<BWAPI::Key, bool> lastKeyState;
        map<const BWEM::ChokePoint *, set<TilePosition>> chokeTiles;
        map<const BWEM::ChokePoint *, pair<Position, Position>> chokeLines;

        int reserveGrid[256][256] ={};
        UnitType usedGrid[256][256] ={};
        bool walkGridLarge[256][256] ={};
        bool walkGridMedium[256][256] ={};
        bool walkGridSmall[256][256] ={};
        bool walkGridFull[256][256] ={};
        bool logInfo = true;

        void findMain()
        {
            mainTile = Broodwar->self()->getStartLocation();
            mainPosition = Position(mainTile) + Position(64, 48);
            mainArea = mapBWEM.GetArea(mainTile);
        }

        void findNatural()
        {
            auto distBest = DBL_MAX;
            for (auto &area : mapBWEM.Areas()) {
                for (auto &base : area.Bases()) {

                    // Must have gas, be accesible and at least 5 mineral patches
                    if (base.Starting()
                        || base.Geysers().empty()
                        || area.AccessibleNeighbours().empty()
                        || base.Minerals().size() < 5)
                        continue;

                    const auto dist = getGroundDistance(base.Center(), mainPosition);
                    if (dist < distBest) {
                        distBest = dist;
                        naturalArea = base.GetArea();
                        naturalTile = base.Location();
                        naturalPosition = Position(naturalTile) + Position(64, 48);
                    }
                }
            }

            if (!naturalArea)
                naturalArea = mainArea;
        }

        void findMainChoke()
        {
            // Add all main chokes to a set
            set<BWEM::ChokePoint const *> mainChokes;
            for (auto &choke : mainArea->ChokePoints())
                mainChokes.insert(choke);
            if (mainChokes.size() == 1 && mapBWEM.GetPath(mainPosition, naturalPosition).size() == 1) {
                mainChoke = *mainChokes.begin();
                return;
            }

            // Add all natural chokes to a set
            set<BWEM::ChokePoint const *> naturalChokes;
            for (auto &choke : naturalArea->ChokePoints())
                naturalChokes.insert(choke);

            // If the natural area has only one chokepoint, then our main choke leads out of our base, find a choke that doesn't belong to the natural as well
            if (naturalArea && naturalArea->ChokePoints().size() == 1) {
                auto distBest = DBL_MAX;
                for (auto &choke : mainArea->ChokePoints()) {
                    const auto dist = getGroundDistance(Position(choke->Center()), mainPosition);
                    if (dist < distBest && naturalChokes.find(choke) == naturalChokes.end()) {
                        mainChoke = choke;
                        distBest = dist;
                    }
                }
            }

            // Find a chokepoint that belongs to main and natural areas            
            else if (naturalArea) {
                auto distBest = DBL_MAX;
                for (auto &choke : naturalArea->ChokePoints()) {
                    const auto dist = getGroundDistance(Position(choke->Center()), mainPosition);
                    if (dist < distBest && mainChokes.find(choke) != mainChokes.end()) {
                        mainChoke = choke;
                        distBest = dist;
                    }
                }
            }

            // If we didn't find a main choke that belongs to main and natural, check if a path exists between both positions
            if (!mainChoke && mainPosition.isValid() && naturalPosition.isValid()) {
                auto distBest = DBL_MAX;
                for (auto &choke : mapBWEM.GetPath(mainPosition, naturalPosition)) {
                    const auto width = choke->Pos(choke->end1).getDistance(choke->Pos(choke->end2));
                    if (width < distBest) {
                        mainChoke = choke;
                        distBest = width;
                    }
                }
            }

            // If we still don't have a main choke, grab the closest chokepoint to our start
            if (!mainChoke) {
                auto distBest = DBL_MAX;
                for (auto &choke : mainArea->ChokePoints()) {
                    const auto dist = Position(choke->Center()).getDistance(mainPosition);
                    if (dist < distBest) {
                        mainChoke = choke;
                        distBest = dist;
                    }
                }
            }
        }

        void findNaturalChoke()
        {
            if (!naturalPosition.isValid()) {
                naturalChoke = mainChoke;
                return;
            }

            set<BWEM::ChokePoint const *> nonChokes;
            for (auto &choke : mapBWEM.GetPath(mainPosition, naturalPosition))
                nonChokes.insert(choke);

            // If the natural area has only one chokepoint, then choose as that
            if (naturalArea && naturalArea->ChokePoints().size() == 1)
                naturalChoke = naturalArea->ChokePoints().front();

            // Find area that shares the choke we need to defend
            else {
                auto distBest = DBL_MAX;
                const BWEM::Area* second = nullptr;
                if (naturalArea) {
                    for (auto &area : naturalArea->AccessibleNeighbours()) {
                        auto center = area->Top();
                        const auto dist = Position(center).getDistance(mapBWEM.Center());

                        bool wrongArea = false;
                        for (auto &choke : area->ChokePoints()) {
                            if ((!choke->Blocked() && choke->Pos(choke->end1).getDistance(choke->Pos(choke->end2)) <= 2) || nonChokes.find(choke) != nonChokes.end()) {
                                wrongArea = true;
                            }
                        }
                        if (wrongArea)
                            continue;

                        if (center.isValid() && dist < distBest)
                            second = area, distBest = dist;
                    }

                    // Find second choke based on the connected area
                    distBest = DBL_MAX;
                    for (auto &choke : naturalArea->ChokePoints()) {
                        if (choke->Center() == mainChoke->Center()
                            || choke->Blocked()
                            || choke->Geometry().size() <= 3
                            || (choke->GetAreas().first != second && choke->GetAreas().second != second))
                            continue;

                        const auto dist = Position(choke->Center()).getDistance(Position(Broodwar->self()->getStartLocation()));
                        if (dist < distBest)
                            naturalChoke = choke, distBest = dist;
                    }
                }
            }
        }

        void findNeutrals()
        {
            // Add overlap for neutrals
            for (auto unit : Broodwar->getNeutralUnits()) {
                if (unit && unit->exists() && unit->getType().topSpeed() == 0.0)
                    addReserve(unit->getTilePosition(), unit->getType().tileWidth(), unit->getType().tileHeight());
                if (unit->getType().isBuilding())
                    addUsed(unit->getTilePosition(), unit->getType());
            }
        }
    }

    void draw()
    {
        WalkPosition mouse(Broodwar->getMousePosition() + Broodwar->getScreenPosition());
        auto mouseArea = mouse.isValid() ? mapBWEM.GetArea(mouse) : nullptr;
        auto k1 = Broodwar->getKeyState(BWAPI::Key::K_1);
        auto k2 = Broodwar->getKeyState(BWAPI::Key::K_2);
        auto k3 = Broodwar->getKeyState(BWAPI::Key::K_3);
        auto k4 = Broodwar->getKeyState(BWAPI::Key::K_4);

        drawReserveOverlap = k1 && k1 != lastKeyState[BWAPI::Key::K_1] ? !drawReserveOverlap : drawReserveOverlap;
        drawUsed = k2 && k2 != lastKeyState[BWAPI::Key::K_2] ? !drawUsed : drawUsed;
        drawWalk = k3 && k3 != lastKeyState[BWAPI::Key::K_3] ? !drawWalk : drawWalk;
        drawArea = k4 && k4 != lastKeyState[BWAPI::Key::K_4] ? !drawArea : drawArea;

        lastKeyState[BWAPI::Key::K_1] = k1;
        lastKeyState[BWAPI::Key::K_2] = k2;
        lastKeyState[BWAPI::Key::K_3] = k3;
        lastKeyState[BWAPI::Key::K_4] = k4;

        // Detect a keypress for drawing information
        if (drawReserveOverlap || drawUsed || drawWalk || drawArea) {

            for (auto x = 0; x < Broodwar->mapWidth(); x++) {
                for (auto y = 0; y < Broodwar->mapHeight(); y++) {
                    const TilePosition t(x, y);

                    // Draw boxes around TilePositions that are reserved or overlapping important map features
                    if (drawReserveOverlap) {
                        if (reserveGrid[x][y] >= 1)
                            Broodwar->drawBoxMap(Position(t) + Position(4, 4), Position(t) + Position(29, 29), Colors::Grey, false);
                    }

                    // Draw boxes around TilePositions that are used
                    if (drawUsed) {
                        const auto type = usedGrid[x][y];
                        if (type != UnitTypes::None)
                            Broodwar->drawBoxMap(Position(t) + Position(8, 8), Position(t) + Position(25, 25), Colors::Black, true);
                    }

                    // Draw boxes around fully walkable TilePositions
                    if (drawWalk) {
                        if (walkGridFull[x][y])
                            Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Black, false);
                        if (walkGridLarge[x][y])
                            Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Black, true);
                    }

                    // Draw boxes around any TilePosition that shares an Area with mouses current Area
                    if (drawArea) {
                        if (mapBWEM.GetArea(t) == mouseArea)
                            Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Green, false);
                    }
                }
            }
        }

        Walls::draw();
        Blocks::draw();
        Stations::draw();
    }

    void onStart()
    {
        // Initializes usedGrid and walkGridFull
        for (int x = 0; x < Broodwar->mapWidth(); x++) {
            for (int y = 0; y < Broodwar->mapHeight(); y++) {

                usedGrid[x][y] = UnitTypes::None;

                auto cnt = 0;
                for (int dx = x * 4; dx < (x * 4) + 4; dx++) {
                    for (int dy = y * 4; dy < (y * 4) + 4; dy++) {
                        const auto w = WalkPosition(dx, dy);
                        if (w.isValid() && Broodwar->isWalkable(w))
                            cnt++;
                    }
                }

                if (cnt >= 14)
                    walkGridFull[x][y] = true;
            }
        }

        // Set all tiles on geysers as fully unwalkable
        for (auto gas : Broodwar->getGeysers()) {
            for (int x = gas->getTilePosition().x; x < gas->getTilePosition().x + 4; x++) {
                for (int y = gas->getTilePosition().y; y < gas->getTilePosition().y + 2; y++) {
                    walkGridFull[x][y] = false;
                }
            }
        }

        // Check for at least 3 columns to left and right of tile for walkability
        const auto isLargeWalkable = [&](auto &t) {
            WalkPosition w(t);

            auto offset1 = Broodwar->isWalkable(w + WalkPosition(-2, 0)) && Broodwar->isWalkable(w + WalkPosition(-2, 1)) && Broodwar->isWalkable(w + WalkPosition(-2, 2)) && Broodwar->isWalkable(w + WalkPosition(-2, 3));
            auto offset2 = Broodwar->isWalkable(w + WalkPosition(-1, 0)) && Broodwar->isWalkable(w + WalkPosition(-1, 1)) && Broodwar->isWalkable(w + WalkPosition(-1, 2)) && Broodwar->isWalkable(w + WalkPosition(-1, 3));
            auto offset3 = Broodwar->isWalkable(w + WalkPosition(4, 0)) && Broodwar->isWalkable(w + WalkPosition(4, 1)) && Broodwar->isWalkable(w + WalkPosition(4, 2)) && Broodwar->isWalkable(w + WalkPosition(4, 3));
            auto offset4 = Broodwar->isWalkable(w + WalkPosition(5, 0)) && Broodwar->isWalkable(w + WalkPosition(5, 1)) && Broodwar->isWalkable(w + WalkPosition(5, 2)) && Broodwar->isWalkable(w + WalkPosition(5, 3));

            auto cornersWalkable = Broodwar->isWalkable(w) && Broodwar->isWalkable(w + WalkPosition(3, 0)) && Broodwar->isWalkable(w + WalkPosition(0, 3)) && Broodwar->isWalkable(w + WalkPosition(3, 3));

            return (offset1 && offset2 && cornersWalkable) ||
                (cornersWalkable && offset3 && offset4);
        };

        // Check for at least 2 columns to left and right of tile for walkability
        const auto isMediumWalkable = [&](auto &t) {
            WalkPosition w(t);

            auto offset1 = Broodwar->isWalkable(w + WalkPosition(-2, 0)) && Broodwar->isWalkable(w + WalkPosition(-2, 1)) && Broodwar->isWalkable(w + WalkPosition(-2, 2)) && Broodwar->isWalkable(w + WalkPosition(-2, 3));
            auto offset2 = Broodwar->isWalkable(w + WalkPosition(-1, 0)) && Broodwar->isWalkable(w + WalkPosition(-1, 1)) && Broodwar->isWalkable(w + WalkPosition(-1, 2)) && Broodwar->isWalkable(w + WalkPosition(-1, 3));
            auto offset3 = Broodwar->isWalkable(w + WalkPosition(4, 0)) && Broodwar->isWalkable(w + WalkPosition(4, 1)) && Broodwar->isWalkable(w + WalkPosition(4, 2)) && Broodwar->isWalkable(w + WalkPosition(4, 3));
            auto offset4 = Broodwar->isWalkable(w + WalkPosition(5, 0)) && Broodwar->isWalkable(w + WalkPosition(5, 1)) && Broodwar->isWalkable(w + WalkPosition(5, 2)) && Broodwar->isWalkable(w + WalkPosition(5, 3));

            return (offset1 && offset2) ||
                (offset2 && offset3) ||
                (offset3 && offset4);
        };

        // As long as Tile is walkable
        const auto isSmallWalkable = [&](auto &t) {
            return true;
        };


        // Initialize walk grids for each rough unit size
        for (int x = 0; x <= Broodwar->mapWidth(); x++) {
            for (int y = 0; y <= Broodwar->mapHeight(); y++) {
                TilePosition t(x, y);
                if (walkGridFull[x][y]) {
                    if (isLargeWalkable(t))
                        walkGridLarge[x][y] = true;
                    if (isMediumWalkable(t))
                        walkGridMedium[x][y] = true;
                    if (isSmallWalkable(t))
                        walkGridSmall[x][y] = true;
                }
            }
        }

        // Create chokepoint geometry cache in TilePositions
        for (auto &area : mapBWEM.Areas()) {
            for (auto &choke : area.ChokePoints()) {
                for (auto &geo : choke->Geometry())
                    chokeTiles[choke].insert(TilePosition(geo));
            }
        }

        findNeutrals();
        findMain();
        findNatural();
        findMainChoke();
        findNaturalChoke();
    }

    void onUnitDiscover(const Unit unit)
    {
        const auto tile = unit->getTilePosition();
        const auto type = unit->getType();

        const auto gameStart = Broodwar->getFrameCount() == 0;
        const auto okayToAdd = (unit->getType().isBuilding() && !unit->isFlying())
            || (gameStart && unit->getType().topSpeed() == 0.0 && unit->getPlayer()->isNeutral());

        // Add used tiles
        if (okayToAdd) {
            for (auto x = tile.x; x < tile.x + type.tileWidth(); x++) {
                for (auto y = tile.y; y < tile.y + type.tileHeight(); y++) {
                    TilePosition t(x, y);
                    if (!t.isValid())
                        continue;
                    usedGrid[x][y] = type;
                }
            }

            // Clear pathfinding cache
            Pathfinding::clearCacheFully();
        }
    }

    void onUnitDestroy(const Unit unit)
    {
        const auto tile = unit->getTilePosition();
        const auto type = unit->getType();

        const auto gameStart = Broodwar->getFrameCount() == 0;
        const auto okayToRemove = (unit->getType().isBuilding() && !unit->isFlying())
            || (!gameStart && unit->getType().topSpeed() == 0.0 && unit->getPlayer()->isNeutral());

        // Add used tiles
        if (okayToRemove) {
            for (auto x = tile.x; x < tile.x + type.tileWidth(); x++) {
                for (auto y = tile.y; y < tile.y + type.tileHeight(); y++) {
                    TilePosition t(x, y);
                    if (!t.isValid())
                        continue;
                    usedGrid[x][y] = UnitTypes::None;
                }
            }

            // Clear pathfinding cache
            Pathfinding::clearCacheFully();
        }
        
        // Update BWEM
        if (unit->getPlayer()->isNeutral() && find_if(Map::mapBWEM.StaticBuildings().begin(), Map::mapBWEM.StaticBuildings().end(), [&](auto &n) { return n.get()->Unit() == unit; }) != Map::mapBWEM.StaticBuildings().end())
            Map::mapBWEM.OnStaticBuildingDestroyed(unit);
    }

    void onUnitMorph(const Unit unit)
    {
        onUnitDiscover(unit);
    }

    void addReserve(const TilePosition t, const int w, const int h)
    {
        for (auto x = t.x; x < t.x + w; x++) {
            for (auto y = t.y; y < t.y + h; y++)
                if (TilePosition(x, y).isValid())
                    reserveGrid[x][y] = 1;
        }
    }

    void removeReserve(const TilePosition t, const int w, const int h)
    {
        for (auto x = t.x; x < t.x + w; x++) {
            for (auto y = t.y; y < t.y + h; y++)
                if (TilePosition(x, y).isValid())
                    reserveGrid[x][y] = 0;
        }
    }

    bool isReserved(const TilePosition here, const int width, const int height)
    {
        for (auto x = here.x; x < here.x + width; x++) {
            for (auto y = here.y; y < here.y + height; y++) {
                TilePosition t(x, y);
                if (!t.isValid())
                    continue;
                if (reserveGrid[x][y] > 0)
                    return true;
            }
        }
        return false;
    }

    void addUsed(const TilePosition t, UnitType type)
    {
        for (auto x = t.x; x < t.x + type.tileWidth(); x++) {
            for (auto y = t.y; y < t.y + type.tileHeight(); y++)
                if (TilePosition(x, y).isValid())
                    usedGrid[x][y] = type;
        }
    }

    void removeUsed(const TilePosition t, const int w, const int h)
    {
        for (auto x = t.x; x < t.x + w; x++) {
            for (auto y = t.y; y < t.y + h; y++)
                if (TilePosition(x, y).isValid())
                    usedGrid[x][y] = UnitTypes::None;
        }
    }

    UnitType isUsed(const TilePosition here, const int width, const int height)
    {
        for (auto x = here.x; x < here.x + width; x++) {
            for (auto y = here.y; y < here.y + height; y++) {
                TilePosition t(x, y);
                if (!t.isValid())
                    continue;
                if (Map::usedGrid[x][y] != UnitTypes::None)
                    return Map::usedGrid[x][y];
            }
        }
        return UnitTypes::None;
    }

    bool isWalkable(const TilePosition here, UnitType type)
    {
        if (type.width() >= 30 || type.height() >= 30)
            return walkGridLarge[here.x][here.y];
        if (type.width() >= 20 || type.height() >= 20)
            return walkGridMedium[here.x][here.y];
        if (type.width() >= 10 || type.height() >= 10)
            return walkGridSmall[here.x][here.y];
        return walkGridFull[here.x][here.y];
    }

    bool isPlaceable(UnitType type, const TilePosition location)
    {
        if (type.requiresCreep()) {
            for (auto x = location.x; x < location.x + type.tileWidth(); x++) {
                const TilePosition creepTile(x, location.y + type.tileHeight());
                if (!Broodwar->isBuildable(creepTile))
                    return false;
            }
        }

        if (type.isResourceDepot() && !Broodwar->canBuildHere(location, type))
            return false;

        for (auto x = location.x; x < location.x + type.tileWidth(); x++) {
            for (auto y = location.y; y < location.y + type.tileHeight(); y++) {

                const TilePosition tile(x, y);
                if (!tile.isValid()
                    || !Broodwar->isBuildable(tile)
                    || !Broodwar->isWalkable(WalkPosition(tile))
                    || Map::isUsed(tile) != UnitTypes::None)
                    return false;
            }
        }
        return true;
    }

    template <class T>
    double getGroundDistance(T s, T e)
    {
        Position start(s), end(e);
        auto dist = 0.0;
        auto last = start;

        const auto validatePoint = [&](WalkPosition w) {
            auto distBest = 0.0;
            auto posBest = Position(w);
            for (auto x = w.x - 1; x < w.x + 1; x++) {
                for (auto y = w.y - 1; y < w.y + 1; y++) {
                    WalkPosition w(x, y);
                    if (!w.isValid()
                        || !mapBWEM.GetArea(w))
                        continue;

                    auto p = Position(w);
                    auto dist = p.getDistance(mapBWEM.Center());
                    if (dist > distBest) {
                        distBest = dist;
                        posBest = p;
                    }
                }
            }
            return posBest;
        };

        // Return DBL_MAX if not valid path points or not walkable path points
        if (!start.isValid() || !end.isValid())
            return DBL_MAX;

        // Check if we're in a valid area, if not try to find a different nearby WalkPosition
        if (!mapBWEM.GetArea(WalkPosition(start)))
            start = validatePoint(WalkPosition(start));
        if (!mapBWEM.GetArea(WalkPosition(end)))
            end = validatePoint(WalkPosition(end));

        // If not valid still, return DBL_MAX
        if (!start.isValid()
            || !end.isValid()
            || !mapBWEM.GetArea(WalkPosition(start))
            || !mapBWEM.GetArea(WalkPosition(end))
            || !mapBWEM.GetArea(WalkPosition(start))->AccessibleFrom(mapBWEM.GetArea(WalkPosition(end))))
            return DBL_MAX;

        // Find the closest chokepoint node
        const auto accurateClosestNode = [&](const BWEM::ChokePoint * cp) {
            return getClosestChokeTile(cp, start);
        };

        const auto fastClosestNode = [&](const BWEM::ChokePoint * cp) {
            auto bestPosition = cp->Center();
            auto bestDist = DBL_MAX;

            const auto n1 = Position(cp->Pos(cp->end1));
            const auto n2 = Position(cp->Pos(cp->end2));
            const auto n3 = Position(cp->Center());

            const auto d1 = n1.getDistance(last);
            const auto d2 = n2.getDistance(last);
            const auto d3 = n3.getDistance(last);

            return d1 < d2 ? (d1 < d3 ? n1 : n3) : (d2 < d3 ? n2 : n3);
        };

        // For each chokepoint, add the distance to the closest chokepoint node
        auto first = true;
        for (auto &cpp : mapBWEM.GetPath(start, end)) {

            auto large = cpp->Pos(cpp->end1).getDistance(cpp->Pos(cpp->end2)) > 40;

            auto next = first && !large ? accurateClosestNode(cpp) : fastClosestNode(cpp);
            dist += next.getDistance(last);
            last = next;
            first = false;
        }

        return dist += last.getDistance(end) - start.getDistance(Position(s)) - end.getDistance(Position(e));
    }

    Position getClosestChokeTile(const BWEM::ChokePoint * choke, Position here)
    {
        auto best = DBL_MAX;
        auto posBest = Positions::Invalid;
        for (auto &tile : chokeTiles[choke]) {
            const auto p = Position(tile) + Position(16, 16);
            const auto dist = p.getDistance(here);
            if (dist < best) {
                posBest = p;
                best = dist;
            }
        }
        return posBest;
    }

    pair<Position, Position> perpendicularLine(pair<Position, Position> points, double length)
    {
        auto n1 = points.first;
        auto n2 = points.second;
        auto dist = n1.getDistance(n2);
        auto dx1 = int((n2.x - n1.x) * length / dist);
        auto dy1 = int((n2.y - n1.y) * length / dist);
        auto dx2 = int((n1.x - n2.x) * length / dist);
        auto dy2 = int((n1.y - n2.y) * length / dist);
        auto direction1 = Position(-dy1, dx1) + ((n1 + n2) / 2);
        auto direction2 = Position(-dy2, dx2) + ((n1 + n2) / 2);
        return make_pair(direction1, direction2);
    }

    const BWEM::Area * getNaturalArea() { return naturalArea; }
    const BWEM::Area * getMainArea() { return mainArea; }
    const BWEM::ChokePoint * getNaturalChoke() { return naturalChoke; }
    const BWEM::ChokePoint * getMainChoke() { return mainChoke; }
    TilePosition getNaturalTile() { return naturalTile; }
    Position getNaturalPosition() { return naturalPosition; }
    TilePosition getMainTile() { return mainTile; }
    Position getMainPosition() { return mainPosition; }
}