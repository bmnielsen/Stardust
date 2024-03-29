#include "BuildingPlacement.h"

#include "Map.h"
#include "PathFinding.h"
#include "Geo.h"
#include "UnitUtil.h"

#include <cfloat>

const double pi = 3.14159265358979323846;

#if LOGGING_ENABLED
#define DEBUG_PLACEMENT true
#endif

namespace BuildingPlacement
{
    namespace
    {
        Base *natural;
        Choke *mainChoke;
        Choke *naturalChoke;

        struct PylonOption
        {
            BWAPI::TilePosition pylon;
            std::set<BWAPI::TilePosition> cannons;
            int score;
        };
        std::vector<PylonOption> pylonOptions;

        BWAPI::TilePosition pathfindingStartTile;
        BWAPI::TilePosition pathfindingEndTile;
        std::set<BWAPI::TilePosition> wallTiles;
        std::set<BWAPI::TilePosition> reservedTiles;
        std::set<BWAPI::WalkPosition> neutralWalkTiles;
        std::set<BWAPI::TilePosition> mineralFieldTiles;

        // Struct used when generating and scoring all of the forge + gateway options
        struct ForgeGatewayWallOption
        {
            BWAPI::TilePosition forge;
            BWAPI::TilePosition gateway;

            int gapSize;
            BWAPI::Position gapCenter;
            BWAPI::Position gapEnd1;
            BWAPI::Position gapEnd2;

            // Constructor for a valid option
            ForgeGatewayWallOption(BWAPI::TilePosition _forge,
                                   BWAPI::TilePosition _gateway,
                                   int _gapSize,
                                   BWAPI::Position _gapCenter,
                                   BWAPI::Position _gapEnd1,
                                   BWAPI::Position _gapEnd2)
                    : forge(_forge)
                    , gateway(_gateway)
                    , gapSize(_gapSize)
                    , gapCenter(_gapCenter)
                    , gapEnd1(_gapEnd1)
                    , gapEnd2(_gapEnd2)
            {};

            // Constructor for an invalid option
            ForgeGatewayWallOption(BWAPI::TilePosition _forge, BWAPI::TilePosition _gateway)
                    : forge(_forge)
                    , gateway(_gateway)
                    , gapSize(INT_MAX)
                    , gapCenter(BWAPI::Positions::Invalid)
                    , gapEnd1(BWAPI::Positions::Invalid)
                    , gapEnd2(BWAPI::Positions::Invalid)
            {};

            // Default constructor
            ForgeGatewayWallOption()
                    : forge(BWAPI::TilePositions::Invalid)
                    , gateway(BWAPI::TilePositions::Invalid)
                    , gapSize(INT_MAX)
                    , gapCenter(BWAPI::Positions::Invalid)
                    , gapEnd1(BWAPI::Positions::Invalid)
                    , gapEnd2(BWAPI::Positions::Invalid)
            {};

            [[nodiscard]] ForgeGatewayWall toWall() const
            {
                return {forge, gateway, BWAPI::TilePositions::Invalid, gapSize, gapCenter, gapEnd1, gapEnd2};
            }
        };

        void addBuildingToReservedTiles(BWAPI::TilePosition tile, BWAPI::UnitType type)
        {
            for (int x = tile.x; x < tile.x + type.tileWidth(); x++)
            {
                for (int y = tile.y; y < tile.y + type.tileHeight(); y++)
                {
                    reservedTiles.insert(BWAPI::TilePosition(x, y));
                }
            }
        }

        bool walkableTile(BWAPI::TilePosition tile)
        {
            return tile.isValid() &&
                   Map::isWalkable(tile) &&
                   !reservedTiles.contains(tile) &&
                   !wallTiles.contains(tile);
        }

        bool buildableTile(BWAPI::TilePosition tile)
        {
            return walkableTile(tile) && BWAPI::Broodwar->isBuildable(tile.x, tile.y);
        }

        bool buildable(BWAPI::UnitType type, BWAPI::TilePosition tile)
        {
            for (int y = tile.y; y < tile.y + type.tileHeight(); y++)
            {
                for (int x = tile.x; x < tile.x + type.tileWidth(); x++)
                {
                    if (!buildableTile(BWAPI::TilePosition(x, y))) return false;
                }
            }

            // Also consider it not buildable if it is flush against one of the natural mineral fields
            for (int y = 0; y < type.tileHeight(); y++)
            {
                if (mineralFieldTiles.contains(tile + BWAPI::TilePosition(-1, y))) return false;
                if (mineralFieldTiles.contains(tile + BWAPI::TilePosition(type.tileWidth(), y))) return false;
            }
            for (int x = 0; x < type.tileWidth(); x++)
            {
                if (mineralFieldTiles.contains(tile + BWAPI::TilePosition(x, -1))) return false;
                if (mineralFieldTiles.contains(tile + BWAPI::TilePosition(x, type.tileHeight()))) return false;
            }

            return true;
        }

        void generatePylonOptions()
        {
            pylonOptions.clear();

            // Determine what direction the choke is in compared to the natural
            auto direction = Geo::directionFromBuilding(natural->getTilePosition(),
                                                        BWAPI::UnitTypes::Protoss_Nexus.tileSize(),
                                                        naturalChoke->center,
                                                        true);

            // Generate the pylon positions we want to consider
            std::set<BWAPI::TilePosition> options;
            if (direction == Geo::Direction::up)
            {
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(-2, -1));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(-1, -2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(0, -2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(1, -2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(2, -2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(3, -2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(4, -2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(4, -1));
            }
            else if (direction == Geo::Direction::down)
            {
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(-2, 2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(-2, 3));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(-1, 3));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(0, 3));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(1, 3));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(2, 3));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(3, 3));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(4, 2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(4, 3));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(5, 2));
                options.insert(natural->getTilePosition() + BWAPI::TilePosition(5, 3));
            }
            else if (direction == Geo::Direction::left)
            {
                for (int x = -3; x <= -2; x++)
                    for (int y = -1; y <= 3; y++)
                        options.insert(natural->getTilePosition() + BWAPI::TilePosition(x, y));
            }
            else if (direction == Geo::Direction::right)
            {
                for (int x = 4; x <= 5; x++)
                    for (int y = -2; y <= 3; y++)
                        options.insert(natural->getTilePosition() + BWAPI::TilePosition(x, y));
            }
            else
            {
                return;
            }

            auto analysis = initializeBaseDefenseAnalysis(natural);
            auto end1 = std::get<0>(analysis);
            auto end2 = std::get<1>(analysis);
            auto initialPositions = std::get<2>(analysis);
            if (!end1 || !end2 || initialPositions.empty()) return;

            // Now consider each possibility and score them based on how well-placed the cannons can be
            for (auto pylon : options)
            {
                // Skip pylons in the mineral line or unbuildable
                if (!buildable(BWAPI::UnitTypes::Protoss_Pylon, pylon)) continue;
                if (natural->isInMineralLine(pylon) || natural->isInMineralLine(pylon + BWAPI::TilePosition(1, 1))) continue;

                // Create copy of positions set so we can remove invalid options
                auto positions = initialPositions;
                auto usePosition = [&positions](BWAPI::TilePosition tile)
                {
                    for (int x = tile.x - 1; x <= tile.x + 1; x++)
                    {
                        for (int y = tile.y - 1; y <= tile.y + 1; y++)
                        {
                            positions.erase(BWAPI::TilePosition(x, y));
                        }
                    }
                };
                usePosition(pylon);

                std::set<BWAPI::TilePosition> cannons;

                // Now place a cannon closest to each end
                auto placeEnd = [&](const Resource& end)
                {
                    int minDist = INT_MAX;
                    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                    for (auto tile : positions)
                    {
                        if (!UnitUtil::Powers(pylon, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;

                        int dist = end->center.getApproxDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16));
                        if (dist < minDist)
                        {
                            minDist = dist;
                            best = tile;
                        }
                    }

                    if (best != BWAPI::TilePositions::Invalid)
                    {
                        usePosition(best);
                        cannons.insert(best);
                    }

                    return minDist;
                };
                int score = placeEnd(end1);
                score += placeEnd(end2);
                if (cannons.size() == 2)
                {
                    pylonOptions.push_back(PylonOption{pylon, cannons, score});
                }
            }

