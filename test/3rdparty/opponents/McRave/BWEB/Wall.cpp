#include "BWEB.h"
#include <chrono>

using namespace std;
using namespace BWAPI;

namespace BWEB {

    namespace {
        map<const BWEM::ChokePoint *, Wall> walls;
        bool logInfo = true;

        int failedPlacement = 0;
        int failedAngle = 0;
        int failedPath = 0;
        int failedTight = 0;
        int failedSpawn = 0;
        int failedPower = 0;

        int resourceGrid[256][256];
        int testGrid[256][256];
        int existingDefenseGrid[256][256];

        int tilesWithinArea(BWEM::Area const * area, const TilePosition here, const int width, const int height)
        {
            auto cnt = 0;
            for (auto x = here.x; x < here.x + width; x++) {
                for (auto y = here.y; y < here.y + height; y++) {
                    TilePosition t(x, y);
                    if (!t.isValid())
                        return false;

                    if (Map::mapBWEM.GetArea(t) == area)
                        cnt++;
                }
            }
            return cnt;
        }
    }

    Position Wall::findCentroid()
    {
        if (rawBuildings.empty())
            return Position(choke->Center());

        // Create current centroid using all buildings except Pylons
        auto currentCentroid = Position(0, 0);
        auto sizeWall = int(rawBuildings.size());
        for (auto &[tile, type] : bestLayout) {
            if (type != UnitTypes::Protoss_Pylon)
                currentCentroid += Position(tile) + Position(type.tileSize()) / 2;
            else
                sizeWall--;
        }

        // Create a centroid if we only have a Pylon wall
        if (sizeWall == 0) {
            sizeWall = bestLayout.size();
            for (auto &[tile, type] : bestLayout)
                currentCentroid += Position(tile) + Position(type.tileSize()) / 2;
        }
        return (currentCentroid / sizeWall);
    }

    TilePosition Wall::findOpening()
    {
        if (!openWall)
            return TilePositions::Invalid;

        // Set any tiles on the path as reserved so we don't build on them
        auto currentPath = findPathOut();
        auto currentOpening = TilePositions::Invalid;

        // Check which tile is closest to each part on the path, set as opening
        auto distBest = DBL_MAX;
        for (auto &pathTile : currentPath.getTiles()) {
            const auto closestChokeGeo = Map::getClosestChokeTile(choke, Position(pathTile));
            const auto dist = closestChokeGeo.getDistance(Position(pathTile));
            const auto centerPath = Position(pathTile) + Position(16, 16);

            auto angleOkay = false;
            auto distOkay = false;

            // Check if the angle and distance is okay
            for (auto &[tileLayout, typeLayout] : currentLayout) {
                if (typeLayout == UnitTypes::Protoss_Pylon)
                    continue;

                const auto centerPiece = Position(tileLayout) + Position(typeLayout.tileWidth() * 16, typeLayout.tileHeight() * 16);
                const auto openingAngle = Map::getAngle(make_pair(centerPiece, centerPath));
                const auto openingDist = centerPiece.getDistance(centerPath);

                if (abs(chokeAngle - openingAngle) < 0.30)
                    angleOkay = true;
                if (openingDist < 320.0)
                    distOkay = true;
            }
            if (distOkay && angleOkay && dist < distBest) {
                distBest = dist;
                currentOpening = pathTile;
            }
        }

        // If we don't have an opening, assign closest path tile to wall centroid as opening
        if (!currentOpening.isValid()) {
            for (auto &pathTile : currentPath.getTiles()) {
                const auto p = Position(pathTile);
                const auto dist = centroid.getDistance(p);
                if (dist < distBest) {
                    distBest = dist;
                    currentOpening = pathTile;
                }
            }
        }

        return currentOpening;
    }

    Path Wall::findPathOut()
    {
        // Check that the path points are possible to reach
        checkPathPoints();
        const auto startCenter = Position(pathStart) + Position(16, 16);
        const auto endCenter = Position(pathEnd) + Position(16, 16);
        const auto biggestUnit = Broodwar->self()->getRace() == Races::Zerg ? UnitTypes::Zerg_Ultralisk : UnitTypes::Protoss_Dragoon;

        // Get a new path
        BWEB::Path newPath(endCenter, startCenter, biggestUnit, false, false);
        newPath.generateBFS([&](const auto &t) { return wallWalkable(t); });
        return newPath;
    }

    bool Wall::powerCheck(const UnitType type, const TilePosition here)
    {
        if (type != UnitTypes::Protoss_Pylon || pylonWall)
            return true;

        // TODO: Create a generic BWEB function that takes 2 tiles and tells you if the 1st tile will power the 2nd tile
        for (auto &[tileLayout, typeLayout] : currentLayout) {
            if (typeLayout == UnitTypes::Protoss_Pylon)
                continue;

            if (typeLayout.tileWidth() == 4) {
                auto powersThis = false;
                if (tileLayout.y - here.y == -5 || tileLayout.y - here.y == 4) {
                    if (tileLayout.x - here.x >= -4 && tileLayout.x - here.x <= 1)
                        powersThis = true;
                }
                if (tileLayout.y - here.y == -4 || tileLayout.y - here.y == 3) {
                    if (tileLayout.x - here.x >= -7 && tileLayout.x - here.x <= 4)
                        powersThis = true;
                }
                if (tileLayout.y - here.y == -3 || tileLayout.y - here.y == 2) {
                    if (tileLayout.x - here.x >= -8 && tileLayout.x - here.x <= 5)
                        powersThis = true;
                }
                if (tileLayout.y - here.y >= -2 && tileLayout.y - here.y <= 1) {
                    if (tileLayout.x - here.x >= -8 && tileLayout.x - here.x <= 6)
                        powersThis = true;
                }
                if (!powersThis)
                    return false;
            }
            else if (typeLayout.tileWidth() == 3) {
                auto powersThis = false;

                if (tileLayout.y - here.y == -4 || tileLayout.y - here.y == 3) {
                    if (tileLayout.x - here.x >= -6 && tileLayout.x - here.x <= 5)
                        powersThis = true;
                }
                if (tileLayout.y - here.y == -3 || tileLayout.y - here.y == 2) {
                    if (tileLayout.x - here.x >= -7 && tileLayout.x - here.x <= 6)
                        powersThis = true;
                }
                if (tileLayout.y - here.y >= -2 && tileLayout.y - here.y <= 1) {
                    if (tileLayout.x - here.x >= -8 && tileLayout.x - here.x <= 7)
                        powersThis = true;
                }
                if (tileLayout.y - here.y == 4) {
                    if (tileLayout.x - here.x >= -3 && tileLayout.x - here.x <= 2)
                        powersThis = true;
                }
                if (!powersThis)
                    return false;
            }
            else {
                auto powersThis = false;
                if (tileLayout.y - here.y == 4) {
                    if (tileLayout.x - here.x >= -3 && tileLayout.x - here.x <= 2)
                        powersThis = true;
                }
                if (tileLayout.y - here.y == -4 || tileLayout.y - here.y == 3) {
                    if (tileLayout.x - here.x >= -6 && tileLayout.x - here.x <= 5)
                        powersThis = true;
                }
                if (tileLayout.y - here.y == -3 || tileLayout.y - here.y == 2) {
                    if (tileLayout.x - here.x >= -7 && tileLayout.x - here.x <= 6)
                        powersThis = true;
                }
                if (tileLayout.y - here.y >= -2 && tileLayout.y - here.y <= 1) {
                    if (tileLayout.x - here.x >= -7 && tileLayout.x - here.x <= 7)
                        powersThis = true;
                }
                if (!powersThis)
                    return false;
            }
        }
        return true;
    }

