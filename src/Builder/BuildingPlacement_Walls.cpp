#include "BuildingPlacement.h"

#include "Map.h"
#include "PathFinding.h"
#include "Geo.h"
#include "UnitUtil.h"

#include <cfloat>

const double pi = 3.14159265358979323846;

#if INSTRUMENTATION_ENABLED
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

        bool buildableTile(BWAPI::TilePosition tile)
        {
            return tile.isValid() &&
                   Map::isWalkable(tile) &&
                   reservedTiles.find(tile) == reservedTiles.end() &&
                   wallTiles.find(tile) == wallTiles.end();
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
                auto placeEnd = [&](BWAPI::Unit end)
                {
                    int minDist = INT_MAX;
                    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                    for (auto tile : positions)
                    {
                        if (!UnitUtil::Powers(pylon, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;

                        int dist = end->getPosition().getApproxDistance(BWAPI::Position(tile) + BWAPI::Position(16, 16));
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

        bool hasPathWithBuilding(BWAPI::TilePosition tile,
                                 BWAPI::TilePosition size,
                                 int maxPathLength = 0,
                                 BWAPI::TilePosition alternateStartTile = BWAPI::TilePositions::Invalid)
        {
            auto startTile = pathfindingStartTile;
            if (alternateStartTile != BWAPI::TilePositions::Invalid)
            {
                startTile = alternateStartTile;
            }

            auto validPathfindingTile = [](BWAPI::TilePosition tile)
            {
                return buildableTile(tile) && natural->mineralLineTiles.find(tile) == natural->mineralLineTiles.end();
            };

            addWallTiles(tile, size);
            auto path = PathFinding::Search(startTile, pathfindingEndTile, validPathfindingTile);
            removeWallTiles(tile, size);

            if (path.empty()) return false;
            if (maxPathLength > 0 && (int)path.size() > maxPathLength) return false;

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

        bool walkableAbove(BWAPI::TilePosition tile)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) - BWAPI::WalkPosition(0, 1);
            for (int x = 0; x < 4; x++)
            {
                if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x + x, start.y))) return false;
            }

            return true;
        }

        bool walkableBelow(BWAPI::TilePosition tile)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(0, 4);
            for (int x = 0; x < 4; x++)
            {
                if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x + x, start.y))) return false;
            }

            return true;
        }

        bool walkableLeft(BWAPI::TilePosition tile)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) - BWAPI::WalkPosition(1, 0);
            for (int y = 0; y < 4; y++)
            {
                if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x, start.y + y))) return false;
            }

            return true;
        }

        bool walkableRight(BWAPI::TilePosition tile)
        {
            if (!tile.isValid()) return false;

            BWAPI::WalkPosition start = BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(4, 0);
            for (int y = 0; y < 4; y++)
            {
                if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(start.x, start.y + y))) return false;
            }

            return true;
        }

        void addBuildingOption(int x,
                               int y,
                               BWAPI::UnitType building,
                               std::set<BWAPI::TilePosition> &buildingOptions,
                               bool tight,
                               std::set<BWAPI::TilePosition> &geyserTiles)
        {
            // Collect the possible build locations covering this tile
            std::set<BWAPI::TilePosition> tiles;

            bool geyserBlockTop = geyserTiles.find(BWAPI::TilePosition(x, y - 1)) != geyserTiles.end()
                                  || geyserTiles.find(BWAPI::TilePosition(x - 1, y - 1)) != geyserTiles.end()
                                  || geyserTiles.find(BWAPI::TilePosition(x + 1, y - 1)) != geyserTiles.end();
            bool geyserBlockLeft = geyserTiles.find(BWAPI::TilePosition(x - 1, y)) != geyserTiles.end()
                                   || geyserTiles.find(BWAPI::TilePosition(x - 1, y - 1)) != geyserTiles.end()
                                   || geyserTiles.find(BWAPI::TilePosition(x - 1, y + 1)) != geyserTiles.end();
            bool geyserBlockBottom = geyserTiles.find(BWAPI::TilePosition(x, y + 1)) != geyserTiles.end()
                                     || geyserTiles.find(BWAPI::TilePosition(x - 1, y + 1)) != geyserTiles.end()
                                     || geyserTiles.find(BWAPI::TilePosition(x + 1, y + 1)) != geyserTiles.end();
            bool geyserBlockRight = geyserTiles.find(BWAPI::TilePosition(x + 1, y)) != geyserTiles.end()
                                    || geyserTiles.find(BWAPI::TilePosition(x + 1, y - 1)) != geyserTiles.end()
                                    || geyserTiles.find(BWAPI::TilePosition(x + 1, y + 1)) != geyserTiles.end();

            // Blocked on top
            if (geyserBlockTop || (building == BWAPI::UnitTypes::Protoss_Forge && !walkableAbove(BWAPI::TilePosition(x, y)))
                || (!tight && !Map::isTerrainWalkable(x, y - 1)))
            {
                for (int i = 0; i < building.tileWidth(); i++)
                {
                    tiles.insert(BWAPI::TilePosition(x - i, y));
                }
            }

            // Blocked on left
            if (geyserBlockLeft || (building == BWAPI::UnitTypes::Protoss_Forge && !walkableLeft(BWAPI::TilePosition(x, y)))
                || (!tight && !Map::isTerrainWalkable(x - 1, y)))
            {
                for (int i = 0; i < building.tileHeight(); i++)
                {
                    tiles.insert(BWAPI::TilePosition(x, y - i));
                }
            }

            // Blocked on bottom
            if (geyserBlockBottom || (building == BWAPI::UnitTypes::Protoss_Gateway && !walkableBelow(BWAPI::TilePosition(x, y)))
                || (!tight && !Map::isTerrainWalkable(x, y + 1)))
            {
                int thisY = y - building.tileHeight() + 1;
                for (int i = 0; i < building.tileWidth(); i++)
                {
                    tiles.insert(BWAPI::TilePosition(x - i, thisY));
                }
            }

            // Blocked on right
            if (geyserBlockRight || (building == BWAPI::UnitTypes::Protoss_Gateway && !walkableRight(BWAPI::TilePosition(x, y)))
                || (!tight && !Map::isTerrainWalkable(x + 1, y)))
            {
                int thisX = x - building.tileWidth() + 1;
                for (int i = 0; i < building.tileHeight(); i++)
                {
                    tiles.insert(BWAPI::TilePosition(thisX, y - i));
                }
            }

            // Add all valid positions to the options set
            for (BWAPI::TilePosition tile : tiles)
            {
                if (!tile.isValid()) continue;
                if (!buildable(building, tile)) continue;

                auto result = buildingOptions.insert(tile);
#if DEBUG_PLACEMENT
                if (result.second) Log::Debug() << building << " option at " << tile;
#endif
            }
        }

        void addForgeGeo(BWAPI::TilePosition forge, std::vector<BWAPI::Position> &geo)
        {
            geo.push_back(center(forge));
            geo.push_back(center(BWAPI::TilePosition(forge.x + 1, forge.y)));
            geo.push_back(center(BWAPI::TilePosition(forge.x + 2, forge.y)));
            geo.push_back(center(BWAPI::TilePosition(forge.x + 2, forge.y + 1)));
            geo.push_back(center(BWAPI::TilePosition(forge.x + 1, forge.y + 1)));
            geo.push_back(center(BWAPI::TilePosition(forge.x, forge.y + 1)));
        }

        void addGatewayGeo(BWAPI::TilePosition gateway, std::vector<BWAPI::Position> &geo)
        {
            geo.push_back(center(gateway));
            geo.push_back(center(BWAPI::TilePosition(gateway.x + 1, gateway.y)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x + 2, gateway.y)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x + 3, gateway.y)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x + 3, gateway.y + 1)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x + 3, gateway.y + 2)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x + 2, gateway.y + 2)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x + 1, gateway.y + 2)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x, gateway.y + 2)));
            geo.push_back(center(BWAPI::TilePosition(gateway.x, gateway.y + 1)));
        }

        void addWallOption(BWAPI::TilePosition forge,
                           BWAPI::TilePosition gateway,
                           std::vector<BWAPI::Position> *geo,
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
            if (forge.x > (gateway.x - 3) && forge.x<(gateway.x + 4) && forge.y>(gateway.y - 2) && forge.y < (gateway.y + 3))
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
            std::vector<BWAPI::Position> geo1;
            std::vector<BWAPI::Position> *geo2;

            if (geo)
            {
                addForgeGeo(forge, geo1);
                addGatewayGeo(gateway, geo1);
                geo2 = geo;
            }
            else
            {
                addForgeGeo(forge, geo1);
                geo2 = new std::vector<BWAPI::Position>();
                addGatewayGeo(gateway, *geo2);
            }

            BWAPI::Position natCenter = natural->getPosition();
            double bestDist = DBL_MAX;
            double bestNatDist = DBL_MAX;
            BWAPI::Position bestCenter = BWAPI::Positions::Invalid;

            BWAPI::Position end1;
            BWAPI::Position end2;

            for (BWAPI::Position first : geo1)
            {
                for (BWAPI::Position second : *geo2)
                {
                    double dist = first.getDistance(second);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestCenter = BWAPI::Position((first.x + second.x) / 2, (first.y + second.y) / 2);
                        bestNatDist = bestCenter.getDistance(natCenter);
                        end1 = first;
                        end2 = second;
                    }
                    else if (dist == bestDist)
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

        bool isAnyWalkable(const BWAPI::TilePosition here)
        {
            const auto start = BWAPI::WalkPosition(here);
            for (auto x = start.x; x < start.x + 4; x++)
            {
                for (auto y = start.y; y < start.y + 4; y++)
                {
                    if (!BWAPI::WalkPosition(x, y).isValid()) return false;
                    if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(x, y))) return true;
                }
            }
            return false;
        }

        void analyzeWallGeo(ForgeGatewayWall &wall)
        {
            BWAPI::Position natCenter = natural->getPosition();

            BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileWidth() * 16,
                                                                                        BWAPI::UnitTypes::Protoss_Forge.tileHeight() * 16);
            BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileWidth() * 16,
                                                                                            BWAPI::UnitTypes::Protoss_Gateway.tileHeight() * 16);
            BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
            bool natSideOfForgeGatewayLine = sideOfLine(forgeCenter, gatewayCenter, natCenter);
            bool natSideOfGapLine = sideOfLine(wall.gapEnd1, wall.gapEnd2, natCenter);

            // Use the bounding box defined by the gap center, natural center, and wall centroid to define the search area
            BWAPI::TilePosition topLeft = BWAPI::TilePosition(BWAPI::Position(std::min(natCenter.x, std::min(centroid.x, wall.gapCenter.x)),
                                                                              std::min(natCenter.y, std::min(centroid.y, wall.gapCenter.y))));
            BWAPI::TilePosition bottomRight = BWAPI::TilePosition(BWAPI::Position(std::max(natCenter.x, std::max(centroid.x, wall.gapCenter.x)),
                                                                                  std::max(natCenter.y, std::max(centroid.y, wall.gapCenter.y))));

            // Add all the valid tiles we can find inside and outside the wall
            for (int x = topLeft.x - 10; x < bottomRight.x + 10; x++)
            {
                for (int y = topLeft.y - 10; y < bottomRight.y + 10; y++)
                {
                    BWAPI::TilePosition tile = BWAPI::TilePosition(x, y);
                    if (!isAnyWalkable(tile)) continue;

                    BWAPI::Position tileCenter = center(tile);
                    if (sideOfLine(forgeCenter, gatewayCenter, tileCenter) != natSideOfForgeGatewayLine
                        && sideOfLine(wall.gapEnd1, wall.gapEnd2, tileCenter) != natSideOfGapLine)
                    {
                        wall.tilesOutsideWall.insert(tile);
                    }
                    else
                    {
                        wall.tilesInsideWall.insert(tile);
                    }
                }
            }

            // Move tiles we flagged inside the wall to outside if they were not in the natural area
            auto &bwemMap = BWEM::Map::Instance();
            auto naturalArea = natural->getArea();
            for (auto it = wall.tilesInsideWall.begin(); it != wall.tilesInsideWall.end();)
            {
                if (bwemMap.GetArea(*it) != naturalArea)
                {
                    wall.tilesOutsideWall.insert(*it);
                    it = wall.tilesInsideWall.erase(it);
                }
                else
                {
                    it++;
                }
            }

            // For tiles outside the wall, prune tiles more than 4 tiles away from the wall
            // Move tiles less than 1.5 tiles away from the wall to a separate set of tiles close to the wall
            for (auto it = wall.tilesOutsideWall.begin(); it != wall.tilesOutsideWall.end();)
            {
                double bestDist = DBL_MAX;
                BWAPI::Position tileCenter = center(*it);

                for (auto const &tile : wall.tilesInsideWall)
                {
                    double dist = center(tile).getDistance(tileCenter);
                    if (dist < bestDist) bestDist = dist;
                }

                if (bestDist < 48)
                {
                    wall.tilesOutsideButCloseToWall.insert(*it);
                }

                if (bestDist > 128)
                {
                    it = wall.tilesOutsideWall.erase(it);
                }
                else
                {
                    it++;
                }
            }
        }

        BWAPI::TilePosition gatewaySpawnPosition(ForgeGatewayWall &wall, BWAPI::TilePosition buildingTile, BWAPI::UnitType buildingType)
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
                if (wallTiles.find(tile) != wallTiles.end()) continue;

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
                std::vector<BWAPI::Position> &end1Geo,
                std::vector<BWAPI::Position> &end2Geo,
                std::set<BWAPI::TilePosition> &end1ForgeOptions,
                std::set<BWAPI::TilePosition> &end1GatewayOptions,
                std::set<BWAPI::TilePosition> &end2ForgeOptions,
                std::set<BWAPI::TilePosition> &end2GatewayOptions,
                bool tight)
        {
            // Geysers are always tight, so treat them specially
            std::set<BWAPI::TilePosition> geyserTiles;
            for (auto geyser : natural->geyserLocations())
            {
                for (int x = geyser.x; x < geyser.x + BWAPI::UnitTypes::Resource_Vespene_Geyser.tileWidth(); x++)
                {
                    for (int y = geyser.y; y < geyser.y + BWAPI::UnitTypes::Resource_Vespene_Geyser.tileHeight(); y++)
                    {
                        geyserTiles.insert(BWAPI::TilePosition(x, y));
                    }
                }
            }

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
                        if (!Map::isWalkable(tile)) end1Geo.push_back(center(tile));
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight, geyserTiles);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight, geyserTiles);
                    }
                }

                // Find options on right side
                for (int x = end2.x - 2; x <= end2.x + 2; x++)
                {
                    for (int y = end2.y - 5; y <= end2.y + 5; y++)
                    {
                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        if (!Map::isWalkable(tile)) end2Geo.push_back(center(tile));
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight, geyserTiles);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight, geyserTiles);
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
                        if (!Map::isWalkable(tile)) end1Geo.push_back(center(tile));
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight, geyserTiles);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight, geyserTiles);
                    }
                }

                // Find options on bottom side
                for (int x = end2.x - 5; x <= end2.x + 5; x++)
                {
                    for (int y = end2.y - 2; y <= end2.y + 2; y++)
                    {
                        BWAPI::TilePosition tile(x, y);
                        if (!tile.isValid()) continue;
                        if (!Map::isWalkable(tile)) end2Geo.push_back(center(tile));
                        if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight, geyserTiles);
                        addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight, geyserTiles);
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
                            if (center(tile).getDistance(end1Center) > center(tile).getDistance(end2Center)) continue;
                            if (!Map::isWalkable(tile)) end1Geo.push_back(center(tile));
                            if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                            addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end1ForgeOptions, tight, geyserTiles);
                            addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end1GatewayOptions, tight, geyserTiles);
                        }

                        // Find options on right side
                        {
                            int x = end2.x + xdelta;
                            int y = end2.y + (int)std::round(xdelta * m) + ydelta;

                            BWAPI::TilePosition tile(x, y);
                            if (!tile.isValid()) continue;
                            if (center(tile).getDistance(end2Center) > center(tile).getDistance(end1Center)) continue;
                            if (!Map::isWalkable(tile)) end2Geo.push_back(center(tile));
                            if (BWAPI::Broodwar->getGroundHeight(tile) != elevation) continue;

                            addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Forge, end2ForgeOptions, tight, geyserTiles);
                            addBuildingOption(x, y, BWAPI::UnitTypes::Protoss_Gateway, end2GatewayOptions, tight, geyserTiles);
                        }
                    }
                }
            }
        }

        void generateWallOptions(
                std::vector<ForgeGatewayWallOption> &wallOptions,
                std::vector<BWAPI::Position> &end1Geo,
                std::vector<BWAPI::Position> &end2Geo,
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
            }

            // Gateway on end1 side, forge below gateway
            for (BWAPI::TilePosition gateway : end1GatewayOptions)
            {
                addWallOption(BWAPI::TilePosition(gateway.x - 2, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x - 1, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 1, gateway.y + 3), gateway, &end2Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 2, gateway.y + 3), gateway, &end2Geo, wallOptions);
            }

            // Gateway on end2 side, forge below gateway
            for (BWAPI::TilePosition gateway : end2GatewayOptions)
            {
                addWallOption(BWAPI::TilePosition(gateway.x - 2, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x - 1, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 1, gateway.y + 3), gateway, &end1Geo, wallOptions);
                addWallOption(BWAPI::TilePosition(gateway.x + 2, gateway.y + 3), gateway, &end1Geo, wallOptions);
            }

            // Prune invalid options from the vector, we don't need to store them any more
            auto it = wallOptions.begin();
            for (; it != wallOptions.end();)
            {
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

        BWAPI::TilePosition getPylonPlacement(ForgeGatewayWall wall, int optimalPathLength)
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

        ForgeGatewayWallOption getBestWallOption(std::vector<ForgeGatewayWallOption> &wallOptions, int optimalPathLength)
        {
            auto &bwemMap = BWEM::Map::Instance();
            ForgeGatewayWallOption bestWallOption;

            BWAPI::Position mapCenter = bwemMap.Center();
            BWAPI::Position startTileCenter = BWAPI::Position(pathfindingStartTile) + BWAPI::Position(16, 16);
            BWAPI::Position natCenter = natural->getPosition();

            double bestWallQuality = DBL_MAX;
            double bestDistCentroid = 0;
            BWAPI::Position bestCentroid;

            for (auto const &wall : wallOptions)
            {
                // Check if there is a pylon location that powers both buildings
                bool powered = false;
                for (auto &pylonOption : pylonOptions)
                {
                    if (UnitUtil::Powers(pylonOption.pylon, wall.forge, BWAPI::UnitTypes::Protoss_Forge) &&
                        UnitUtil::Powers(pylonOption.pylon, wall.gateway, BWAPI::UnitTypes::Protoss_Gateway))
                    {
                        powered = true;
                        break;
                    }
                }
                if (!powered) continue;

                // Center of each building
                BWAPI::Position forgeCenter = BWAPI::Position(wall.forge) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Forge.tileSize()) / 2);
                BWAPI::Position gatewayCenter = BWAPI::Position(wall.gateway) + (BWAPI::Position(BWAPI::UnitTypes::Protoss_Gateway.tileSize()) / 2);

                // Prefer walls where the door is closer to our start tile
                // Includes a small factor of the distance to the map center to encourage shorter paths from the door to the rest of the map
                // Rounded to half-tile resolution
                int distChoke = (int)std::floor((wall.gapCenter.getDistance(startTileCenter) + log(wall.gapCenter.getDistance(mapCenter))) / 16.0);

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
                double wallQuality = wall.gapSize * distChoke * (1.0 + std::abs(2 - straightness) / 2.0);

                // Compute the centroid of the wall buildings
                // If the other scores are equal, we prefer a centroid farther away from the natural
                // In all cases we require the centroid to be at least 6 tiles away
                BWAPI::Position centroid = (forgeCenter + gatewayCenter) / 2;
                double distCentroidNat = centroid.getDistance(natCenter);
                if (distCentroidNat < 192.0) continue;

#if DEBUG_PLACEMENT
                Log::Debug() << "Considering forge=" << wall.forge << ";gateway=" << wall.gateway << ";gapc=" << BWAPI::TilePosition(wall.gapCenter)
                             << ";gapw=" << wall.gapSize << ";dchoke=" << distChoke << ";straightness=" << straightness << ";qualityFactor="
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

        BWAPI::TilePosition getCannonPlacement(ForgeGatewayWall &wall, int optimalPathLength)
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
                    if (!tile.isValid()) continue;
                    if (wall.pylon.isValid() && !UnitUtil::Powers(wall.pylon, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon)) continue;
                    if (!buildable(BWAPI::UnitTypes::Protoss_Photon_Cannon, tile)) continue;
                    if (!overlapsNaturalArea(tile, BWAPI::UnitTypes::Protoss_Pylon)) continue;

                    BWAPI::Position cannonCenter = BWAPI::Position(BWAPI::TilePosition(x, y)) + BWAPI::Position(32, 32);
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

                    BWAPI::TilePosition spawn = gatewaySpawnPosition(wall, tile, BWAPI::UnitTypes::Protoss_Photon_Cannon);
                    if (!spawn.isValid()) continue;

                    int borderingTiles = 0;
                    if (!buildableTile(BWAPI::TilePosition(x - 1, y))) borderingTiles++;
                    if (!buildableTile(BWAPI::TilePosition(x - 1, y + 1))) borderingTiles++;
                    if (!buildableTile(BWAPI::TilePosition(x, y - 1))) borderingTiles++;
                    if (!buildableTile(BWAPI::TilePosition(x + 1, y - 1))) borderingTiles++;
                    if (!buildableTile(BWAPI::TilePosition(x + 2, y))) borderingTiles++;
                    if (!buildableTile(BWAPI::TilePosition(x + 2, y + 1))) borderingTiles++;
                    if (!buildableTile(BWAPI::TilePosition(x, y + 2))) borderingTiles++;
                    if (!buildableTile(BWAPI::TilePosition(x + 1, y + 2))) borderingTiles++;

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
            int optimalPathLength = PathFinding::Search(pathfindingStartTile, pathfindingEndTile).size();
#if DEBUG_PLACEMENT
            Log::Debug() << "Pathfinding between " << pathfindingStartTile << " and " << pathfindingEndTile << ", initial length "
                         << optimalPathLength;
#endif

            // Step 1: Analyze choke geo and find potential forge and gateway options
            std::vector<BWAPI::Position> end1Geo;
            std::vector<BWAPI::Position> end2Geo;

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
            if (wallOptions.empty()) return {};

            // Step 3: Select the best wall and do some calculations we'll need later
            ForgeGatewayWall bestWall = getBestWallOption(wallOptions, optimalPathLength).toWall();

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
            for (int i = 0; i < 2; i++)
            {
                BWAPI::TilePosition cannon = getCannonPlacement(bestWall, optimalPathLength);
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

            BWAPI::TilePosition pylon = getPylonPlacement(bestWall, optimalPathLength);
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

                pylon = getPylonPlacement(bestWall, optimalPathLength);
            }

            // Return invalid wall if no pylon location can be found
            if (!pylon.isValid())
            {
#if DEBUG_PLACEMENT
                Log::Debug() << "ERROR: Could not find valid pylon, but this should have been checked when picking the best wall";
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
            }

            // Analyze the wall geo now since we know this is the final wall
            analyzeWallGeo(bestWall);

            // Step 6: Find remaining cannon positions (up to 6)

            // Find location closest to the wall that is behind it
            // Only return powered buildings
            // Prefer a location that is close to the door
            // Prefer a location that doesn't leave space around it
            for (int n = bestWall.cannons.size(); n < 6; n++)
            {
                BWAPI::TilePosition cannon = getCannonPlacement(bestWall, optimalPathLength);
                if (!cannon.isValid()) break;

                hasPathWithBuilding(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize(), optimalPathLength * 3);

                addWallTiles(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.tileSize());
                bestWall.cannons.push_back(cannon);

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
        natural = Map::getStartingBaseNatural(base);
        auto chokes = Map::getStartingBaseChokes(base);
        mainChoke = chokes.first;
        naturalChoke = chokes.second;
        if (!natural || !mainChoke || !naturalChoke)
        {
            Log::Get() << "Wall cannot be created; missing natural, main choke, or natural choke";
            return {};
        }

        // Map-specific hard-coded walls
        auto mapSpecificWall = Map::mapSpecificOverride()->getWall(base->getTilePosition());
        if (mapSpecificWall.isValid())
        {
            analyzeWallGeo(mapSpecificWall);
            return mapSpecificWall;
        }

        // Select possible locations for the pylon that are close to the choke and provide natural cannon locations
        generatePylonOptions();
        if (pylonOptions.empty())
        {
            Log::Get() << "Wall cannot be created; no pylon options";
            return {};
        }
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

        // Initialize reserved tiles
        // These are tiles in the natural we don't want to block
        reservedTiles.clear();
        auto addBuildingToReservedTiles = [](BWAPI::TilePosition tile, BWAPI::UnitType type)
        {
            for (int x = tile.x; x < tile.x + type.tileWidth(); x++)
            {
                for (int y = tile.y; y < tile.y + type.tileHeight(); y++)
                {
                    reservedTiles.insert(BWAPI::TilePosition(x, y));
                }
            }
        };
        addBuildingToReservedTiles(natural->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus);

        // Initialize pathfinding tiles
        initializePathfindingTiles();

        // Create the wall
#if DEBUG_PLACEMENT
        Log::Debug() << "Creating wall; tight=" << tight;
#endif
        ForgeGatewayWall wall = createForgeGatewayWall(tight, INT_MAX);

        // Fall back to non-tight if a tight wall could not be found
        if (tight && !wall.isValid())
        {
#if DEBUG_PLACEMENT
            Log::Debug() << "Tight wall invalid, trying with loose";
#endif

            wall = createForgeGatewayWall(false, INT_MAX);
        }

            // If a tight wall has a large gap, check if a loose wall is better
        else if (tight && wall.gapSize >= 5)
        {
#if DEBUG_PLACEMENT
            Log::Debug() << "Tight wall has large gap, trying with loose";
#endif

            // Generate the loose wall, with the constraint of the gap being at least 1.5 tiles smaller
            ForgeGatewayWall looseWall = createForgeGatewayWall(false, wall.gapSize - 3);
            if (looseWall.isValid())
            {
#if DEBUG_PLACEMENT
                Log::Debug() << "Using loose wall";
#endif

                wall = looseWall;
            }

                // The loose wall wasn't better, so reset the overlap
            else
            {
#if DEBUG_PLACEMENT
                Log::Debug() << "Using tight wall";
#endif
            }
        }

#if DEBUG_PLACEMENT
        if (wall.isValid())
        {
            Log::Debug() << "Wall: " << wall;
        }
        else
        {
            Log::Debug() << "Could not find wall";
        }
#endif

        return wall;
    }
}