            std::sort(pylonOptions.begin(), pylonOptions.end(), [](const PylonOption &a, const PylonOption &b) {
                return a.score < b.score;
            });
        }

        void addWallTiles(BWAPI::TilePosition tile, BWAPI::TilePosition size)
        {
            for (int x = tile.x; x < tile.x + size.x; x++)
            {
                for (int y = tile.y; y < tile.y + size.y; y++)
                {
                    wallTiles.insert(BWAPI::TilePosition(x, y));
                }
            }
        }

        void removeWallTiles(BWAPI::TilePosition tile, BWAPI::TilePosition size)
        {
            for (int x = tile.x; x < tile.x + size.x; x++)
            {
                for (int y = tile.y; y < tile.y + size.y; y++)
                {
                    wallTiles.erase(BWAPI::TilePosition(x, y));
                }
            }
        }

        bool validPathfindingTile(BWAPI::TilePosition tile)
        {
            return walkableTile(tile) && !natural->mineralLineTiles.contains(tile);
        }

        size_t pathLength(BWAPI::TilePosition alternateStartTile = BWAPI::TilePositions::Invalid)
        {
            auto startTile = pathfindingStartTile;
            if (alternateStartTile != BWAPI::TilePositions::Invalid)
            {
                startTile = alternateStartTile;
            }

            return PathFinding::Search(startTile, pathfindingEndTile, validPathfindingTile).size();
        }

        bool hasPathWithBuilding(BWAPI::TilePosition tile,
                                 BWAPI::TilePosition size,
                                 size_t maxPathLength = 0,
                                 BWAPI::TilePosition alternateStartTile = BWAPI::TilePositions::Invalid)
        {
            addWallTiles(tile, size);
            auto length = pathLength(alternateStartTile);
            removeWallTiles(tile, size);

            if (length == 0) return false;
            if (maxPathLength > 0 && (int)length > maxPathLength) return false;

            return true;
        }

        void swap(BWAPI::TilePosition &first, BWAPI::TilePosition &second)
        {
            BWAPI::TilePosition tmp = second;
            second = first;
            first = tmp;
        }

        BWAPI::Position center(BWAPI::TilePosition tile)
        {
            return BWAPI::Position(tile) + BWAPI::Position(16, 16);
        }

        BWAPI::Position center(BWAPI::WalkPosition walk)
        {
            return BWAPI::Position(walk) + BWAPI::Position(4, 4);
        }

        bool walkableAbove(BWAPI::TilePosition tile, int toleranceAbove, int toleranceLeft, int toleranceRight)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start(tile);
            for (int x = (0 - toleranceLeft); x < (4 + toleranceRight); x++)
            {
                for (int y = 1; y <= toleranceAbove; y++)
                {
                    BWAPI::WalkPosition walk(start.x + x, start.y - y);
                    if (!walk.isValid() || !BWAPI::Broodwar->isWalkable(walk) || neutralWalkTiles.contains(walk)) return false;
                }
            }

            return true;
        }

        bool walkableBelow(BWAPI::TilePosition tile, int toleranceBelow, int toleranceLeft, int toleranceRight)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start(tile);
            for (int x = (0 - toleranceLeft); x < (4 + toleranceRight); x++)
            {
                for (int y = 1; y <= toleranceBelow; y++)
                {
                    BWAPI::WalkPosition walk(start.x + x, start.y + 3 + y);
                    if (!walk.isValid() || !BWAPI::Broodwar->isWalkable(walk) || neutralWalkTiles.contains(walk)) return false;
                }
            }

            return true;
        }

        bool walkableLeft(BWAPI::TilePosition tile, int toleranceLeft, int toleranceAbove, int toleranceBelow)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start(tile);
            for (int y = (0 - toleranceAbove); y < (4 + toleranceBelow); y++)
            {
                for (int x = 1; x <= toleranceLeft; x++)
                {
                    BWAPI::WalkPosition walk(start.x - x, start.y + y);
                    if (!walk.isValid() || !BWAPI::Broodwar->isWalkable(walk) || neutralWalkTiles.contains(walk)) return false;
                }
            }

            return true;
        }

        bool walkableRight(BWAPI::TilePosition tile, int toleranceRight, int toleranceAbove, int toleranceBelow)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start(tile);
            for (int y = (0 - toleranceAbove); y < (4 + toleranceBelow); y++)
            {
                for (int x = 1; x <= toleranceRight; x++)
                {
                    BWAPI::WalkPosition walk(start.x + 3 + x, start.y + y);
                    if (!walk.isValid() || !BWAPI::Broodwar->isWalkable(walk) || neutralWalkTiles.contains(walk)) return false;
                }
            }

            return true;
        }

        void addBuildingOption(int x,
                               int y,
                               BWAPI::UnitType building,
                               std::set<BWAPI::TilePosition> &buildingOptions,
                               bool tight)
        {
            // Collect the possible build locations covering this tile
            std::set<BWAPI::TilePosition> tiles;

            bool isForge = (building == BWAPI::UnitTypes::Protoss_Forge);
            bool isGate = (building == BWAPI::UnitTypes::Protoss_Gateway);

            // Blocked on top
            if ((isForge && !walkableAbove(BWAPI::TilePosition(x, y), 1, 1, 1))
                || (!tight && !Map::isTerrainWalkable(x, y - 1)))
            {
                for (int i = 0; i < building.tileWidth(); i++)
                {
#if DEBUG_PLACEMENT
                    auto tile = BWAPI::TilePosition(x - i, y);
                    auto result = tiles.insert(tile);
                    if (result.second && tile.isValid() && buildable(building, tile))
                    {
                        Log::Debug() << building << " option at " << tile << ": blocked above " << BWAPI::TilePosition(x, y);
                    }
#else
                    tiles.insert(BWAPI::TilePosition(x - i, y));
#endif
                }
            }

            // Blocked on left
            if ((isForge && !walkableLeft(BWAPI::TilePosition(x, y), 1, 1, 1))
                || (!tight && !Map::isTerrainWalkable(x - 1, y)))
            {
                for (int i = 0; i < building.tileHeight(); i++)
                {
#if DEBUG_PLACEMENT
                    auto tile = BWAPI::TilePosition(x, y - i);
                    auto result = tiles.insert(tile);
                    if (result.second && tile.isValid() && buildable(building, tile))
                    {
                        Log::Debug() << building << " option at " << tile << ": blocked on left " << BWAPI::TilePosition(x, y);
                    }
#else
                    tiles.insert(BWAPI::TilePosition(x, y - i));
#endif
                }
            }

            // Blocked on bottom
            if ((isForge && !walkableBelow(BWAPI::TilePosition(x, y), 1, 1, 1))
                || (isGate && !walkableBelow(BWAPI::TilePosition(x, y), 2, 0, 1))
                || (!tight && !Map::isTerrainWalkable(x, y + 1)))
            {
                int thisY = y - building.tileHeight() + 1;
                for (int i = 0; i < building.tileWidth(); i++)
                {
#if DEBUG_PLACEMENT
                    auto tile = BWAPI::TilePosition(x - i, thisY);
                    auto result = tiles.insert(tile);
                    if (result.second && tile.isValid() && buildable(building, tile))
                    {
                        Log::Debug() << building << " option at " << tile << ": blocked on bottom " << BWAPI::TilePosition(x, y);
                    }
#else
                    tiles.insert(BWAPI::TilePosition(x - i, thisY));
#endif
                }
            }

            // Blocked on right
            if ((isForge && !walkableRight(BWAPI::TilePosition(x, y), 1, 1, 1))
                || (isGate && !walkableRight(BWAPI::TilePosition(x, y), 1, 0, 2))
                || (!tight && !Map::isTerrainWalkable(x + 1, y)))
            {
                int thisX = x - building.tileWidth() + 1;
                for (int i = 0; i < building.tileHeight(); i++)
                {
#if DEBUG_PLACEMENT
                    auto tile = BWAPI::TilePosition(thisX, y - i);
                    auto result = tiles.insert(tile);
                    if (result.second && tile.isValid() && buildable(building, tile))
                    {
                        Log::Debug() << building << " option at " << tile << ": blocked on right " << BWAPI::TilePosition(x, y);
                    }
#else
                    tiles.insert(BWAPI::TilePosition(thisX, y - i));
#endif
                }
            }

            // Add all valid positions to the options set
            for (BWAPI::TilePosition tile : tiles)
            {
                if (!tile.isValid()) continue;
                if (!buildable(building, tile)) continue;
                buildingOptions.insert(tile);
            }
        }

        void addBuildingGeo(BWAPI::TilePosition tile, BWAPI::UnitType type, std::set<BWAPI::WalkPosition> &geo)
        {
            // Compute walkable pixels in each dimension
            int pixelsLeft = (type.tileWidth() * 16) - type.dimensionLeft();
            int pixelsRight = (type.tileWidth() * 16) - type.dimensionRight() - 1;
            int pixelsTop = (type.tileHeight() * 16) - type.dimensionUp();
            int pixelsBottom = (type.tileHeight() * 16) - type.dimensionDown() - 1;

            // Compute offset with first fully-unwalkable walktile in each dimension
            int left = (pixelsLeft + 7) / 8;
            int right = (type.tileWidth() * 4) - ((pixelsRight + 7) / 8) - 1;
            int top = (pixelsTop + 7) / 8;
            int bottom = (type.tileHeight() * 4) - ((pixelsBottom + 7) / 8) - 1;

            BWAPI::WalkPosition start(tile);

            // Top and bottom rows
            for (int x = left; x <= right; x++)
            {
                geo.insert(start + BWAPI::WalkPosition(x, top));
                geo.insert(start + BWAPI::WalkPosition(x, bottom));
            }

            // Left and right columns, ignoring corners that were already handled above
            for (int y = (top + 1); y <= (bottom - 1); y++)
            {
                geo.insert(start + BWAPI::WalkPosition(left, y));
                geo.insert(start + BWAPI::WalkPosition(right, y));
            }
        }

        void addWallOption(BWAPI::TilePosition forge,
                           BWAPI::TilePosition gateway,
                           std::set<BWAPI::WalkPosition> *geo,
                           std::vector<ForgeGatewayWallOption> &wallOptions)
        {
            // Check if we've already considered this wall
            for (auto const &option : wallOptions)
            {
                if (option.forge == forge && option.gateway == gateway)
                {
                    return;
                }
            }

            // Buildings overlap
            if (Geo::Overlaps(forge, 3, 2, gateway, 4, 3))
            {
                wallOptions.emplace_back(forge, gateway);
                return;
            }

            // Buildings cannot be placed
            if (!buildable(BWAPI::UnitTypes::Protoss_Forge, forge) ||
                !buildable(BWAPI::UnitTypes::Protoss_Gateway, gateway))
            {
                wallOptions.emplace_back(forge, gateway);
                return;
            }

            // Set up the sets of positions we are comparing between
            std::set<BWAPI::WalkPosition> geo1;
            std::set<BWAPI::WalkPosition> *geo2;

            if (geo)
            {
                addBuildingGeo(forge, BWAPI::UnitTypes::Protoss_Forge, geo1);
                addBuildingGeo(gateway, BWAPI::UnitTypes::Protoss_Gateway, geo1);
                geo2 = geo;
            }
            else
            {
                addBuildingGeo(forge, BWAPI::UnitTypes::Protoss_Forge, geo1);
                geo2 = new std::set<BWAPI::WalkPosition>();
                addBuildingGeo(gateway, BWAPI::UnitTypes::Protoss_Gateway, *geo2);
            }

            BWAPI::Position natCenter = natural->getPosition();
            double bestDist = DBL_MAX;
            double bestNatDist = DBL_MAX;
            BWAPI::Position bestCenter = BWAPI::Positions::Invalid;

            BWAPI::Position end1;
            BWAPI::Position end2;

            for (BWAPI::WalkPosition firstWP : geo1)
            {
                for (BWAPI::WalkPosition secondWP : *geo2)
                {
                    auto first = center(firstWP);
                    auto second = center(secondWP);

                    double dist = first.getDistance(second);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestCenter = BWAPI::Position((first.x + second.x) / 2, (first.y + second.y) / 2);
                        bestNatDist = bestCenter.getDistance(natCenter);
                        end1 = first;
                        end2 = second;
                    }
                    else if (std::abs(dist - bestDist) < 0.0001)
                    {
                        BWAPI::Position thisCenter = BWAPI::Position((first.x + second.x) / 2, (first.y + second.y) / 2);
                        double natDist = thisCenter.getDistance(natCenter);

                        if (natDist < bestNatDist)
                        {
                            bestCenter = thisCenter;
                            bestNatDist = natDist;
                            end1 = first;
                            end2 = second;
                        }
                    }
                }
            }

            if (!bestCenter.isValid())
            {
                Log::Debug() << "Error scoring wall forge " << forge << ", gateway " << gateway << ", geo1 size " << geo1.size() << ", geo2 size "
                             << geo2->size();
                wallOptions.emplace_back(forge, gateway);
                return;
            }

            if (!geo)
            {
                delete geo2;
            }

            // Gap must be at least 64
            if (bestDist < 64.0)
            {
                wallOptions.emplace_back(forge, gateway);
                return;
            }

            wallOptions.emplace_back(forge, gateway, (int)floor(bestDist / 16.0) - 2, bestCenter, end1, end2);