    bool Wall::angleCheck(const UnitType type, const TilePosition here)
    {
        const auto centerHere = Position(here) + Position(type.tileWidth() * 16, type.tileHeight() * 16);

        // If we want a closed wall, we don't care the angle of the buildings
        if (!openWall || (type == UnitTypes::Protoss_Pylon && !pylonWall && !pylonWallPiece))
            return true;

        // Check if the angle is okay between all pieces in the current layout
        for (auto &[tileLayout, typeLayout] : currentLayout) {
            if (typeLayout == UnitTypes::Protoss_Pylon && !pylonWall && !pylonWallPiece)
                continue;

            const auto centerPiece = Position(tileLayout) + Position(typeLayout.tileWidth() * 16, typeLayout.tileHeight() * 16);
            const auto wallAngle = Map::getAngle(make_pair(centerPiece, centerHere));

            if (abs(chokeAngle - wallAngle) > 0.50)
                return false;
        }
        return true;
    }

    bool Wall::placeCheck(const UnitType type, const TilePosition here)
    {
        // Allow Pylon to overlap station defenses
        if (type == UnitTypes::Protoss_Pylon) {
            if (station && find(station->getDefenses().begin(), station->getDefenses().end(), here) != station->getDefenses().end())
                return true;
        }

        // Can't place below a Hatchery
        for (auto x = here.x; x < here.x + type.tileWidth(); x++) {
            const TilePosition t(x, here.y - 1);
            if (Map::isUsed(t) == UnitTypes::Zerg_Hatchery)
                return false;
        }

        // Can't place behind our base
        auto center = Position(here) + Position(type.tileWidth() * 16, type.tileHeight() * 16);
        if (center.getDistance(Position(choke->Center())) >= base->Center().getDistance(Position(choke->Center())))
            return false;

        // Check if placement is valid
        if (Map::isReserved(here, type.tileWidth(), type.tileHeight())
            || !Map::isPlaceable(type, here)
            || tilesWithinArea(area, here, type.tileWidth(), type.tileHeight()) == 0)
            return false;
        return true;
    }

    bool Wall::tightCheck(const UnitType type, const TilePosition here, bool visual)
    {
        // If this is a powering pylon and we are not making a pylon wall, we don't care if it's tight
        if (type == UnitTypes::Protoss_Pylon && !pylonWall && !pylonWallPiece)
            return true;

        // Dimensions of current buildings UnitType
        const auto dimL = (type.tileWidth() * 16) - type.dimensionLeft();
        const auto dimR = (type.tileWidth() * 16) - type.dimensionRight() - 1;
        const auto dimU = (type.tileHeight() * 16) - type.dimensionUp();
        const auto dimD = (type.tileHeight() * 16) - type.dimensionDown() - 1;
        const auto walkHeight = type.tileHeight() * 4;
        const auto walkWidth = type.tileWidth() * 4;

        // Dimension of UnitType to check tightness for
        const auto vertTight = (tightType == UnitTypes::None) ? 32 : tightType.height();
        const auto horizTight = (tightType == UnitTypes::None) ? 32 : tightType.width();

        // Checks each side of the building to see if it is valid for walling purposes
        const auto checkL = dimL < horizTight;
        const auto checkR = dimR < horizTight;
        const auto checkU = dimU < vertTight;
        const auto checkD = dimD < vertTight;

        // Figures out how many extra tiles we can check tightness for
        const auto extraL = pylonWall || !requireTight ? 0 : max(0, (horizTight - dimL) / 8);
        const auto extraR = pylonWall || !requireTight ? 0 : max(0, (horizTight - dimR) / 8);
        const auto extraU = pylonWall || !requireTight ? 0 : max(0, (vertTight - dimU) / 8);
        const auto extraD = pylonWall || !requireTight ? 0 : max(0, (vertTight - dimD) / 8);

        // Setup boundary WalkPositions to check for tightness
        const auto left =  WalkPosition(here) - WalkPosition(1 + extraL, 0);
        const auto right = WalkPosition(here) + WalkPosition(walkWidth + extraR, 0);
        const auto up =  WalkPosition(here) - WalkPosition(0, 1 + extraU);
        const auto down =  WalkPosition(here) + WalkPosition(0, walkHeight + extraD);

        // Used for determining if the tightness we found is suitable
        const auto firstBuilding = currentLayout.size() == 0;
        const auto lastBuilding = currentLayout.size() == (rawBuildings.size() - 1);
        auto terrainTight = false;
        auto parentTight = false;
        auto p1Tight = 0;
        auto p2Tight = 0;

        // Functions for each dimension check
        const auto gapRight = [&](UnitType parent) {
            return (parent.tileWidth() * 16) - parent.dimensionLeft() + dimR;
        };
        const auto gapLeft = [&](UnitType parent) {
            return (parent.tileWidth() * 16) - parent.dimensionRight() - 1 + dimL;
        };
        const auto gapUp = [&](UnitType parent) {
            return (parent.tileHeight() * 16) - parent.dimensionDown() - 1 + dimU;
        };
        const auto gapDown = [&](UnitType parent) {
            return (parent.tileHeight() * 16) - parent.dimensionUp() + dimD;
        };

        // Check if the building is terrain tight when placed here
        const auto terrainTightCheck = [&](WalkPosition w, bool check) {
            const auto t = TilePosition(w);

            // If the walkposition is invalid or unwalkable
            if (tightType != UnitTypes::None && check && (!w.isValid() || !Broodwar->isWalkable(w)))
                return true;

            // If we don't care about walling tight and the tile isn't walkable
            if (!requireTight && !Map::isWalkable(t))
                return true;

            // If there's a mineral field or geyser here
            if (Map::isUsed(t).isResourceContainer())
                return true;
            return false;
        };

        // Iterate vertical tiles adjacent of this placement
        const auto checkVerticalSide = [&](WalkPosition start, bool check, const auto gap) {
            for (auto x = start.x - 1 - extraL; x < start.x + walkWidth + 1 + extraR; x++) {
                const WalkPosition w(x, start.y);
                const auto t = TilePosition(w);
                const auto parent = Map::isUsed(t);
                const auto leftCorner = x < start.x;
                const auto rightCorner = x >= start.x + walkWidth;

                if (visual) {
                    Broodwar->drawBoxMap(Position(w) + Position(2, 2), Position(w) + Position(6, 6), Colors::Red);
                }

                // If this is a corner
                if (leftCorner || rightCorner) {

                    // Check if it's tight with the terrain
                    if (!terrainTight && terrainTightCheck(w, check) && leftCorner ? terrainTightCheck(w, checkL) : terrainTightCheck(w, checkR))
                        terrainTight = true;

                    // Check if it's tight with a parent
                    if (!parentTight && find(rawBuildings.begin(), rawBuildings.end(), parent) != rawBuildings.end() && (!requireTight || (gap(parent) < vertTight && (leftCorner ? gapLeft(parent) < horizTight : gapRight(parent) < horizTight))))
                        parentTight = true;
                }
                else {

                    // Check if it's tight with the terrain
                    if (!terrainTight && terrainTightCheck(w, check))
                        terrainTight = true;

                    // Check if it's tight with a parent
                    if (!parentTight && find(rawBuildings.begin(), rawBuildings.end(), parent) != rawBuildings.end() && (!requireTight || gap(parent) < vertTight))
                        parentTight = true;
                }

                // Check to see which node it is closest to (0 is don't check, 1 is not tight, 2 is tight)
                if (!openWall && !Map::isWalkable(t) && w.getDistance(choke->Center()) < 4) {
                    if (w.getDistance(choke->Pos(choke->end1)) < w.getDistance(choke->Pos(choke->end2))) {
                        if (p1Tight == 0)
                            p1Tight = 1;
                        if (terrainTight)
                            p1Tight = 2;
                    }
                    else if (p2Tight == 0) {
                        if (p2Tight == 0)
                            p2Tight = 1;
                        if (terrainTight)
                            p2Tight = 2;
                    }
                }
            }
        };

        // Iterate horizontal tiles adjacent of this placement
        const auto checkHorizontalSide = [&](WalkPosition start, bool check, const auto gap) {
            for (auto y = start.y - 1 - extraU; y < start.y + walkHeight + 1 + extraD; y++) {
                const WalkPosition w(start.x, y);
                const auto t = TilePosition(w);
                const auto parent = Map::isUsed(t);
                const auto topCorner = y < start.y;
                const auto downCorner = y >= start.y + walkHeight;

                if (visual) {
                    Broodwar->drawBoxMap(Position(w), Position(w) + Position(9, 9), Colors::Blue);
                }

                // If this is a corner
                if (topCorner || downCorner) {

                    // Check if it's tight with the terrain
                    if (!terrainTight && terrainTightCheck(w, check) && topCorner ? terrainTightCheck(w, checkU) : terrainTightCheck(w, checkD))
                        terrainTight = true;

                    // Check if it's tight with a parent
                    if (!parentTight && find(rawBuildings.begin(), rawBuildings.end(), parent) != rawBuildings.end() && (!requireTight || (gap(parent) < horizTight && (topCorner ? gapUp(parent) < vertTight : gapDown(parent) < vertTight))))
                        parentTight = true;
                }
                else {

                    // Check if it's tight with the terrain
                    if (!terrainTight && terrainTightCheck(w, check))
                        terrainTight = true;

                    // Check if it's tight with a parent
                    if (!parentTight && find(rawBuildings.begin(), rawBuildings.end(), parent) != rawBuildings.end() && (!requireTight || gap(parent) < horizTight))
                        parentTight = true;
                }

                // Check to see which node it is closest to (0 is don't check, 1 is not tight, 2 is tight)
                if (!openWall && !Map::isWalkable(t) && w.getDistance(choke->Center()) < 4) {
                    if (w.getDistance(choke->Pos(choke->end1)) < w.getDistance(choke->Pos(choke->end2))) {
                        if (p1Tight == 0)
                            p1Tight = 1;
                        if (terrainTight)
                            p1Tight = 2;
                    }
                    else if (p2Tight == 0) {
                        if (p2Tight == 0)
                            p2Tight = 1;
                        if (terrainTight)
                            p2Tight = 2;
                    }
                }
            }
        };

        // For each side, check if it's terrain tight or tight with any adjacent buildings
        checkVerticalSide(up, checkU, gapUp);
        checkVerticalSide(down, checkD, gapDown);
        checkHorizontalSide(left, checkL, gapLeft);
        checkHorizontalSide(right, checkR, gapRight);

        // If we want a closed wall, we need all buildings to be tight at the tightness resolution...
        if (!openWall) {
            if (!lastBuilding && !firstBuilding)      // ...to the parent if not first building
                return parentTight;
            if (firstBuilding)                        // ...to the terrain if first building
                return terrainTight && p1Tight != 1 && p2Tight != 1;
            if (lastBuilding)                         // ...to the parent and terrain if last building                
                return terrainTight && parentTight && p1Tight != 1 && p2Tight != 1;
        }

        // If we want an open wall, we need this building to be tight at tile resolution to a parent or terrain
        else if (openWall)
            return (terrainTight || parentTight);
        return false;
    }

    bool Wall::spawnCheck(const UnitType type, const TilePosition here)
    {
        // TODO: Check if units spawn in bad spots, just returns true for now
        checkPathPoints();
        const auto startCenter = Position(pathStart) + Position(16, 16);
        const auto endCenter = Position(pathEnd) + Position(16, 16);
        Path pathOut;
        return true;
    }