#if DEBUG_PLACEMENT
            Log::Debug() << "Scored wall forge " << forge << ", gateway " << gateway << ", gap " << bestDist << ", center "
                         << BWAPI::TilePosition(bestCenter);
#endif
        }

        // Computes the angle with the x-axis of a vector as defined by points p0, p1
        double vectorAngle(BWAPI::Position p0, BWAPI::Position p1)
        {
            // Infinite slope has an arctan of pi/2
            if (p0.x == p1.x) return pi / 2;

            // Angle is the arctan of the slope
            return std::atan(double(p1.y - p0.y) / double(p1.x - p0.x));
        }

        // Computes the angular distance between the vectors a and b (as defined by points a0, a1, b0, b1)
        double angularDistance(BWAPI::Position a0, BWAPI::Position a1, BWAPI::Position b0, BWAPI::Position b1)
        {
            double a = vectorAngle(a0, a1);
            double b = vectorAngle(b0, b1);

            return std::min(std::abs(a - b), pi - std::abs(a - b));
        }

        bool sideOfLine(BWAPI::Position lineEnd1, BWAPI::Position lineEnd2, BWAPI::Position point)
        {
            return ((point.x - lineEnd1.x) * (lineEnd2.y - lineEnd1.y)) - ((point.y - lineEnd1.y) * (lineEnd2.x - lineEnd1.x)) < 0;
        }

        void analyzeWallGeo(ForgeGatewayWall &wall)
        {
            // Pull the gap ends so they are on the closest unwalkable positions
            std::set<BWAPI::Position> unwalkablePositions;
            auto addBuildingToUnwalkablePositions = [&unwalkablePositions](BWAPI::UnitType type, BWAPI::TilePosition tile)
            {
                auto buildingCenter = BWAPI::Position(tile) + BWAPI::Position(type.tileWidth() * 16, type.tileHeight() * 16);
                for (int x = (buildingCenter.x - type.dimensionLeft()); x <= (buildingCenter.x + type.dimensionRight()); x++)
                {
                    for (int y = (buildingCenter.y - type.dimensionUp()); y <= (buildingCenter.y + type.dimensionDown()); y++)
                    {
                        unwalkablePositions.insert({x, y});
                    }
                }
            };
            auto walkablePos = [&unwalkablePositions](BWAPI::Position pos)
            {
                if (!pos.isValid()) return false;
                if (unwalkablePositions.contains(pos)) return false;
                auto walk = BWAPI::WalkPosition(pos);
                if (neutralWalkTiles.contains(walk)) return false;
                if (!BWAPI::Broodwar->isWalkable(walk)) return false;
                return true;
            };
            auto pullToWalkable = [&walkablePos](BWAPI::Position end, BWAPI::Position other)
            {
                int distTotal = end.getApproxDistance(other);
                int xdiff = other.x - end.x;
                int ydiff = other.y - end.y;

                // If the end is walkable at the start, we pull in the opposite direction until we hit something that isn't walkable
                if (walkablePos(end))
                {
                    for (int distStop = 1; distStop <= distTotal; distStop++)
                    {
                        BWAPI::Position pos(
                                end.x - (int) std::round(((double) distStop / distTotal) * xdiff),
                                end.y - (int) std::round(((double) distStop / distTotal) * ydiff));

                        if (walkablePos(pos)) continue;

                        return pos;
                    }

                    Log::Get() << "WARNING: Pulling wall gap ends did not find unwalkable moving backwards from " << end << " vs. " << other;
                    return end;
                }

                auto previous = end;
                for (int distStop = 1; distStop <= distTotal; distStop++)
                {
                    BWAPI::Position pos(
                            end.x + (int) std::round(((double) distStop / distTotal) * xdiff),
                            end.y + (int) std::round(((double) distStop / distTotal) * ydiff));

                    if (!walkablePos(pos))
                    {
                        previous = pos;
                        continue;
                    }

                    return previous;
                }

                Log::Get() << "WARNING: Pulling wall gap ends did not find a walkable position between " << end << " and " << other;
                return end;
            };
            addBuildingToUnwalkablePositions(BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            addBuildingToUnwalkablePositions(BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            wall.gapEnd1 = pullToWalkable(wall.gapEnd1, wall.gapEnd2);
            wall.gapEnd2 = pullToWalkable(wall.gapEnd2, wall.gapEnd1);
            wall.gapCenter = (wall.gapEnd1 + wall.gapEnd2) / 2;
            double gapLength = wall.gapEnd1.getDistance(wall.gapEnd2);
            wall.gapSize = (int)ceil((gapLength / 16.0) - 0.00001);

            // Compute probe blocking positions
            int probes = (int)ceil(((gapLength - 15.0) / 38.0) - 0.0001);
            double spaceBetween = (gapLength - ((double)(23 * probes))) / ((double)(probes + 1));
            int xdiff = wall.gapEnd2.x - wall.gapEnd1.x;
            int ydiff = wall.gapEnd2.y - wall.gapEnd1.y;
            for (int i = 0; i < probes; i++)
            {
                double distStop = (23.0 * i) + (spaceBetween * (i + 1)) + 11.5;
                wall.probeBlockingPositions.emplace(
                        wall.gapEnd1.x + (int)std::round(((double)distStop / gapLength) * xdiff),
                        wall.gapEnd1.y + (int)std::round(((double)distStop / gapLength) * ydiff)
                );
            }

            // Compute the sets of tiles inside and outside the wall

            // We start by setting forge and gateway building tiles as inside the wall
            auto addBuildingToInsideTiles = [&wall](BWAPI::UnitType type, BWAPI::TilePosition tile)
            {
                for (int x = tile.x; x < (tile.x + type.tileWidth()); x++)
                {
                    for (int y = tile.y; y < (tile.y + type.tileHeight()); y++)
                    {
                        wall.tilesInsideWall.emplace(x, y);
                    }
                }
            };
            addBuildingToInsideTiles(BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            addBuildingToInsideTiles(BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);

            // Then we trace the wall gap as inside the wall
            std::vector<BWAPI::TilePosition> gapTiles;
            Geo::FindTilesBetween(BWAPI::TilePosition(wall.gapEnd1), BWAPI::TilePosition(wall.gapEnd2), gapTiles);
            std::copy(gapTiles.begin(), gapTiles.end(), std::inserter(wall.tilesInsideWall, wall.tilesInsideWall.end()));

            // Then we flood-fill inside tiles from the natural center, not going outside the natural area unless close to the gap center
            {
                auto gapCenterTile = BWAPI::TilePosition(wall.gapCenter);
                std::queue<BWAPI::TilePosition> queue;
                auto visited = wall.tilesInsideWall;
                auto visit = [&](BWAPI::TilePosition tile)
                {
                    if (visited.contains(tile)) return;
                    visited.insert(tile);

                    if (!tile.isValid()) return;
                    if (!Map::isWalkable(tile)) return;

                    if (tile.getApproxDistance(gapCenterTile) > 6 && BWEM::Map::Instance().GetNearestArea(tile) != natural->getArea()) return;

                    wall.tilesInsideWall.insert(tile);

                    queue.emplace(tile.x - 1, tile.y);
                    queue.emplace(tile.x + 1, tile.y);
                    queue.emplace(tile.x, tile.y - 1);
                    queue.emplace(tile.x, tile.y + 1);
                };
                queue.push(natural->getTilePosition() + BWAPI::TilePosition(-1, -1));
                while (!queue.empty())
                {
                    auto current = queue.front();
                    queue.pop();
                    visit(current);
                }
            }

            // Mark the outside edge of the forge and gateway as being outside the wall
            auto outside = [&wall](BWAPI::TilePosition tile)
            {
                if (!tile.isValid()) return false;
                if (!Map::isWalkable(tile)) return false;
                return !wall.tilesInsideWall.contains(tile);
            };
            auto markOutsideBuildingEdge = [&wall, &outside](BWAPI::UnitType type, BWAPI::TilePosition tile)
            {
                for (int x = tile.x; x < (tile.x + type.tileWidth()); x++)
                {
                    for (int y = tile.y; y < (tile.y + type.tileHeight()); y++)
                    {
                        if (outside(BWAPI::TilePosition(x - 1, y)) ||
                            outside(BWAPI::TilePosition(x + 1, y)) ||
                            outside(BWAPI::TilePosition(x, y - 1)) ||
                            outside(BWAPI::TilePosition(x, y + 1)))
                        {
                            wall.tilesOutsideWall.emplace(x, y);
                            wall.tilesOutsideButCloseToWall.emplace(x, y);
                        }
                    }
                }
                for (int x = tile.x; x < (tile.x + type.tileWidth()); x++)
                {
                    for (int y = tile.y; y < (tile.y + type.tileHeight()); y++)
                    {
                        BWAPI::TilePosition here(x, y);
                        if (wall.tilesOutsideWall.contains(here))
                        {
                            wall.tilesInsideWall.erase(here);
                        }
                    }
                }
            };
            markOutsideBuildingEdge(BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            markOutsideBuildingEdge(BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);

            // Mark the outside bordering tiles to the gap tiles as outside the wall
            auto markIfOutside = [&wall, &outside](BWAPI::TilePosition tile)
            {
                if (!outside(tile)) return;

                wall.tilesOutsideWall.insert(tile);
                wall.tilesOutsideButCloseToWall.insert(tile);
            };
            for (auto &tile : gapTiles)
            {
                markIfOutside(BWAPI::TilePosition(tile.x - 1, tile.y));
                markIfOutside(BWAPI::TilePosition(tile.x + 1, tile.y));
                markIfOutside(BWAPI::TilePosition(tile.x, tile.y - 1));
                markIfOutside(BWAPI::TilePosition(tile.x, tile.y + 1));
            }

            // Flood-fill the outside tiles until we reach 10 tiles away
            {
                std::queue<std::pair<BWAPI::TilePosition, int>> queue;
                std::set<BWAPI::TilePosition> visited;
                auto visit = [&](BWAPI::TilePosition tile, int dist)
                {
                    if (visited.contains(tile)) return;
                    visited.insert(tile);

                    if (!tile.isValid()) return;
                    if (!Map::isWalkable(tile)) return;

                    if (wall.tilesInsideWall.contains(tile)) return;

                    wall.tilesOutsideWall.insert(tile);
                    if (dist < 3)
                    {
                        wall.tilesOutsideButCloseToWall.insert(tile);
                    }

                    if (dist == 10) return;

                    queue.emplace(std::make_pair<BWAPI::TilePosition, int>({tile.x - 1, tile.y}, dist + 1));
                    queue.emplace(std::make_pair<BWAPI::TilePosition, int>({tile.x + 1, tile.y}, dist + 1));
                    queue.emplace(std::make_pair<BWAPI::TilePosition, int>({tile.x, tile.y - 1}, dist + 1));
                    queue.emplace(std::make_pair<BWAPI::TilePosition, int>({tile.x, tile.y + 1}, dist + 1));
                };
                for (auto tile : wall.tilesOutsideWall)
                {
                    queue.emplace(tile, 0);
                }
                while (!queue.empty())
                {
                    auto current = queue.front();
                    queue.pop();
                    visit(current.first, current.second);
                }
            }
        }

        BWAPI::TilePosition gatewaySpawnPosition(const ForgeGatewayWall &wall, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType)
        {
            // Populate a vector of possible spawn tiles in the order they are considered
            std::vector<BWAPI::TilePosition> tiles;
            tiles.push_back(wall.gateway + BWAPI::TilePosition(0, 3));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(1, 3));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(2, 3));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(3, 3));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 3));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 2));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(4, 0));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(4, -1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(3, -1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(2, -1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(1, -1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(0, -1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, -1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 0));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 1));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 2));
            tiles.push_back(wall.gateway + BWAPI::TilePosition(-1, 3));

            // Return the first tile that is:
            // - Valid
            // - Walkable
            // - Not overlapping any existing building
            // - Not overlapping the building being scored

            BWAPI::TilePosition result = BWAPI::TilePositions::Invalid;
            addWallTiles(buildingTile, buildingType.tileSize());

            for (const auto &tile : tiles)
            {
                if (!tile.isValid() || !Map::isWalkable(tile)) continue;
                if (wallTiles.contains(tile)) continue;

                result = tile;
                break;
            }

            removeWallTiles(buildingTile, buildingType.tileSize());
            return result;
        }

        void initializePathfindingTiles()
        {
            auto &bwemMap = BWEM::Map::Instance();
            auto naturalArea = natural->getArea();

            // End tile

            // Initialize 5 tiles away from the choke in the opposite direction of the natural
            BWAPI::TilePosition start;

            auto p0 = BWAPI::Position(naturalChoke->center);
            auto p1 = natural->getPosition();

            // Special case where the slope is infinite
            if (p0.x == p1.x)
            {
                start = BWAPI::TilePosition(p0 + BWAPI::Position(0, (p0.y > p1.y ? 160 : -160)));
            }
            else
            {
                // First get the slope, m = (y1 - y0)/(x1 - x0)
                double m = double(p1.y - p0.y) / double(p1.x - p0.x);

                // Now the equation for a new x is x0 +- d/sqrt(1 + m^2)
                double x = p0.x + (p0.x > p1.x ? 1.0 : -1.0) * 160.0 / (sqrt(1 + m * m));

                // And y is m(x - x0) + y0
                double y = m * (x - p0.x) + p0.y;

                start = BWAPI::TilePosition(BWAPI::Position((int)x, (int)y));
            }

            // Now find the closest valid tile that is not in the natural area
            pathfindingEndTile = start;
            double bestDist = DBL_MAX;
            for (int x = start.x - 5; x < start.x + 5; x++)
            {
                for (int y = start.y - 5; y < start.y + 5; y++)
                {
                    BWAPI::TilePosition tile(x, y);
                    if (!tile.isValid()) continue;
                    if (!validPathfindingTile(tile)) continue;

                    auto area = bwemMap.GetArea(tile);
                    if (area && area != naturalArea)
                    {
                        double dist = tile.getDistance(start);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            pathfindingEndTile = tile;
                        }
                    }
                }
            }

#if DEBUG_PLACEMENT
            Log::Debug() << "Initializing end tile: nat@" << BWAPI::TilePosition(p1) << ";choke@" << BWAPI::TilePosition(p0) << ";start@"
                         << BWAPI::TilePosition(center(start)) << ";final@" << BWAPI::TilePosition(center(pathfindingEndTile));
#endif

            // Start tile

            // Initialize to a tile between the main choke and natural center, closer to the main choke
            auto mainChokeCenter = BWAPI::TilePosition(mainChoke->center);
            pathfindingStartTile = (mainChokeCenter + mainChokeCenter + mainChokeCenter + natural->getTilePosition()) / 4;

            if (!bwemMap.GetArea(pathfindingStartTile) || !Map::isTerrainWalkable(pathfindingStartTile.x, pathfindingStartTile.y))
            {
                BWAPI::TilePosition tileBest = BWAPI::TilePositions::Invalid;
                auto distBest = INT_MAX;
                for (auto x = pathfindingStartTile.x - 2; x < pathfindingStartTile.x + 2; x++)
                {
                    for (auto y = pathfindingStartTile.y - 2; y < pathfindingStartTile.y + 2; y++)
                    {
                        auto tile = BWAPI::TilePosition(x, y);
                        const auto dist = tile.getApproxDistance(pathfindingEndTile);

                        if (bwemMap.GetArea(tile) == naturalArea && dist < distBest)
                        {
                            tileBest = tile;
                            distBest = dist;
                        }
                    }
                }

                if (tileBest.isValid()) pathfindingStartTile = tileBest;
            }

#if DEBUG_PLACEMENT
            Log::Debug() << "Start tile: " << pathfindingStartTile;