    bool Wall::wallWalkable(const TilePosition& tile)
    {
        // Checks for any collision and inverts the return value
        if (!tile.isValid()
            || (Map::mapBWEM.GetArea(tile) && Map::mapBWEM.GetArea(tile) != area && find(accessibleNeighbors.begin(), accessibleNeighbors.end(), Map::mapBWEM.GetArea(tile)) == accessibleNeighbors.end())
            || !Map::isWalkable(tile)
            || (allowLifted && Map::isUsed(tile) != UnitTypes::Terran_Barracks && Map::isUsed(tile) != UnitTypes::None)
            || (!allowLifted && Map::isUsed(tile) != UnitTypes::None && Map::isUsed(tile) != UnitTypes::Zerg_Larva)
            || (Map::isReserved(tile) && existingDefenseGrid[tile.x][tile.y] == 0)
            || resourceGrid[tile.x][tile.y] > 0)
            return false;
        return true;
    }

    void Wall::initialize()
    {
        // Clear failed counters
        failedPlacement         = 0;
        failedAngle             = 0;
        failedPath              = 0;
        failedTight             = 0;
        failedSpawn             = 0;
        failedPower             = 0;

        // Set BWAPI::Points to invalid (default constructor is None)
        centroid                = BWAPI::Positions::Invalid;
        opening                 = BWAPI::TilePositions::Invalid;
        pathStart               = BWAPI::TilePositions::Invalid;
        pathEnd                 = BWAPI::TilePositions::Invalid;
        initialPathStart        = BWAPI::TilePositions::Invalid;
        initialPathEnd          = BWAPI::TilePositions::Invalid;

        // Set important terrain features
        bestWallScore           = 0;
        accessibleNeighbors     = area->AccessibleNeighbours();
        pylonWall               = count(rawBuildings.begin(), rawBuildings.end(), BWAPI::UnitTypes::Protoss_Pylon) > 1;
        creationStart           = TilePosition(choke->Center());
        base                    = !area->Bases().empty() ? &area->Bases().front() : nullptr;
        flatRamp                = Broodwar->isBuildable(TilePosition(choke->Center()));
        station                 = Stations::getClosestStation(TilePosition(area->Top()));
        chokeAngle              = station->getDefenseAngle();

        // Initialize a grid of existing defense locations
        if (station) {
            for (auto &def : station->getDefenses()) {
                existingDefenseGrid[def.x][def.y] = 1;
                existingDefenseGrid[def.x + 1][def.y] = 1;
                existingDefenseGrid[def.x][def.y + 1] = 1;
                existingDefenseGrid[def.x + 1][def.y + 1] = 1;
            }
        }

        // Setup a resource grid that prevents pathing through mineral lines
        if (base) {
            for (auto &mineral : base->Minerals()) {
                for (int x = -1; x < 3; x++) {
                    for (int y = -1; y < 3; y++) {
                        resourceGrid[mineral->TopLeft().x + x][mineral->TopLeft().y + y] = 1;
                    }
                }
            }
        }

        // Check if a Pylon should be put in the wall to help the size of the Wall or away from the wall for protection
        auto p1 = choke->Pos(choke->end1);
        auto p2 = choke->Pos(choke->end2);
        pylonWallPiece = abs(p1.x - p2.x) * 8 >= 320 || abs(p1.y - p2.y) * 8 >= 256 || p1.getDistance(p2) * 8 >= 288;

        // Initiliaze path points and check them
        initializePathPoints();
        checkPathPoints();

        // Create a jps path for limiting BFS exploration using the distance of the jps path
        Path jpsPath(Position(pathStart), Position(pathEnd), UnitTypes::Zerg_Zergling, true, false);
        jpsPath.generateJPS([&](const TilePosition &t) { return jpsPath.unitWalkable(t); });
        jpsDist = jpsPath.getDistance();

        // Create notable locations to keep Wall pieces within proxmity of
        if (base)
            notableLocations ={ base->Center(), Position(initialPathStart) + Position(16,16), (base->Center() + Position(initialPathStart)) / 2 };
        else
            notableLocations ={ Position(initialPathStart) + Position(16,16), Position(initialPathEnd) + Position(16,16) };

        // Sort all the pieces and iterate over them to find the best wall - by Hannes
        if (find(rawBuildings.begin(), rawBuildings.end(), UnitTypes::Protoss_Pylon) != rawBuildings.end()) {
            sort(rawBuildings.begin(), rawBuildings.end(), [](UnitType l, UnitType r) { return (l == UnitTypes::Protoss_Pylon) < (r == UnitTypes::Protoss_Pylon); }); // Moves pylons to end
            sort(rawBuildings.begin(), find(rawBuildings.begin(), rawBuildings.end(), UnitTypes::Protoss_Pylon)); // Sorts everything before pylons
        }
        else if (find(rawBuildings.begin(), rawBuildings.end(), UnitTypes::Zerg_Hatchery) != rawBuildings.end()) {
            sort(rawBuildings.begin(), rawBuildings.end(), [](UnitType l, UnitType r) { return (l == UnitTypes::Zerg_Hatchery) > (r == UnitTypes::Zerg_Hatchery); }); // Moves hatchery to start
            sort(find(rawBuildings.begin(), rawBuildings.end(), UnitTypes::Zerg_Hatchery), rawBuildings.begin()); // Sorts everything after hatchery
        }
        else
            sort(rawBuildings.begin(), rawBuildings.end());

        // If there is a base in this area and we're creating an open wall, move creation start within 10 tiles of it            
        if (openWall && base) {
            auto startCenter = Position(creationStart) + Position(16, 16);
            auto distBest = DBL_MAX;
            auto moveTowards = (Position(initialPathStart) + base->Center()) / 2;

            // Iterate 3x3 around the current TilePosition and try to get within 5 tiles
            auto triedCount = 0;
            while (startCenter.getDistance(moveTowards) > 320.0 && triedCount < 50) {
                triedCount++;
                const auto initialStart = creationStart;
                for (int x = initialStart.x - 1; x <= initialStart.x + 1; x++) {
                    for (int y = initialStart.y - 1; y <= initialStart.y + 1; y++) {
                        const TilePosition t(x, y);
                        if (!t.isValid())
                            continue;

                        const auto p = Position(t) + Position(16, 16);
                        const auto dist = p.getDistance(moveTowards);

                        if (dist < distBest) {
                            distBest = dist;
                            creationStart = t;
                            startCenter = p;
                            movedStart = true;
                        }
                    }
                }
            }
        }

        // If the creation start position isn't buildable, move towards the top of this area to find a buildable location
        if (openWall) {
            auto triedCount = 0;
            while (!Broodwar->isBuildable(creationStart) && triedCount < 50) {
                triedCount++;
                auto distBest = DBL_MAX;
                const auto initialStart = creationStart;
                for (int x = initialStart.x - 1; x <= initialStart.x + 1; x++) {
                    for (int y = initialStart.y - 1; y <= initialStart.y + 1; y++) {
                        const TilePosition t(x, y);
                        if (!t.isValid())
                            continue;

                        const auto p = Position(t);
                        const auto dist = p.getDistance(Position(area->Top()));

                        if (dist < distBest) {
                            distBest = dist;
                            creationStart = t;
                            movedStart = true;
                        }
                    }
                }
            }
        }
    }