#endif
        }

        void initializeNeutrals()
        {
            neutralWalkTiles.clear();

            for (const auto &unit : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                int width = 0;
                int height = 0;
                if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg || unit->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg)
                {
                    width = 4;
                    height = 4;
                }
                else if (unit->getType().isMineralField())
                {
                    width = 8;
                    height = 4;
                }
                else if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
                {
                    width = 16;
                    height = 8;
                }
                else
                {
                    continue;
                }

                int left = unit->getPosition().x - unit->getType().dimensionLeft();
                int top = unit->getPosition().y - unit->getType().dimensionUp();
                auto walk = BWAPI::WalkPosition(BWAPI::Position(left, top));
                for (int walkX = 0; walkX < width; walkX++)
                {
                    for (int walkY = 0; walkY < height; walkY++)
                    {
                        neutralWalkTiles.insert(walk + BWAPI::WalkPosition(walkX, walkY));
                    }
                }
            }

            mineralFieldTiles.clear();
            for (const auto &mineralPatch : natural->mineralPatches())
            {
                mineralFieldTiles.insert(mineralPatch->tile);
                mineralFieldTiles.insert(mineralPatch->tile + BWAPI::TilePosition(1, 0));
            }
        }

        bool overlapsNaturalArea(BWAPI::TilePosition tile, BWAPI::UnitType building)
        {
            auto &bwemMap = BWEM::Map::Instance();
            auto naturalArea = natural->getArea();

            for (auto x = tile.x; x < tile.x + building.tileWidth(); x++)
            {
                for (auto y = tile.y; y < tile.y + building.tileHeight(); y++)
                {
                    BWAPI::TilePosition test(x, y);
                    auto area = bwemMap.GetArea(test);
                    if (!area) area = bwemMap.GetNearestArea(test);
                    if (area == naturalArea) return true;
                }
            }

            return false;
        }

        bool areForgeAndGatewayTouching(BWAPI::TilePosition forge, BWAPI::TilePosition gateway)
        {
            return forge.x >= (gateway.x - 3)
                   && forge.x <= (gateway.x + 4)
                   && forge.y >= (gateway.y - 2)
                   && forge.y <= (gateway.y + 3);
        }

        void analyzeChokeGeoAndFindBuildingOptions(
                std::set<BWAPI::WalkPosition> &end1Geo,
                std::set<BWAPI::WalkPosition> &end2Geo,
                std::set<BWAPI::TilePosition> &end1ForgeOptions,
                std::set<BWAPI::TilePosition> &end1GatewayOptions,
                std::set<BWAPI::TilePosition> &end2ForgeOptions,
                std::set<BWAPI::TilePosition> &end2GatewayOptions,
                bool tight)
        {
            // Adds unwalkable walk positions that border a walkable walk position
            auto processEndGeo = [](BWAPI::TilePosition tile, std::set<BWAPI::WalkPosition> &geo)
            {
                auto walkable = [](BWAPI::WalkPosition pos)
                {
                    return pos.isValid() && BWAPI::Broodwar->isWalkable(pos) && !neutralWalkTiles.contains(pos);
                };

                for (int walkX = 0; walkX < 4; walkX++)
                {
                    for (int walkY = 0; walkY < 4; walkY++)
                    {
                        BWAPI::WalkPosition pos(tile.x * 4 + walkX, tile.y * 4 + walkY);
                        if (!walkable(pos) && (
                                walkable(pos + BWAPI::WalkPosition(1, 0)) ||
                                walkable(pos + BWAPI::WalkPosition(0, 1)) ||
                                walkable(pos + BWAPI::WalkPosition(-1, 0)) ||
                                walkable(pos + BWAPI::WalkPosition(0, -1))
                        ))
                        {
                            geo.insert(pos);
                        }
                    }
                }
            };

            // Get elevation of natural, we want our wall to be at the same elevation
            auto elevation = BWAPI::Broodwar->getGroundHeight(natural->getTilePosition());

            auto end1 = BWAPI::TilePosition(naturalChoke->choke->Pos(BWEM::ChokePoint::end1));
            auto end2 = BWAPI::TilePosition(naturalChoke->choke->Pos(BWEM::ChokePoint::end2));

            auto diffX = end1.x - end2.x;
            auto diffY = end1.y - end2.y;

            if (diffY >= -2 && diffY <= 2)
            {
                // Make sure end1 is the left side
                if (end1.x > end2.x) swap(end1, end2);

                // Straight vertical wall
#if DEBUG_PLACEMENT
                Log::Debug() << "Vertical wall between " << end1 << " and " << end2;
#endif

                // Find options on left side
                for (int x = end1.x - 2; x <= end1.x + 2; x++)
                {
                    for (int y = end1.y - 5; y <= end1.y + 5; y++)
                    {
                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        processEndGeo(tile, end1Geo);
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight);
                    }
                }

                // Find options on right side
                for (int x = end2.x - 2; x <= end2.x + 2; x++)
                {
                    for (int y = end2.y - 5; y <= end2.y + 5; y++)
                    {
                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        processEndGeo(tile, end2Geo);
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight);
                    }
                }
            }
            else if (diffX >= -2 && diffX <= 2)
            {
                // Make sure end1 is the top side
                if (end1.y > end2.y) swap(end1, end2);

                // Straight horizontal wall
#if DEBUG_PLACEMENT
                Log::Debug() << "Horizontal wall between " << end1 << " and " << end2;
#endif

                // Find options on top side
                for (int x = end1.x - 5; x <= end1.x + 5; x++)
                {
                    for (int y = end1.y - 2; y <= end1.y + 2; y++)
                    {
                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        processEndGeo(tile, end1Geo);
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight);
                    }
                }

                // Find options on bottom side
                for (int x = end2.x - 5; x <= end2.x + 5; x++)
                {
                    for (int y = end2.y - 2; y <= end2.y + 2; y++)
                    {
                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        processEndGeo(tile, end2Geo);
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight);
                    }
                }
            }
            else
            {
                // Make sure end1 is the left side
                if (end1.x > end2.x) swap(end1, end2);

                // Diagonal wall
#if DEBUG_PLACEMENT
                Log::Debug() << "Diagonal wall between " << end1 << " and " << end2;
#endif

                BWAPI::Position end1Center = center(end1);
                BWAPI::Position end2Center = center(end2);

                // Follow the slope perpendicular to the choke on both ends
                double m = (-1.0) / (((double)end2Center.y - end1Center.y) / ((double)end2Center.x - end1Center.x));
                for (int xdelta = -4; xdelta <= 4; xdelta++)
                {
                    for (int ydelta = -3; ydelta <= 3; ydelta++)
                    {
                        // Find options on left side
                        {
                            int x = end1.x + xdelta;
                            int y = end1.y + (int)std::round(xdelta * m) + ydelta;

                            BWAPI::TilePosition tile(x, y);
                            if (!tile.isValid()) continue;

                            auto forgeCenter = BWAPI::Position(tile) + BWAPI::Position(48, 32);
                            auto gatewayCenter = BWAPI::Position(tile) + BWAPI::Position(64, 48);
                            auto forgeOK = (forgeCenter.getDistance(end1Center) < (0.8 * forgeCenter.getDistance(end2Center)));
                            auto gateOK = (gatewayCenter.getDistance(end1Center) < (0.8 * gatewayCenter.getDistance(end2Center)));
                            if (!forgeOK && !gateOK) continue;

                            processEndGeo(tile, end1Geo);
                            if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                            if (forgeOK) addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight);
                            if (gateOK) addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight);
                        }

                        // Find options on right side
                        {
                            int x = end2.x + xdelta;
                            int y = end2.y + (int)std::round(xdelta * m) + ydelta;

                            BWAPI::TilePosition tile(x, y);
                            if (!tile.isValid()) continue;

                            auto forgeCenter = BWAPI::Position(tile) + BWAPI::Position(48, 32);
                            auto gatewayCenter = BWAPI::Position(tile) + BWAPI::Position(64, 48);
                            auto forgeOK = (forgeCenter.getDistance(end2Center) < (0.8 * forgeCenter.getDistance(end1Center)));
                            auto gateOK = (gatewayCenter.getDistance(end2Center) < (0.8 * gatewayCenter.getDistance(end1Center)));
                            if (!forgeOK && !gateOK) continue;

                            processEndGeo(tile, end2Geo);
                            if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                            if (forgeOK) addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight);
                            if (gateOK) addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight);
                        }
                    }
                }
            }
        }

        void generateWallOptions(
                std::vector<ForgeGatewayWallOption> &wallOptions,
                std::set<BWAPI::WalkPosition> &end1Geo,
                std::set<BWAPI::WalkPosition> &end2Geo,
                std::set<BWAPI::TilePosition> &end1ForgeOptions,
                std::set<BWAPI::TilePosition> &end1GatewayOptions,
                std::set<BWAPI::TilePosition> &end2ForgeOptions,
                std::set<BWAPI::TilePosition> &end2GatewayOptions,
                int maxGapSize)
        {
            // Forge on end1 side
            for (BWAPI::TilePosition forge : end1ForgeOptions)
            {
                // Gateway on end2 side
                for (BWAPI::TilePosition gate : end2GatewayOptions)
                {
                    addWallOption(forge, gate, nullptr, wallOptions);
                }

                // Gateway above forge
                addWallOption(forge, BWAPI::TilePosition(forge.x - 3, forge.y - 3), &end2Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x - 2, forge.y - 3), &end2Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x - 1, forge.y - 3), &end2Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x, forge.y - 3), &end2Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x + 1, forge.y - 3), &end2Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x + 2, forge.y - 3), &end2Geo, wallOptions);
            }

            // Forge on end2 side
            for (BWAPI::TilePosition forge : end2ForgeOptions)
            {
                // Gateway on end1 side
                for (BWAPI::TilePosition gate : end1GatewayOptions)
                {
                    addWallOption(forge, gate, nullptr, wallOptions);
                }

                // Gateway above forge
                addWallOption(forge, BWAPI::TilePosition(forge.x - 3, forge.y - 3), &end1Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x - 2, forge.y - 3), &end1Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x - 1, forge.y - 3), &end1Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x, forge.y - 3), &end1Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x + 1, forge.y - 3), &end1Geo, wallOptions);
                addWallOption(forge, BWAPI::TilePosition(forge.x + 2, forge.y - 3), &end1Geo, wallOptions);
            }

            // Gateway on end1 side, forge below gateway
            for (BWAPI::TilePosition gateway : end1GatewayOptions)
            {
                addWallOption(BWAPI::TilePosition(gateway.x - 2, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x - 1, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 1, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 2, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 3, gateway.y + 3), gateway, &end2Geo, wallOptions);
            }

            // Gateway on end2 side, forge below gateway
            for (BWAPI::TilePosition gateway : end2GatewayOptions)
            {
                addWallOption(BWAPI::TilePosition(gateway.x - 2, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x - 1, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 1, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 2, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 3, gateway.y + 3), gateway, &end1Geo, wallOptions);
            }

            // Prune invalid options from the vector, we don't need to store them any more
            auto it = wallOptions.begin();
            for (; it != wallOptions.end();)
            {
#if DEBUG_PLACEMENT
                if (it->gapCenter != BWAPI::Positions::Invalid && it->gapSize > maxGapSize)
                {
                    Log::Debug() << "Rejected wall forge " << it->forge << ", gateway " << it->gateway
                                 << ", gapSize " << it->gapSize << " > " << maxGapSize;
                }
#endif
                if (it->gapCenter == BWAPI::Positions::Invalid || it->gapSize > maxGapSize)
                {
                    it = wallOptions.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        BWAPI::TilePosition getPylonPlacementFromPylonOptions(const ForgeGatewayWall &wall, size_t optimalPathLength)
        {
            BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileWidth() * 16,
                                                                                        BWAPI::UnitTypes::Protoss_Forge.tileHeight() * 16);
            BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileWidth() * 16,
                                                                                            BWAPI::UnitTypes::Protoss_Gateway.tileHeight() * 16);
            BWAPI::Position natCenter = natural->getPosition();
            bool natSideOfForgeGatewayLine = sideOfLine(forgeCenter, gatewayCenter, natCenter);

            for (auto &pylonOption : pylonOptions)
            {
                auto tile = pylonOption.pylon;
                if (!UnitUtil::Powers(tile, wall.gateway, BWAPI::UnitTypes::Protoss_Gateway)) continue;
                if (!UnitUtil::Powers(tile, wall.forge, BWAPI::UnitTypes::Protoss_Forge)) continue;

                bool powersCannons = true;
                for (const auto &cannon : wall.cannons)
                {
                    powersCannons = powersCannons && UnitUtil::Powers(tile, cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon);
                }
                if (!powersCannons) continue;

                bool natCannonsBuildable = true;
                for (auto natCannon : pylonOption.cannons)
                {
                    natCannonsBuildable = natCannonsBuildable && buildable(BWAPI::UnitTypes::Protoss_Photon_Cannon, natCannon);
                }
                if (!natCannonsBuildable) continue;

                if (!buildable(BWAPI::UnitTypes::Protoss_Pylon, tile)) continue;

                BWAPI::TilePosition spawn = gatewaySpawnPosition(wall, tile, BWAPI::UnitTypes::Protoss_Pylon);
                if (!spawn.isValid())
                {
                    continue;
                }

                // Ensure there is a valid path through the wall
                if (!hasPathWithBuilding(tile, BWAPI::UnitTypes::Protoss_Pylon.tileSize(), optimalPathLength * 2))
                {
#if DEBUG_PLACEMENT
                    Log::Debug() << "Pylon " << tile << " rejected for not having a valid main path";
#endif
                    continue;
                }

                // Ensure there is a valid path from the gateway spawn position
                if (sideOfLine(forgeCenter, gatewayCenter, center(spawn)) == natSideOfForgeGatewayLine)
                {
                    if (!hasPathWithBuilding(tile, BWAPI::UnitTypes::Protoss_Pylon.tileSize(), 0, spawn))
                    {
#if DEBUG_PLACEMENT
                        Log::Debug() << "Pylon " << tile << " rejected for not having a path from gateway spawn position";
#endif
                        continue;
                    }
                }

                return tile;
            }

            return BWAPI::TilePositions::Invalid;
        }

        BWAPI::TilePosition getPylonPlacement(const ForgeGatewayWall &wall, size_t optimalPathLength)
        {
            BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileWidth() * 16,
                                                                                        BWAPI::UnitTypes::Protoss_Forge.tileHeight() * 16);
            BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileWidth() * 16,
                                                                                            BWAPI::UnitTypes::Protoss_Gateway.tileHeight() * 16);
            BWAPI::Position startTileCenter = BWAPI::Position(pathfindingStartTile) + BWAPI::Position(16, 16);
            BWAPI::Position natCenter = natural->getPosition();
            bool natSideOfForgeGatewayLine = sideOfLine(forgeCenter, gatewayCenter, natCenter);
            bool natSideOfGapLine = sideOfLine(wall.gapEnd1, wall.gapEnd2, natCenter);

            BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
            double distCentroidNat = centroid.getDistance(natCenter);

            double bestPylonDist = 0;
            BWAPI::TilePosition bestPylon = BWAPI::TilePositions::Invalid;

            for (int x = wall.gateway.x - 10; x <= wall.gateway.x + 10; x++)
            {
                for (int y = wall.gateway.y - 10; y <= wall.gateway.y + 10; y++)
                {
                    BWAPI::TilePosition tile(x, y);
                    if (!tile.isValid()) continue;
                    if (!UnitUtil::Powers(tile, wall.gateway, BWAPI::UnitTypes::Protoss_Gateway)) continue;
                    if (!UnitUtil::Powers(tile, wall.forge, BWAPI::UnitTypes::Protoss_Forge)) continue;

                    bool powersCannons = true;
                    for (const auto &cannon : wall.cannons)
                    {
                        powersCannons = powersCannons && UnitUtil::Powers(tile, cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon);
                    }
                    if (!powersCannons) continue;

                    if (!buildable(BWAPI::UnitTypes::Protoss_Pylon, tile)) continue;

                    BWAPI::Position pylonCenter = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(32, 32);
                    BWAPI::Position pylonTopLeftWithBuffer = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(-8, -8);
                    BWAPI::Position pylonBottomRightWithBuffer = BWAPI::Position(BWAPI::TilePosition(x + 2, y + 2)) + BWAPI::Position(8, 8);
                    if ((sideOfLine(forgeCenter, gatewayCenter, pylonTopLeftWithBuffer) != natSideOfForgeGatewayLine
                         || sideOfLine(forgeCenter, gatewayCenter, pylonBottomRightWithBuffer) != natSideOfForgeGatewayLine)
                        && (sideOfLine(wall.gapEnd1, wall.gapEnd2, pylonTopLeftWithBuffer) != natSideOfGapLine
                            || sideOfLine(wall.gapEnd1, wall.gapEnd2, pylonBottomRightWithBuffer) != natSideOfGapLine))
                    {
        #if DEBUG_PLACEMENT
                        Log::Debug() << "Pylon " << tile << " rejected for being on the wrong side of the line";
        #endif
                        continue;
                    }

                    if (pylonCenter.getDistance(natCenter) > distCentroidNat)
                    {
        #if DEBUG_PLACEMENT
                        Log::Debug() << "Pylon " << tile << " rejected for being further away from the natural";
        #endif
                        continue;
                    }

                    BWAPI::TilePosition spawn = gatewaySpawnPosition(wall, tile, BWAPI::UnitTypes::Protoss_Pylon);
                    if (!spawn.isValid())
                    {
                        continue;
                    }

                    double dist = pylonCenter.getDistance(startTileCenter) / pylonCenter.getDistance(natCenter);
                    if (dist > bestPylonDist)
                    {
                        // Ensure there is a valid path through the wall
                        if (!hasPathWithBuilding(tile, BWAPI::UnitTypes::Protoss_Pylon.tileSize(), optimalPathLength * 2))
                        {
        #if DEBUG_PLACEMENT
                            Log::Debug() << "Pylon " << tile << " rejected for not having a valid main path";
        #endif
                            continue;
                        }

                        // Ensure there is a valid path from the gateway spawn position
                        if (sideOfLine(forgeCenter, gatewayCenter, center(spawn)) == natSideOfForgeGatewayLine)
                        {
                            if (!hasPathWithBuilding(tile, BWAPI::UnitTypes::Protoss_Pylon.tileSize(), 0, spawn))
                            {
        #if DEBUG_PLACEMENT
                                Log::Debug() << "Pylon " << tile << " rejected for not having a path from gateway spawn position";
        #endif
                                continue;
                            }
                        }

                        bestPylonDist = dist;
                        bestPylon = tile;
                    }
                }
            }

            return bestPylon;
        }

        ForgeGatewayWallOption getBestWallOption(std::vector<ForgeGatewayWallOption> &wallOptions,
                                                 size_t optimalPathLength,
                                                 BWAPI::TilePosition (*pylonPlacer)(const ForgeGatewayWall &, size_t))
        {
            ForgeGatewayWallOption bestWallOption;

            BWAPI::Position natCenter = natural->getPosition();

            double bestWallQuality = DBL_MAX;
            double bestDistCentroid = 0;
            BWAPI::Position bestCentroid;

            for (auto const &wall : wallOptions)
            {
#if DEBUG_PLACEMENT
                Log::Debug() << "Starting scoring forge=" << wall.forge << ";gateway=" << wall.gateway
                             << ";gapc=" << BWAPI::TilePosition(wall.gapCenter);
#endif

                // Check if there is a pylon location
                addWallTiles(wall.forge, BWAPI::UnitTypes::Protoss_Forge.tileSize());
                addWallTiles(wall.gateway, BWAPI::UnitTypes::Protoss_Gateway.tileSize());

                auto pylon = pylonPlacer(wall.toWall(), optimalPathLength);

                removeWallTiles(wall.forge, BWAPI::UnitTypes::Protoss_Forge.tileSize());
                removeWallTiles(wall.gateway, BWAPI::UnitTypes::Protoss_Gateway.tileSize());

                if (!pylon.isValid()) continue;

                // Center of each building
                BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileSize()) / 2);
                BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileSize()) / 2);

                // Prefer walls that create a longer path
                auto distIncrease = pathLength() - optimalPathLength;
                if (distIncrease < 0) continue; // Shouldn't be possible, but guard for it just in case

                // Prefer walls that are slightly crooked, so we get better cannon placements
                // For walls where the forge and gateway are touching, measure this by comparing the slope of the wall building centers to the slope of the gap, rounded to 15 degree increments
                // Otherwise compute it based on the relative distance between the natural center and the forge and gateway
                int straightness = 0;
                if (areForgeAndGatewayTouching(wall.forge, wall.gateway))
                {
                    straightness = (int)std::floor(angularDistance(forgeCenter, gatewayCenter, wall.gapEnd1, wall.gapEnd2) / (pi / 12));
                }
                else
                {
                    double distForge = natCenter.getDistance(forgeCenter);
                    double distGateway = natCenter.getDistance(gatewayCenter);
                    straightness = (int)std::floor(2.0 * std::max(distForge, distGateway) / std::min(distForge, distGateway));
                }

                // Combine the gap size and the previous two values into a measure of wall quality
                // Distance increase is capped at 10 tiles and scaled to a factor of 0.8 - 1.2
                // Straightness target is 2, above or below cause the final result to increase
                double wallQuality = wall.gapSize
                    * (0.8 + 0.4 * (std::max(0.0, 10.0 - (double)distIncrease) / 10.0))
                    * (1.0 + std::abs(2 - straightness) / 2.0);

                // Compute the centroid of the wall buildings
                // If the other scores are equal, we prefer a centroid farther away from the natural
                // In all cases we require the centroid to be at least 6 tiles away
                BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
                double distCentroidNat = centroid.getDistance(natCenter);
                if (distCentroidNat < 192.0) continue;