    void Wall::initializePathPoints()
    {
        auto line = make_pair(Position(choke->Pos(choke->end1)) + Position(4, 4), Position(choke->Pos(choke->end2)) + Position(4, 4));
        auto perpLine = openWall ? Map::perpendicularLine(line, 160.0) : Map::perpendicularLine(line, 96.0);
        auto lineStart = perpLine.first.getDistance(Position(area->Top())) > perpLine.second.getDistance(Position(area->Top())) ? perpLine.second : perpLine.first;
        auto lineEnd = perpLine.first.getDistance(Position(area->Top())) > perpLine.second.getDistance(Position(area->Top())) ? perpLine.first : perpLine.second;
        auto isMain = station && station->isMain();
        auto isNatural = station && station->isNatural();

        // If it's a natural wall, path between the closest main and end of the perpendicular line
        if (isNatural) {
            Station * closestMain = Stations::getClosestMainStation(TilePosition(choke->Center()));
            initialPathStart = closestMain ? TilePosition(Map::mapBWEM.GetPath(station->getBase()->Center(), closestMain->getBase()->Center()).front()->Center()) : TilePosition(lineStart);
            initialPathEnd = TilePosition(lineEnd);
        }

        // If it's a main wall, path between a point between the roughly the choke and the area top
        else if (isMain) {
            initialPathEnd = (TilePosition(choke->Center()) + TilePosition(lineEnd)) / 2;
            initialPathStart = (TilePosition(area->Top()) + TilePosition(lineStart)) / 2;
        }

        // Other walls
        else {
            initialPathStart = TilePosition(lineStart);
            initialPathEnd = TilePosition(lineEnd);
        }

        pathStart = initialPathStart;
        pathEnd = initialPathEnd;
    }

    void Wall::checkPathPoints()
    {
        // Push the path start as far from the path end if it's not in a valid location
        auto distBest = 0.0;
        if (!wallWalkable(pathStart)) {
            for (auto x = initialPathStart.x - 4; x < initialPathStart.x + 4; x++) {
                for (auto y = initialPathStart.y - 4; y < initialPathStart.y + 4; y++) {
                    const TilePosition t(x, y);
                    const auto dist = t.getDistance(initialPathEnd);
                    if (!wallWalkable(t))
                        continue;

                    if (dist > distBest) {
                        pathStart = t;
                        distBest = dist;
                    }
                }
            }
        }

        // Push the path end as far from the path start if it's not in a valid location
        distBest = 0.0;
        if (!wallWalkable(pathEnd)) {
            for (auto x = initialPathEnd.x - 4; x < initialPathEnd.x + 4; x++) {
                for (auto y = initialPathEnd.y - 4; y < initialPathEnd.y + 4; y++) {
                    const TilePosition t(x, y);
                    const auto dist = t.getDistance(initialPathStart);
                    if (!wallWalkable(t))
                        continue;

                    if (dist > distBest) {
                        pathEnd = t;
                        distBest = dist;
                    }
                }
            }
        }
    }

    void Wall::addPieces()
    {
        // If not adding any buildings to the wall
        if (rawBuildings.empty())
            return;

        // For each permutation, try to make a wall combination that is better than the current best
        do {
            currentLayout.clear();
            typeIterator = rawBuildings.begin();
            addNextPiece(creationStart);
        } while (Broodwar->self()->getRace() == Races::Zerg ? next_permutation(find(rawBuildings.begin(), rawBuildings.end(), UnitTypes::Zerg_Hatchery), rawBuildings.end())
            : next_permutation(rawBuildings.begin(), find(rawBuildings.begin(), rawBuildings.end(), UnitTypes::Protoss_Pylon)));

        for (auto &[tile, type] : bestLayout) {
            addToWallPieces(tile, type);
            Map::addReserve(tile, type.tileWidth(), type.tileHeight());
            Map::addUsed(tile, type);
        }
    }

    void Wall::addNextPiece(TilePosition start)
    {
        const auto type = *typeIterator;
        const auto radius = (typeIterator == rawBuildings.begin()) ? 8 : 5;

        for (auto x = start.x - radius; x < start.x + radius; x++) {
            for (auto y = start.y - radius; y < start.y + radius; y++) {
                const TilePosition tile(x, y);

                if (!tile.isValid())
                    continue;

                const auto center = Position(tile) + Position(type.tileWidth() * 16, type.tileHeight() * 16);
                const auto closestGeo = Map::getClosestChokeTile(choke, center);

                // Open walls need to be placed within proximity of notable features
                if (openWall) {
                    auto closestNotable = Positions::Invalid;
                    auto closestNotableDist = DBL_MAX;
                    for (auto & pos : notableLocations) {
                        auto dist = pos.getDistance(center);
                        if (dist < closestNotableDist) {
                            closestNotable = pos;
                            closestNotableDist = dist;
                        }
                    }
                    if (center.getDistance(closestNotable) >= 350.0 || center.getDistance(closestNotable) >= closestGeo.getDistance(closestNotable) + 96.0)
                        continue;
                }

                // Try not to seal the wall poorly
                if (!openWall && flatRamp) {
                    auto dist = min({ Position(tile).getDistance(Position(choke->Center())),
                                      Position(tile + TilePosition(type.tileWidth(), 0)).getDistance(Position(choke->Center())),
                                      Position(tile + TilePosition(type.tileWidth(), type.tileHeight())).getDistance(Position(choke->Center())),
                                      Position(tile + TilePosition(0, type.tileHeight())).getDistance(Position(choke->Center())) });
                    if (dist < 16.0)
                        continue;
                }

                // Required checks for this wall to be valid
                if (!powerCheck(type, tile)) {
                    failedPower++;
                    continue;
                }
                if (!angleCheck(type, tile)) {
                    failedAngle++;
                    continue;
                }
                if (!placeCheck(type, tile)) {
                    failedPlacement++;
                    continue;
                }
                if (!tightCheck(type, tile)) {
                    failedTight++;
                    continue;
                }
                if (!spawnCheck(type, tile)) {
                    failedSpawn++;
                    continue;
                }

                // 1) Store the current type, increase the iterator
                currentLayout[tile] = type;
                Map::addUsed(tile, type);
                typeIterator++;

                // 2) If at the end, score wall
                if (typeIterator == rawBuildings.end())
                    scoreWall();
                else
                    addNextPiece(tile);

                // 3) Erase this current placement and repeat
                if (typeIterator != rawBuildings.begin())
                    typeIterator--;

                currentLayout.erase(tile);
                Map::removeUsed(tile, type.tileWidth(), type.tileHeight());
            }
        }
    }

    void Wall::addDefenses()
    {
        if (rawDefenses.empty() || !valid)
            return;

        // Find a final path
        if (openWall)
            finalPath = findPathOut();

        auto left = Position(choke->Center()).x < base->Center().x;
        auto up = Position(choke->Center()).y < base->Center().y;
        auto type = (rawDefenses.front());
        auto baseDist = base->Center().getDistance(Position(choke->Center()));

        const auto validDefense = [&](const TilePosition tile, UnitType type) {
            if (!tile.isValid()
                || Map::isReserved(tile, 2, 2)
                || tilesWithinArea(area, tile, type.tileWidth(), type.tileHeight()) == 0
                || !Map::isPlaceable(type, tile))
                return false;
            return true;
        };

        map<int, vector<TilePosition>> wallPlacements;

        // Round to nearest pi/8 rads
        auto nearestEight = int(round(chokeAngle / 0.3926991));
        defenseAngle = nearestEight % 8;

        // 0/8
        if (defenseAngle == 0) {
            wallPlacements[1] ={ {-6,-4}, {-4, -4}, {-2, -4}, {0, -4}, {2, -4}, {4, -4}, {6, -4}, {8, -4} };
            wallPlacements[2] ={ {-6,-2}, {-4, -2}, {-2, -2}, {0, -2}, {2, -2}, {4, -2}, {6, -2}, {8, -2} };
            wallPlacements[3] ={ {-6, 0}, {-4, 0}, {-2, 0}, {4, 0}, {6, 0}, {8, 0} };
            wallPlacements[4] ={ {-6, 2}, {-4, 2}, {-2, 2}, {4, 2}, {6, 2}, {8, 2} };
        }

        // pi/8
        if (defenseAngle == 1 || defenseAngle == 7) {
            wallPlacements[1] ={ {-6, -1}, {-4, -2}, {-2, -3}, {0, -4}, {2, -4}, {4, -5}, {6, -6}, {8, -7} };
            wallPlacements[2] ={ {-6, 1}, {-4, 0}, {-2, -1}, {0, -2}, {2, -2}, {4, -3}, {6, -4}, {8, -5} };
            wallPlacements[3] ={ {-6, 3}, {-4, 2}, {-2, 1}, {4, -1}, {6, -2}, {8, -3}, };
            wallPlacements[4] ={ {-6, 5}, {-4, 4}, {-3, 3}, {4, 1}, {6, 0}, {8, -1} };
        }

        // pi/4
        if (defenseAngle == 2 || defenseAngle == 6) {
            wallPlacements[1] ={ {-6, 0}, {-4, -2}, {-2, -4}, {0, -6} };
            wallPlacements[2] ={ {-6, 2}, {-4, 0}, {-2, -2}, {0, -4}, {2, -6} };
            wallPlacements[3] ={ {-6, 4}, {-4, 2}, {-2, 0}, {0, -2}, {2, -4}, {4, -6} };
            wallPlacements[4] ={ {-6, 6}, {-4, 4}, {-2, 2}, {2, -2}, {4, -4}, {6, -6} };
        }

        // 3pi/8
        if (defenseAngle == 3 || defenseAngle == 5) {
            wallPlacements[1] ={ {-4, 6}, {-4, 4}, {-4, 2}, {-4, 0}, {-3, -2}, {-2, -4}, {-1, -6} };
            wallPlacements[2] ={ {-2, 6}, {-2, 4}, {-2, 2}, {-2, 0}, {-1, -2}, {0, -4}, {1, -6} };
            wallPlacements[3] ={ {0, 5}, {0, 3}, {1, -2}, {2, -4}, {3, -6} };
            wallPlacements[4] ={ {2, 5}, {2, 3}, {3, -2}, {4, -4}, {5, -6} };
        }

        // pi/2
        if (defenseAngle == 4) {
            wallPlacements[1] ={ {-4, 6}, {-4, 4}, {-4, 2}, {-4, 0}, {-4, -2}, {-4, -4}, {-4, -6} };
            wallPlacements[2] ={ {-2, 6}, {-2, 4}, {-2, 2}, {-2, 0}, {-2, -2}, {-2, -4}, {-2, -6} };
            wallPlacements[3] ={ {0, 7}, {0, 5}, {0, 3}, {0, -2}, {0, -4}, {0, -6} };
            wallPlacements[4] ={ {2, 7}, {2, 5}, {2, 3}, {2, -2}, {2, -4}, {2, -6} };
        }

        // Flip them vertically / horizontally as needed
        if (base->Center().y < Position(choke->Center()).y) {
            for (int i = 1; i <= 4; i++) {
                for (auto &placement : wallPlacements[i])
                    placement.y = -(placement.y - 1);
            }
        }
        if (base->Center().x < Position(choke->Center()).x) {
            for (int i = 1; i <= 4; i++) {
                for (auto &placement : wallPlacements[i])
                    placement.x = -(placement.x - 2);
            }
        }

        auto defenseType = UnitTypes::None;
        if (Broodwar->self()->getRace() == Races::Protoss)
            defenseType = UnitTypes::Protoss_Photon_Cannon;
        if (Broodwar->self()->getRace() == Races::Terran)
            defenseType = UnitTypes::Terran_Missile_Turret;
        if (Broodwar->self()->getRace() == Races::Zerg)
            defenseType = UnitTypes::Zerg_Creep_Colony;

        // Add wall defenses to the set
        for (auto &[i, placements] : wallPlacements) {
            for (auto &placement : placements) {
                auto tile = base->Location() + placement;
                auto stationDefense = station->getDefenses().find(tile) != station->getDefenses().end();
                if ((!Map::isReserved(tile, 2, 2) && Map::isPlaceable(defenseType, tile)) || stationDefense) {
                    defenses[i].insert(tile);
                    Map::addReserve(tile, 2, 2);
                    Map::addUsed(tile, defenseType);
                    defenses[0].insert(tile);
                }
            }
        }

        // Add station defenses to the set
        for (auto &defense : station->getDefenses())
            defenses[0].insert(defense);
    }