#if DEBUG_PLACEMENT
                Log::Debug() << "Considering forge=" << wall.forge << ";gateway=" << wall.gateway << ";gapc=" << BWAPI::TilePosition(wall.gapCenter)
                             << ";gapw=" << wall.gapSize << ";dinc=" << distIncrease << ";straightness=" << straightness << ";qualityFactor="
                             << wallQuality << ";dcentroid=" << distCentroidNat;
#endif

                if (wallQuality < bestWallQuality
                    || (wallQuality == bestWallQuality && distCentroidNat > bestDistCentroid))
                {
                    bestWallOption = wall;
                    bestWallQuality = wallQuality;
                    bestDistCentroid = distCentroidNat;
                    bestCentroid = centroid;
#if DEBUG_PLACEMENT
                    Log::Debug() << "(best)";
#endif
                }
            }

            return bestWallOption;
        }

        bool overlapsProbeBlockingLocation(ForgeGatewayWall &wall, BWAPI::TilePosition tile, BWAPI::UnitType type)
        {
            return std::any_of(wall.probeBlockingPositions.begin(),
                               wall.probeBlockingPositions.end(),
                               [&tile, &type](const auto &probeBlockingPosition)
                               {
                                   return Geo::Overlaps(BWAPI::TilePosition(probeBlockingPosition), 1, 1, tile, type.tileWidth(), type.tileHeight());
                               });
        }

        BWAPI::TilePosition getCannonPlacement(ForgeGatewayWall &wall, size_t optimalPathLength, std::set<BWAPI::TilePosition> &unusedNaturalCannons)
        {
            BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileSize()) / 2);
            BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileSize()) / 2);
            BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
            BWAPI::Position natCenter = natural->getPosition();
            bool natSideOfForgeGatewayLine = sideOfLine(forgeCenter, gatewayCenter, natCenter);

            bool forgeGatewayTouching = areForgeAndGatewayTouching(wall.forge, wall.gateway);

            BWAPI::TilePosition startTile = wall.pylon.isValid() ? wall.pylon : BWAPI::TilePosition(centroid);

            double distBest = 0.0;
            BWAPI::TilePosition tileBest = BWAPI::TilePositions::Invalid;
            for (int x = startTile.x - 10; x <= startTile.x + 10; x++)
            {
                for (int y = startTile.y - 10; y <= startTile.y + 10; y++)
                {
                    BWAPI::TilePosition tile(x, y);

                    // Natural cannons come "pre-validated"
                    bool prevalidated = unusedNaturalCannons.contains(tile);

                    BWAPI::Position cannonCenter = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(32, 32);
                    BWAPI::TilePosition spawn;
                    if (!prevalidated)
                    {
                        if (!tile.isValid()) continue;
                        if (wall.pylon.isValid() && !UnitUtil::Powers(wall.pylon, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;
                        if (!buildable(BWAPI::UnitTypes::Protoss_Photon_Cannon, tile)) continue;
                        if (!overlapsNaturalArea(tile, BWAPI::UnitTypes::Protoss_Pylon)) continue;
                        if (overlapsProbeBlockingLocation(wall, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;

                        if (sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(16, 16)) != natSideOfForgeGatewayLine
                            || sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(16, -16)) != natSideOfForgeGatewayLine
                            || sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(-16, 16)) != natSideOfForgeGatewayLine
                            || sideOfLine(forgeCenter, gatewayCenter, cannonCenter + BWAPI::Position(-16, -16)) != natSideOfForgeGatewayLine)
                        {
    #if DEBUG_PLACEMENT
                            Log::Debug() << "Cannon " << tile << " rejected because on wrong side of line";
    #endif
                            continue;
                        }

                        spawn = gatewaySpawnPosition(wall, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon);
                        if (!spawn.isValid()) continue;
                    }

                    int borderingTiles = 0;
                    if (!walkableTile(BWAPI::TilePosition(x - 1, y))) borderingTiles++;
                    if (!walkableTile(BWAPI::TilePosition(x - 1, y + 1))) borderingTiles++;
                    if (!walkableTile(BWAPI::TilePosition(x, y - 1))) borderingTiles++;
                    if (!walkableTile(BWAPI::TilePosition(x + 1, y - 1))) borderingTiles++;
                    if (!walkableTile(BWAPI::TilePosition(x + 2, y))) borderingTiles++;
                    if (!walkableTile(BWAPI::TilePosition(x + 2, y + 1))) borderingTiles++;
                    if (!walkableTile(BWAPI::TilePosition(x, y + 2))) borderingTiles++;
                    if (!walkableTile(BWAPI::TilePosition(x + 1, y + 2))) borderingTiles++;

                    // When forge and gateway are touching, use the wall centroid as the distance measurement
                    // Otherwise, use the smallest distance to either of the buildings
                    double distToWall = forgeGatewayTouching ? centroid.getDistance(cannonCenter) : std::min(gatewayCenter.getDistance(cannonCenter),
                                                                                                             forgeCenter.getDistance(cannonCenter));

                    // When forge and gateway are touching, prefer locations closer to the door
                    // Otherwise, prefer locations further from the door so we don't put them in the gap
                    double distToDoor = forgeGatewayTouching ? log10(wall.gapCenter.getDistance(cannonCenter)) : 1 / log(wall.gapCenter.getDistance(
                            cannonCenter));

                    // Compute a factor based on how many bordering tiles there are
                    double borderingFactor = std::pow(0.95, borderingTiles);

                    // Putting it all together
                    double dist = 1.0 / (distToWall * distToDoor * borderingFactor);

#if DEBUG_PLACEMENT
                    Log::Debug() << "Considering cannon @ " << tile << ";overall=" << dist << ";walldist=" << distToWall << ";doordistfactor="
                                 << distToDoor << ";borderfactor=" << borderingFactor << ";bordering=" << borderingTiles;
#endif

                    if (dist > distBest)
                    {
                        if (!prevalidated)
                        {
                            // Ensure there is still a valid path through the wall
                            if (!hasPathWithBuilding(tile, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize(), optimalPathLength * 3))
                            {
    #if DEBUG_PLACEMENT
                                Log::Debug() << "(rejected as blocks path)";
    #endif
                                continue;
                            }

                            // Ensure there is a valid path from the gateway spawn position
                            if (sideOfLine(forgeCenter, gatewayCenter, center(spawn)) == natSideOfForgeGatewayLine)
                            {
                                if (!hasPathWithBuilding(tile, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize(), 0, spawn))
                                {
    #if DEBUG_PLACEMENT
                                    Log::Debug() << "(rejected as blocks path from gateway spawn point)";
    #endif
                                    continue;
                                }
                            }
                        }

                        tileBest = tile;
                        distBest = dist;

#if DEBUG_PLACEMENT
                        Log::Debug() << "(best)";
#endif
                    }
                }
            }

            return tileBest;
        }

        ForgeGatewayWall createForgeGatewayWall(bool tight, int maxGapSize)
        {
            wallTiles.clear();

            // Initialize pathfinding
            size_t optimalPathLength = pathLength();
#if DEBUG_PLACEMENT
            Log::Debug() << "Pathfinding between " << pathfindingStartTile << " and " << pathfindingEndTile << ", initial length "
                         << optimalPathLength;
#endif

            // Step 1: Analyze choke geo and find potential forge and gateway options
            std::set<BWAPI::WalkPosition> end1Geo;
            std::set<BWAPI::WalkPosition> end2Geo;

            std::set<BWAPI::TilePosition> end1ForgeOptions;
            std::set<BWAPI::TilePosition> end1GatewayOptions;
            std::set<BWAPI::TilePosition> end2ForgeOptions;
            std::set<BWAPI::TilePosition> end2GatewayOptions;

            analyzeChokeGeoAndFindBuildingOptions(end1Geo,
                                                  end2Geo,
                                                  end1ForgeOptions,
                                                  end1GatewayOptions,
                                                  end2ForgeOptions,
                                                  end2GatewayOptions,
                                                  tight);

            // Step 2: Generate possible combinations
            std::vector<ForgeGatewayWallOption> wallOptions;
            generateWallOptions(wallOptions,
                                end1Geo,
                                end2Geo,
                                end1ForgeOptions,
                                end1GatewayOptions,
                                end2ForgeOptions,
                                end2GatewayOptions,
                                maxGapSize);

            // Return if we have no valid wall
            if (wallOptions.empty())
            {
#if DEBUG_PLACEMENT
                Log::Debug() << "No wall options generated";
#endif
                return {};
            }

#if DEBUG_PLACEMENT
            Log::Debug() << "Generated " << wallOptions.size() << " wall options";
#endif

            auto pylonPlacer = &getPylonPlacementFromPylonOptions;

            // Step 3: Select the best wall and do some calculations we'll need later
            ForgeGatewayWall bestWall = getBestWallOption(wallOptions, optimalPathLength, pylonPlacer).toWall();

            // If there is no valid wall, try again with separate pylons for wall and natural
            if (!bestWall.forge.isValid())
            {
                // Add the natural pylon and cannons to the reserved tiles
                auto naturalDefenseLocations = baseStaticDefenseLocations(natural);
                if (naturalDefenseLocations.isValid())
                {
                    addBuildingToReservedTiles(naturalDefenseLocations.powerPylon, BWAPI::UnitTypes::Protoss_Pylon);
                    for (const auto &cannonTile : naturalDefenseLocations.workerDefenseCannons)
                    {
                        addBuildingToReservedTiles(cannonTile, BWAPI::UnitTypes::Protoss_Photon_Cannon);
                    }
                }

                // Try again with the alternate pylon placer
                pylonPlacer = &getPylonPlacement;
                bestWall = getBestWallOption(wallOptions, optimalPathLength, pylonPlacer).toWall();
            }

            // Abort if there is no valid wall
            if (!bestWall.forge.isValid())
            {
                return {};
            }

            addWallTiles(bestWall.forge, BWAPI::UnitTypes::Protoss_Forge.tileSize());
            addWallTiles(bestWall.gateway, BWAPI::UnitTypes::Protoss_Gateway.tileSize());

            // Step 4: Find initial cannons
            // We do this before finding the pylon so the pylon doesn't interfere too much with optimal cannon placement
            // If we can't place the pylon later, we will roll back cannons until we can
            std::set<BWAPI::TilePosition> unusedNaturalCannons;
            for (int i = 0; i < 2; i++)
            {
                BWAPI::TilePosition cannon = getCannonPlacement(bestWall, optimalPathLength, unusedNaturalCannons);
                if (cannon.isValid())
                {
                    addWallTiles(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize());
                    bestWall.cannons.push_back(cannon);

#if DEBUG_PLACEMENT
                    Log::Debug() << "Added cannon @ " << cannon;
#endif
                }
            }

            // Step 5: Find a pylon position and finalize the wall selection

            BWAPI::TilePosition pylon = pylonPlacer(bestWall, optimalPathLength);
            while (!pylon.isValid())
            {
                // Undo cannon placement until we can place the pylon
                if (bestWall.cannons.empty()) break;

                BWAPI::TilePosition cannon = bestWall.cannons.back();
                bestWall.cannons.pop_back();
                removeWallTiles(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize());

#if DEBUG_PLACEMENT
                Log::Debug() << "Removed cannon @ " << cannon;
#endif

                pylon = pylonPlacer(bestWall, optimalPathLength);
            }

            // Return invalid wall if no pylon location can be found
            if (!pylon.isValid())
            {
#if DEBUG_PLACEMENT
                Log::Get() << "ERROR: Wall generation: Could not find valid pylon, but this should have been checked when picking the best wall";
#endif
                return {};
            }

#if DEBUG_PLACEMENT
            Log::Debug() << "Added pylon @ " << pylon;
#endif
            bestWall.pylon = pylon;
            addWallTiles(pylon, BWAPI::UnitTypes::Protoss_Pylon.tileSize());

            // Add the natural cannons
            for (auto &pylonOption : pylonOptions)
            {
                if (pylonOption.pylon != bestWall.pylon) continue;

                bestWall.naturalCannons.assign(pylonOption.cannons.begin(), pylonOption.cannons.end());
                unusedNaturalCannons.insert(pylonOption.cannons.begin(), pylonOption.cannons.end());

                // Build cannon furthest from wall first
                std::sort(bestWall.naturalCannons.begin(),
                          bestWall.naturalCannons.end(),
                          [&bestWall](const BWAPI::TilePosition &a, const BWAPI::TilePosition &b)
                          {
                              return a.getApproxDistance(bestWall.forge) > a.getApproxDistance(bestWall.forge);
                          });

                for (auto naturalCannon : bestWall.naturalCannons)
                {
                    addWallTiles(naturalCannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize());
                }
                break;
            }

            // Step 6: Analyze the wall geo and remove cannons overlapping probe blocking positions
            analyzeWallGeo(bestWall);
            for (auto it = bestWall.cannons.begin(); it != bestWall.cannons.end(); )
            {
                if (overlapsProbeBlockingLocation(bestWall, *it, BWAPI::UnitTypes::Protoss_Photon_Cannon))
                {
                    it = bestWall.cannons.erase(it);
                }
                else
                {
                    it++;
                }
            }

            // Step 7: Find remaining cannon positions (up to 6 in total)

            // Find location closest to the wall that is behind it
            // Only return powered buildings
            // Prefer a location that is close to the door
            // Prefer a location that doesn't leave space around it
            // Allow using the natural cannons

            for (auto n = bestWall.cannons.size(); n < 6; n++)
            {
                BWAPI::TilePosition cannon = getCannonPlacement(bestWall, optimalPathLength, unusedNaturalCannons);
                if (!cannon.isValid()) break;

                addWallTiles(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize());
                bestWall.cannons.push_back(cannon);
                unusedNaturalCannons.erase(cannon);

#if DEBUG_PLACEMENT
                Log::Debug() << "Added cannon @ " << cannon;
#endif
            }

            return bestWall;
        }
    }

    ForgeGatewayWall createForgeGatewayWall(bool tight, Base *base)
    {
#if DEBUG_PLACEMENT
        Log::Debug() << "Creating wall; tight=" << tight;
#endif

        // Ensure we have the ability to make a wall
        auto chokes = Map::getStartingBaseChokes(base);
        mainChoke = chokes.first;
        naturalChoke = chokes.second;

        if (mainChoke == naturalChoke)
        {
            natural = base;
        }
        else
        {
            natural = Map::mapSpecificOverride()->naturalForWallPlacement(base);
            if (!natural) natural = Map::getStartingBaseNatural(base);
        }

        if (!natural || !mainChoke || !naturalChoke)
        {
            Log::Get() << "Wall cannot be created; missing natural, main choke, or natural choke";
            return {};
        }

#if DEBUG_PLACEMENT
        Log::Debug() << "Main @ " << Map::getMyMain()->getTilePosition();
        Log::Debug() << "Natural @ " << natural->getTilePosition();
        Log::Debug() << "Main choke @ " << BWAPI::TilePosition(mainChoke->center);
        Log::Debug() << "Natural choke @ " << BWAPI::TilePosition(naturalChoke->center);
#endif

        // Map-specific hard-coded walls
        auto mapSpecificWall = Map::mapSpecificOverride()->getWall(base->getTilePosition());
        if (mapSpecificWall)
        {
            if (mapSpecificWall->isValid())
            {
                analyzeWallGeo(*mapSpecificWall);
            }
#if DEBUG_PLACEMENT
            Log::Debug() << "Using map-specific wall: " << (*mapSpecificWall);
#endif
            return *mapSpecificWall;
        }

        // Initialize reserved tiles
        // These are tiles in the natural we don't want to block
        reservedTiles.clear();
        addBuildingToReservedTiles(natural->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus);

        // Initialize pathfinding tiles
        initializePathfindingTiles();

        // Initialize neutrals that can be used in the wall
        initializeNeutrals();

        // Select possible locations for the pylon that are close to the choke and provide natural cannon locations
        generatePylonOptions();

#if DEBUG_PLACEMENT
        Log::Debug() << "Pylon options:";
        for (auto &pylonOption : pylonOptions)
        {
            std::ostringstream out;
            out << pylonOption.score << ": " << "Pylon@" << pylonOption.pylon << ";Cannons@";
            for (auto cannon : pylonOption.cannons)
            {
                out << cannon;
            }
            Log::Debug() << out.str();
        }
#endif

        // Create the wall
#if DEBUG_PLACEMENT
        Log::Debug() << "Creating wall; tight=" << tight;
#endif
        ForgeGatewayWall wall = createForgeGatewayWall(tight, 6);

        // Fall back to non-tight if a tight wall could not be found
        if (!wall.isValid())
        {
#if DEBUG_PLACEMENT
            Log::Debug() << "Could not find wall";
#endif
            return {};
        }

#if DEBUG_PLACEMENT
        Log::Debug() << "Wall: " << wall;
#endif

        return wall;
    }
}