    void Wall::scoreWall()
    {
        // Create a path searching for an opening
        allowLifted = false;
        auto pathOut = findPathOut();

        // If we want an open wall and it's not reachable, or we want a closed wall and it is reachable
        if ((openWall && !pathOut.isReachable()) || (!openWall && pathOut.isReachable())) {
            failedPath++;
            return;
        }

        auto pathCount = 1;
        vector<TilePosition> possibleDoors;
        for (int i = 0; i < 10; i++) {
            auto test = findOpening();
            if (test.isValid()) {
                possibleDoors.push_back(test);
                Map::addReserve(test, 1, 1);
                pathCount++;
            }
        }

        for (auto &tile : possibleDoors)
            Map::removeReserve(tile, 1, 1);

        // If we want a closed wall, we need to check if we can get out if we're Terran
        if (!openWall && Broodwar->self()->getRace() == Races::Terran) {
            allowLifted = true;
            auto pathOut = findPathOut();
            if (!pathOut.isReachable()) {
                failedPath++;
                return;
            }
        }

        // Find distance for each piece to the closest choke tile to the path start point
        auto dist = 1.0;
        if (openWall) {
            //auto optimalChokeTile = pathStart.getDistance(TilePosition(choke->Pos(choke->end1))) < pathStart.getDistance(TilePosition(choke->Pos(choke->end2))) ? Position(choke->Pos(choke->end1)) : Position(choke->Pos(choke->end2));
            for (auto &[tile, type] : currentLayout) {
                const auto center = Position(tile) + Position(type.tileWidth() * 16, type.tileHeight() * 16);
                const auto chokeDist = Position(choke->Center()).getDistance(center);
                dist += log(chokeDist);
            }
        }

        // Find distance for each piece to the center of the choke
        else {
            for (auto &[tile, type] : currentLayout) {
                const auto center = Position(tile) + Position(type.tileWidth() * 16, type.tileHeight() * 16);
                const auto chokeDist = center.getDistance(Position(choke->Center()) + Position(4, 4));
                (type == UnitTypes::Protoss_Pylon && !pylonWall && !pylonWallPiece) ? dist -= 5.0 * chokeDist : dist += chokeDist;
            }
        }

        // Score wall and store if better than current best layout
        auto score = dist / pathCount;
        if (pathCount < bestDoorCount || (pathCount <= bestDoorCount && score > bestWallScore)) {
            bestLayout = currentLayout;
            bestWallScore = score;
            bestDoorCount = pathCount;
        }
    }

    void Wall::cleanup()
    {
        // Remove used from tiles
        for (auto &tile : smallTiles)
            Map::removeUsed(tile, 2, 2);
        for (auto &tile : mediumTiles)
            Map::removeUsed(tile, 3, 2);
        for (auto &tile : largeTiles)
            Map::removeUsed(tile, 4, 3);
        for (auto &[_, tiles] : defenses) {
            for (auto &tile : tiles)
                Map::removeUsed(tile, 2, 2);
        }

        if (!valid)
            return;

        // Add a reserved path
        if (openWall) {
            for (auto &tile : finalPath.getTiles())
                Map::addReserve(tile, 1, 1);
        }
    }

    int Wall::getGroundDefenseCount()
    {
        // Returns how many visible ground defensive structures exist in this Walls defense locations
        int count = 0;
        for (auto &tile : defenses[0]) {
            auto &type = Map::isUsed(tile);
            if (type == UnitTypes::Protoss_Photon_Cannon
                || type == UnitTypes::Zerg_Sunken_Colony
                || type == UnitTypes::Terran_Bunker)
                count++;
        }
        return count;
    }

    int Wall::getAirDefenseCount()
    {
        // Returns how many visible air defensive structures exist in this Walls defense locations
        int count = 0;
        for (auto &tile : defenses[0]) {
            auto &type = Map::isUsed(tile);
            if (type == UnitTypes::Protoss_Photon_Cannon
                || type == UnitTypes::Zerg_Spore_Colony
                || type == UnitTypes::Terran_Missile_Turret)
                count++;
        }
        return count;
    }

    void Wall::draw()
    {
        set<Position> anglePositions;
        int color = Broodwar->self()->getColor();
        int textColor = color == 185 ? textColor = Text::DarkGreen : Broodwar->self()->getTextColor();

        // Draw boxes around each feature
        auto drawBoxes = true;
        if (drawBoxes) {
            for (auto &tile : smallTiles) {
                Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), color);
                Broodwar->drawTextMap(Position(tile) + Position(4, 4), "%cW", textColor);
                anglePositions.insert(Position(tile) + Position(32, 32));
            }
            for (auto &tile : mediumTiles) {
                Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(97, 65), color);
                Broodwar->drawTextMap(Position(tile) + Position(4, 4), "%cW", textColor);
                anglePositions.insert(Position(tile) + Position(48, 32));
                //tightCheck(UnitTypes::Terran_Supply_Depot, tile, true);
            }
            for (auto &tile : largeTiles) {
                Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(129, 97), color);
                Broodwar->drawTextMap(Position(tile) + Position(4, 4), "%cW", textColor);
                anglePositions.insert(Position(tile) + Position(64, 48));
                //tightCheck(UnitTypes::Terran_Barracks, tile, true);
            }
            for (int i = 1; i <= 4; i++) {
                for (auto &tile : defenses[i]) {
                    Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), color);
                    Broodwar->drawTextMap(Position(tile) + Position(4, 4), "%cW %d", textColor, i);
                }
            }
        }

        // Draw angles of each piece
        auto drawAngles = false;
        if (drawAngles) {
            for (auto &pos1 : anglePositions) {
                for (auto &pos2 : anglePositions) {
                    if (pos1 == pos2)
                        continue;
                    const auto angle = Map::getAngle(make_pair(pos1, pos2));

                    Broodwar->drawLineMap(pos1, pos2, color);
                    Broodwar->drawTextMap((pos1 + pos2) / 2, "%c%.2f", textColor, angle);
                }
            }
        }

        // Draw opening
        Broodwar->drawBoxMap(Position(opening), Position(opening) + Position(33, 33), color, true);

        // Draw the line and angle of the ChokePoint
        auto p1 = choke->Pos(choke->end1);
        auto p2 = choke->Pos(choke->end2);
        auto angle = Map::getAngle(make_pair(p1, p2));
        Broodwar->drawTextMap(Position(choke->Center()), "%c%.2f", Text::Grey, angle);
        Broodwar->drawLineMap(Position(p1), Position(p2), Colors::Grey);

        // Draw the path points        
        Broodwar->drawCircleMap(Position(pathStart), 6, Colors::Black, true);
        Broodwar->drawCircleMap(Position(pathEnd), 6, Colors::White, true);

        Broodwar->drawTextMap(Position(choke->Center()), "%d", defenseAngle);
    }
}

namespace BWEB::Walls {

    Wall* createHardWall(multimap<UnitType, TilePosition>& buildings, const BWEM::Area * area, const BWEM::ChokePoint * choke)
    {
        for (auto &[type, tile] : buildings)
            Map::addReserve(tile, type.tileWidth(), type.tileHeight());        
        BWEB::Wall wall(area, choke, buildings);
        walls.emplace(choke, wall);
        return &walls.at(choke);
    }

    Wall* createWall(vector<UnitType>& buildings, const BWEM::Area * area, const BWEM::ChokePoint * choke, const UnitType tightType, const vector<UnitType>& defenses, const bool openWall, const bool requireTight)
    {
        ofstream writeFile;
        string buffer;
        auto timePointNow = chrono::system_clock::now();
        auto timeNow = chrono::system_clock::to_time_t(timePointNow);

        // Print the clock position of this Wall
        auto clock = abs(round((Map::getAngle(make_pair(Map::mapBWEM.Center(), Position(area->Top()))) + 1.57) / 0.52));
        if (Position(area->Top()).x < Map::mapBWEM.Center().x)
            clock+= 6;

        // Open the log file if desired and write information
        if (logInfo) {
            writeFile.open("bwapi-data/write/BWEB_Wall.txt", std::ios::app);
            writeFile << ctime(&timeNow);
            writeFile << Broodwar->mapFileName().c_str() << endl;
            writeFile << "At: " << clock << " o'clock." << endl;
            writeFile << endl;

            writeFile << "Buildings:" << endl;
            for (auto &building : buildings)
                writeFile << building.c_str() << endl;
            writeFile << endl;
        }

        // Verify inputs are correct
        if (!area) {
            writeFile << "BWEB: Can't create a wall without a valid BWEM::Area" << endl;
            return nullptr;
        }

        if (!choke) {
            writeFile << "BWEB: Can't create a wall without a valid BWEM::Chokepoint" << endl;
            return nullptr;
        }

        // Verify not attempting to create a Wall in the same Area/ChokePoint combination
        for (auto &[_, wall] : walls) {
            if (wall.getArea() == area && wall.getChokePoint() == choke) {
                writeFile << "BWEB: Can't create a Wall where one already exists." << endl;
                return &wall;
            }
        }

        // Create a Wall
        Wall wall(area, choke, buildings, defenses, tightType, requireTight, openWall);

        // Verify the Wall creation was successful
        auto wallFound = (wall.getSmallTiles().size() + wall.getMediumTiles().size() + wall.getLargeTiles().size()) == wall.getRawBuildings().size();

        // Log information
        if (logInfo) {
            writeFile << "Failure Reasons:" << endl;
            writeFile << "Power: " << failedPower << endl;
            writeFile << "Angle: " << failedAngle << endl;
            writeFile << "Placement: " << failedPlacement << endl;
            writeFile << "Tight: " << failedTight << endl;
            writeFile << "Path: " << failedPath << endl;
            writeFile << "Spawn: " << failedSpawn << endl;
            writeFile << endl;

            double dur = std::chrono::duration <double, std::milli>(chrono::system_clock::now() - timePointNow).count();
            writeFile << "Generation Time: " << dur << "ms and " << (wallFound ? "successful." : "failed.") << endl;
            writeFile << "--------------------" << endl;
        }

        // If we found a suitable Wall, push into container and return pointer to it
        if (wallFound) {
            walls.emplace(choke, wall);
            return &walls.at(choke);
        }
        return nullptr;
    }

    Wall* createOpenWall()
    {
        return nullptr;
    }

    Wall* createSimCity()
    {
        return nullptr;
    }

    Wall* getClosestWall(TilePosition here)
    {
        auto distBest = DBL_MAX;
        Wall * bestWall = nullptr;
        for (auto &[_, wall] : walls) {
            const auto dist = here.getDistance(TilePosition(wall.getChokePoint()->Center()));

            if (dist < distBest) {
                distBest = dist;
                bestWall = &wall;
            }
        }
        return bestWall;
    }

    Wall* getWall(const BWEM::ChokePoint * choke)
    {
        if (!choke)
            return nullptr;

        for (auto &[_, wall] : walls) {
            if (wall.getChokePoint() == choke)
                return &wall;
        }
        return nullptr;
    }

    map<const BWEM::ChokePoint *, Wall>& getWalls() {
        return walls;
    }

    void draw()
    {
        for (auto &[_, wall] : walls)
            wall.draw();


        //return;
        for (int x = 0; x < Broodwar->mapWidth(); x++) {
            for (int y = 0; y < Broodwar->mapHeight(); y++) {
                auto t = TilePosition(x, y);
                if (testGrid[x][y] == 1)
                    Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Yellow);
                if (testGrid[x][y] == 2)
                    Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Blue);
                if (testGrid[x][y] == 3)
                    Broodwar->drawBoxMap(Position(t), Position(t) + Position(33, 33), Colors::Green);
            }
        }
    }
}
