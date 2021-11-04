#include "BWEB.h"

using namespace std;
using namespace BWAPI;

namespace BWEB {

    void Block::draw()
    {
        int color = Broodwar->self()->getColor();
        int textColor = color == 185 ? textColor = Text::DarkGreen : Broodwar->self()->getTextColor();

        // Draw boxes around each feature
        for (auto &tile : smallTiles) {
            Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(65, 65), color);
            Broodwar->drawTextMap(Position(tile) + Position(52, 52), "%cB", textColor);
        }
        for (auto &tile : mediumTiles) {
            Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(97, 65), color);
            Broodwar->drawTextMap(Position(tile) + Position(84, 52), "%cB", textColor);
        }
        for (auto &tile : largeTiles) {
            Broodwar->drawBoxMap(Position(tile), Position(tile) + Position(129, 97), color);
            Broodwar->drawTextMap(Position(tile) + Position(116, 84), "%cB", textColor);
        }

        Broodwar->drawTextMap(Position(tile), "%d, %d", w, h);
    }
}

namespace BWEB::Blocks
{
    namespace {
        vector<Block> allBlocks;
        int blockGrid[256][256];
        int testGrid[256][256];

        void addToBlockGrid(TilePosition start, TilePosition end)
        {
            for (int x = start.x; x < end.x; x++) {
                for (int y = start.y; y < end.y; y++)
                    blockGrid[x][y] = 1;
            }
        }

        struct PieceCount {
            map<Piece, int> pieces;
            int count(Piece p) { return pieces[p]; }
        };
        map<const BWEM::Area *, PieceCount> piecePerArea;

        int countPieces(vector<Piece> pieces, Piece type)
        {
            auto count = 0;
            for (auto &piece : pieces) {
                if (piece == type)
                    count++;
            }
            return count;
        }

        vector<TilePosition> wherePieces(TilePosition tile, vector<Piece> pieces) {
            vector<TilePosition> pieceLayout;
            auto rowHeight = 0;
            auto rowWidth = 0;
            int w = 0, h = 0;
            auto here = tile;
            for (auto &p : pieces) {
                if (p == Piece::Small) {
                    pieceLayout.push_back(here);
                    here += TilePosition(2, 0);
                    rowWidth += 2;
                    rowHeight = std::max(rowHeight, 2);
                }
                if (p == Piece::Medium) {
                    pieceLayout.push_back(here);
                    here += TilePosition(3, 0);
                    rowWidth += 3;
                    rowHeight = std::max(rowHeight, 2);
                }
                if (p == Piece::Large) {
                    pieceLayout.push_back(here);
                    here += TilePosition(4, 0);
                    rowWidth += 4;
                    rowHeight = std::max(rowHeight, 3);
                }
                if (p == Piece::Addon) {
                    pieceLayout.push_back(here + TilePosition(0, 1));
                    here += TilePosition(2, 0);
                    rowWidth += 2;
                    rowHeight = std::max(rowHeight, 2);
                }
                if (p == Piece::Row) {
                    w = std::max(w, rowWidth);
                    h += rowHeight;
                    rowWidth = 0;
                    rowHeight = 0;
                    here = tile + TilePosition(0, h);
                }
                if (p == Piece::Space) {
                    here += TilePosition(1, 0);
                    rowWidth += 1;
                    rowHeight = std::max(rowHeight, 2);
                }
            }
            return pieceLayout;
        }

        vector<Piece> whichPieces(int width, int height, bool faceUp = false, bool faceLeft = false)
        {
            vector<Piece> pieces;

            // Zerg Block pieces
            if (Broodwar->self()->getRace() == Races::Zerg) {
                if (height == 2) {
                    if (width == 2)
                        pieces ={ Piece::Small };
                    if (width == 3)
                        pieces ={ Piece::Medium };
                    if (width == 5)
                        pieces ={ Piece::Small, Piece::Medium };
                }
                else if (height == 3) {
                    if (width == 4)
                        pieces ={ Piece::Large };
                }
                else if (height == 4) {
                    if (width == 3)
                        pieces ={ Piece::Medium, Piece::Row, Piece::Medium };
                    if (width == 5)
                        pieces ={ Piece::Small, Piece::Medium, Piece::Row, Piece::Small, Piece::Medium };
                }
                else if (height == 5) {
                    if (width == 8)
                        pieces ={ Piece::Medium, Piece::Medium, Piece::Small, Piece::Row, Piece::Large, Piece::Large };
                }
                else if (height == 6) {
                    if (width == 5)
                        pieces ={ Piece::Small, Piece::Medium, Piece::Row, Piece::Medium, Piece::Small, Piece::Row, Piece::Small, Piece::Medium };
                    if (width == 6) {
                        faceLeft ?
                            pieces ={ Piece::Small, Piece::Medium, Piece::Row, Piece::Medium, Piece::Medium, Piece::Row, Piece::Space, Piece::Medium, Piece::Small } :
                            pieces ={ Piece::Space, Piece::Medium, Piece::Small, Piece::Row, Piece::Medium, Piece::Medium, Piece::Row, Piece::Small, Piece::Medium };
                    }
                }
            }

            // Protoss Block pieces
            if (Broodwar->self()->getRace() == Races::Protoss) {
                if (height == 2) {
                    if (width == 5)
                        pieces ={ Piece::Small, Piece::Medium };
                }
                else if (height == 4) {
                    if (width == 5)
                        pieces ={ Piece::Small, Piece::Medium, Piece::Row, Piece::Small, Piece::Medium };
                }
                else if (height == 5) {
                    if (width == 4)
                        pieces ={ Piece::Large, Piece::Row, Piece::Small, Piece::Small };
                    if (width == 8) {
                        if (faceLeft) {
                            if (faceUp)
                                pieces ={ Piece::Large, Piece::Large, Piece::Row, Piece::Medium, Piece::Medium, Piece::Small };
                            else
                                pieces ={ Piece::Medium, Piece::Medium, Piece::Small, Piece::Row, Piece::Large, Piece::Large };
                        }
                        else {
                            if (faceUp)
                                pieces ={ Piece::Large, Piece::Large, Piece::Row, Piece::Small, Piece::Medium, Piece::Medium };
                            else
                                pieces ={ Piece::Small, Piece::Medium, Piece::Medium, Piece::Row, Piece::Large, Piece::Large };
                        }
                    }
                    if (width == 16) {
                        if (faceLeft) {
                            if (faceUp)
                                pieces ={ Piece::Large, Piece::Large, Piece::Large, Piece::Large, Piece::Row, Piece::Medium, Piece::Medium, Piece::Small, Piece::Medium, Piece::Medium, Piece::Small };
                            else
                                pieces ={ Piece::Medium, Piece::Medium, Piece::Small, Piece::Medium, Piece::Medium, Piece::Small, Piece::Row, Piece::Large, Piece::Large, Piece::Large, Piece::Large };
                        }
                        else {
                            if (faceUp)
                                pieces ={ Piece::Large, Piece::Large, Piece::Large, Piece::Large, Piece::Row, Piece::Small, Piece::Medium, Piece::Medium, Piece::Small, Piece::Medium, Piece::Medium };
                            else
                                pieces ={ Piece::Small, Piece::Medium, Piece::Medium, Piece::Small, Piece::Medium, Piece::Medium, Piece::Row, Piece::Large, Piece::Large, Piece::Large, Piece::Large };
                        }
                    }
                }
                else if (height == 6) {
                    if (width == 10)
                        pieces ={ Piece::Large, Piece::Addon, Piece::Large, Piece::Row, Piece::Large, Piece::Small, Piece::Large };
                    if (width == 18)
                        pieces ={ Piece::Large, Piece::Large, Piece::Addon, Piece::Large, Piece::Large, Piece::Row, Piece::Large, Piece::Large, Piece::Small, Piece::Large, Piece::Large };
                }
                else if (height == 8) {
                    if (width == 5)
                        pieces ={ Piece::Large, Piece::Row, Piece::Small, Piece::Medium, Piece::Row, Piece::Large };
                    if (width == 4)
                        pieces ={ Piece::Large, Piece::Row, Piece::Small, Piece::Small, Piece::Row, Piece::Large };
                    if (width == 8)
                        pieces ={ Piece::Large, Piece::Large, Piece::Row, Piece::Space, Piece::Space, Piece::Small, Piece::Small, Piece::Space, Piece::Space, Piece::Row, Piece::Large, Piece::Large };
                }
            }

            // Terran Block pieces
            if (Broodwar->self()->getRace() == Races::Terran) {
                if (height == 2) {
                    if (width == 3)
                        pieces ={ Piece::Medium };
                    if (width == 6)
                        pieces ={ Piece::Medium, Piece::Medium };
                }
                else if (height == 4) {
                    if (width == 3)
                        pieces ={ Piece::Medium, Piece::Row, Piece::Medium };
                }
                else if (height == 6) {
                    if (width == 3)
                        pieces ={ Piece::Medium, Piece::Row, Piece::Medium, Piece::Row, Piece::Medium };
                }
                else if (height == 3) {
                    if (width == 6)
                        pieces ={ Piece::Large, Piece::Addon };
                }
                else if (height == 4) {
                    if (width == 6)
                        pieces ={ Piece::Medium, Piece::Medium, Piece::Row, Piece::Medium, Piece::Medium };
                    if (width == 9)
                        pieces ={ Piece::Medium, Piece::Medium, Piece::Medium, Piece::Row, Piece::Medium, Piece::Medium, Piece::Medium };
                }
                else if (height == 5) {
                    if (width == 6)
                        pieces ={ Piece::Large, Piece::Addon, Piece::Row, Piece::Medium, Piece::Medium };
                }
                else if (height == 6) {
                    if (width == 6)
                        pieces ={ Piece::Large, Piece::Addon, Piece::Row, Piece::Large, Piece::Addon };
                }
            }
            return pieces;
        }

        multimap<TilePosition, Piece> generatePieceLayout(vector<Piece> pieces, vector <TilePosition> layout)
        {
            multimap<TilePosition, Piece> pieceLayout;
            auto pieceItr = pieces.begin();
            auto layoutItr = layout.begin();
            while (pieceItr != pieces.end() && layoutItr != layout.end()) {
                if (*pieceItr == Piece::Row || *pieceItr == Piece::Space) {
                    pieceItr++;
                    continue;
                }
                pieceLayout.insert(make_pair(*layoutItr, *pieceItr));
                pieceItr++;
                layoutItr++;
            }
            return pieceLayout;
        }

        bool canAddBlock(const TilePosition here, const int width, const int height, multimap<TilePosition, Piece> pieces, BlockType type)
        {
            const auto blockWalkable = [&](const TilePosition &t) {
                return (t.x < here.x || t.x > here.x + width || t.y < here.y || t.y > here.y + height) && !blockGrid[t.x][t.y];
            };

            const auto blockExists = [&](const TilePosition &t) {
                return blockGrid[t.x][t.y + 3];
            };

            const auto productionReachable = [&](TilePosition start) {
                if (!Map::mapBWEM.GetArea(here))
                    return false;
                for (auto &choke : Map::mapBWEM.GetArea(here)->ChokePoints()) {
                    auto path = BWEB::Path(start + TilePosition(0, 3), TilePosition(choke->Center()), UnitTypes::Protoss_Dragoon, false, false);
                    auto maxDist = path.getSource().getDistance(path.getTarget());
                    auto maxDim = max(width, height);
                    path.generateJPS([&](const TilePosition &t) { return path.terrainWalkable(t) && blockWalkable(t) && t.getDistance(path.getTarget()) < maxDist + 5; });
                    Pathfinding::clearCacheFully();
                    if (!path.isReachable())
                        return false;
                }
                return true;
            };

            // Check if placing a Hatchery within this Block cannot be done due to resources around it
            for (auto &[tile, piece] : pieces) {
                if (piece == Piece::Large && Broodwar->self()->getRace() == BWAPI::Races::Zerg && !Broodwar->canBuildHere(tile, UnitTypes::Zerg_Hatchery))
                    return false;
            }

            // Check if a Block of specified size would overlap any bases, resources or other blocks
            for (auto x = here.x; x < here.x + width; x++) {
                for (auto y = here.y; y < here.y + height; y++) {
                    const TilePosition t(x, y);
                    if (!t.isValid() || !Map::mapBWEM.GetTile(t).Buildable() || Map::isReserved(t))
                        return false;
                }
            }

            // Check if this Block would not be reachable
            for (auto &[tile, piece] : pieces) {
                if (piece == Piece::Large && !productionReachable(tile))
                    return false;
            }

            // Check if placing a Block here will prevent other Blocks from being reachable
            for (auto &block : allBlocks) {
                if (Map::mapBWEM.GetArea(block.getTilePosition()) != Map::mapBWEM.GetArea(here))
                    continue;

                for (auto &large : block.getLargeTiles()) {
                    if (!productionReachable(large))
                        return false;
                }
            }

            // Check if placing a Block here will prevent other Stations from being reachable
            if (type != BlockType::Proxy) {
                for (auto &station : Stations::getStations()) {
                    if (station.getBase()->GetArea() != Map::mapBWEM.GetArea(here))
                        continue;

                    /*if (!blockExists(station.getBase()->Location()) && !productionReachable(station.getBase()->Location()))
                        return false;
                    for (auto &location : station.getSecondaryLocations()) {
                        if (!blockExists(location) && (!blockWalkable(location + TilePosition(0, 3))) || !productionReachable(location))
                            return false;
                    }*/
                }
            }
            return true;
        }

        void createBlock(TilePosition here, multimap<TilePosition, Piece> pieceLayout, int width, int height, BlockType type)
        {
            Block newBlock(here, pieceLayout, width, height, type);
            auto area = Map::mapBWEM.GetArea(here);
            allBlocks.push_back(newBlock);
            addToBlockGrid(here, here + TilePosition(width, height));
            for (auto &[_, piece] : pieceLayout)
                piecePerArea[area].pieces[piece]++;

            // Store pieces
            for (auto &[placement, piece] : pieceLayout) {
                if (piece == Piece::Small) {
                    BWEB::Map::addReserve(placement, 2, 2);
                }
                if (piece == Piece::Medium) {
                    BWEB::Map::addReserve(placement, 3, 2);
                }
                if (piece == Piece::Large) {
                    BWEB::Map::addReserve(placement, 4, 3);
                }
                if (piece == Piece::Addon) {
                    BWEB::Map::addReserve(placement, 2, 2);
                }
            }
        }

        void initialize()
        {
            for (auto &station : Stations::getStations()) {
                addToBlockGrid(station.getBase()->Location(), station.getBase()->Location() + TilePosition(4, 3));
                for (auto &secondary : station.getSecondaryLocations())
                    addToBlockGrid(secondary, secondary + TilePosition(4, 3));
                for (auto &def : station.getDefenses())
                    addToBlockGrid(def, def + TilePosition(2, 2));
                for (auto &mineral : station.getBase()->Minerals()) {
                    auto halfway = (mineral->Pos() + station.getBase()->Center()) / 2;
                    addToBlockGrid(TilePosition(halfway) - TilePosition(1, 1), TilePosition(halfway) + TilePosition(1, 1));
                }
            }
        }

        void findMainStartBlocks()
        {
            const auto race = Broodwar->self()->getRace();
            const auto firstStart = Map::getMainPosition();
            const auto secondStart = race != Races::Zerg ? (Position(Map::getMainChoke()->Center()) + Map::getMainPosition()) / 2 : Map::getMainPosition();
            const auto area = Map::getMainArea();

            const auto creepOnCorners = [&](TilePosition here, int width, int height) {
                return Broodwar->hasCreep(here)
                    && Broodwar->hasCreep(here + TilePosition(width - 1, 0))
                    && Broodwar->hasCreep(here + TilePosition(0, height - 1))
                    && Broodwar->hasCreep(here + TilePosition(width - 1, height - 1));
            };

            const auto searchStart = [&](Position start) {
                auto tileStart = TilePosition(start);
                auto tileBest = TilePositions::Invalid;
                auto distBest = DBL_MAX;
                multimap<TilePosition, Piece> bestPieceLayout;
                bool generated = false;
                for (int i = 10; i > 1; i--) {
                    for (int j = 10; j > 1; j--) {

                        // Try to find a block near our starting location
                        for (auto x = tileStart.x - 15; x <= tileStart.x + 15; x++) {
                            for (auto y = tileStart.y - 15; y <= tileStart.y + 15; y++) {
                                const auto tile = TilePosition(x, y);
                                const auto blockCenter = Position(tile) + Position(i * 16, j * 16);

                                // Check if we have pieces and a layout to use
                                const auto pieces = whichPieces(i, j, blockCenter.y < Map::getMainPosition().y, blockCenter.x < Map::getMainPosition().x);
                                const auto layout = wherePieces(tile, pieces);
                                if (pieces.empty() || layout.empty())
                                    continue;

                                const auto smallCount = countPieces(pieces, Piece::Small);
                                const auto mediumCount = countPieces(pieces, Piece::Medium);
                                const auto largeCount = countPieces(pieces, Piece::Large);

                                if (!tile.isValid()
                                    || mediumCount < 1
                                    || (race == Races::Zerg && smallCount > 0 && piecePerArea[area].count(Piece::Small) >= 2)
                                    || (race == Races::Zerg && smallCount == 0 && mediumCount == 0)
                                    || (race == Races::Protoss && largeCount < 2)
                                    || (race == Races::Terran && largeCount < 1)
                                    || (race == Races::Zerg && !creepOnCorners(tile, i, j)))
                                    continue;

                                multimap<TilePosition, Piece> pieceLayout = generatePieceLayout(pieces, layout);
                                const auto dist = blockCenter.getDistance(start);

                                if (dist < distBest && canAddBlock(tile, i, j, pieceLayout, BlockType::Start)) {
                                    bestPieceLayout = pieceLayout;
                                    distBest = dist;
                                    tileBest = tile;
                                }
                            }
                        }

                        if (tileBest.isValid()) {
                            generated = true;
                            createBlock(tileBest, bestPieceLayout, i, j, BlockType::Start);
                            return true;
                        }
                    }
                }
                return false;
            };

            while (searchStart(firstStart)) {}
            while (searchStart(secondStart)) {}
        }

        void findMainDefenseBlock()
        {
            if (Broodwar->self()->getRace() == Races::Zerg)
                return;

            // Added a block that allows a good shield battery placement or bunker placement
            vector<Piece> pieces ={ Piece::Small, Piece::Medium };
            multimap<TilePosition, Piece> pieceLayout;
            auto tileBest = TilePositions::Invalid;
            auto start = Map::getMainChoke() ? TilePosition(Map::getMainChoke()->Center()) : Map::getMainTile();
            auto distBest = DBL_MAX;
            for (auto x = start.x - 12; x <= start.x + 16; x++) {
                for (auto y = start.y - 12; y <= start.y + 16; y++) {
                    const TilePosition tile(x, y);
                    const auto blockCenter = Position(tile) + Position(80, 32);
                    const auto dist = (blockCenter.getDistance((Position)Map::getMainChoke()->Center()));

                    if (!tile.isValid()
                        || Map::mapBWEM.GetArea(tile) != Map::getMainArea()
                        || dist < 96.0)
                        continue;

                    const auto layout = wherePieces(tile, pieces);
                    pieceLayout = generatePieceLayout(pieces, layout);
                    if (dist < distBest && canAddBlock(tile, 5, 2, pieceLayout, BlockType::Defensive)) {
                        tileBest = tile;
                        distBest = dist;
                    }
                }
            }

            //createBlock(tileBest, pieceLayout, 5, 2, BlockType::Defensive);
        }

        void findProductionBlocks()
        {
            // Calculate distance for each tile to our natural choke, we want to place bigger blocks closer to the chokes
            multimap<double, TilePosition> tilesByPathDist;
            for (int x = 0; x < Broodwar->mapWidth(); x++) {
                for (int y = 0; y < Broodwar->mapHeight(); y++) {
                    const TilePosition t(x, y);
                    if (t.isValid() && Broodwar->isBuildable(t) && Map::mapBWEM.GetArea(t) && !Map::mapBWEM.GetArea(t)->Bases().empty()) {
                        const auto &base = Map::mapBWEM.GetArea(t)->Bases().front();
                        const auto dist = Position(t).getDistance(base.Center());
                        tilesByPathDist.emplace(make_pair(dist, t));
                    }
                }
            }

            // Iterate every tile
            for (int i = 20; i > 0; i--) {
                for (int j = 20; j > 0; j--) {

                    // Check if we have pieces to use
                    const auto pieces = whichPieces(i, j);
                    if (pieces.empty())
                        continue;

                    const auto smallCount = countPieces(pieces, Piece::Small);
                    const auto mediumCount = countPieces(pieces, Piece::Medium);
                    const auto largeCount = countPieces(pieces, Piece::Large);

                    int cnt = 0;
                    for (auto &[v, tile] : tilesByPathDist) {

                        auto area = Map::mapBWEM.GetArea(tile);
                        if (!area)
                            continue;

                        // Protoss caps large pieces in the main at 16 if we don't have necessary medium pieces
                        if (Broodwar->self()->getRace() == Races::Protoss) {
                            if (largeCount > 0 && piecePerArea[area].pieces[Piece::Large] >= 16 && piecePerArea[area].pieces[Piece::Medium] < 8)
                                continue;
                            if (mediumCount > 0 && piecePerArea[area].pieces[Piece::Medium] >= 9)
                                continue;
                        }

                        // Zerg tech placement is limited by creep with adjacent hatchery
                        if (Broodwar->self()->getRace() == Races::Zerg) {
                            if (mediumCount + smallCount > 0/* && largeCount == 0*/)
                                continue;
                            if (piecePerArea[area].pieces[Piece::Large] + largeCount >= 4)
                                continue;
                        }

                        // Terran only need about 20 depot spots
                        if (Broodwar->self()->getRace() == Races::Terran) {
                            if (mediumCount > 0 && piecePerArea[area].pieces[Piece::Medium] >= 20)
                                continue;
                        }

                        const auto layout = wherePieces(tile, pieces);
                        multimap<TilePosition, Piece> pieceLayout = generatePieceLayout(pieces, layout);
                        if (canAddBlock(tile, i, j, pieceLayout, BlockType::Production))
                            createBlock(tile, pieceLayout, i, j, BlockType::Production);
                    }
                }
            }
        }

        void findProxyBlock()
        {
            return;

            // For base-specific locations, avoid all areas likely to be traversed by worker scouts
            set<const BWEM::Area*> areasToAvoid;
            for (auto &first : Map::mapBWEM.StartingLocations()) {
                for (auto &second : Map::mapBWEM.StartingLocations()) {
                    if (first == second)
                        continue;

                    for (auto &choke : Map::mapBWEM.GetPath(Position(first), Position(second))) {
                        areasToAvoid.insert(choke->GetAreas().first);
                        areasToAvoid.insert(choke->GetAreas().second);
                    }
                }

                // Also add any areas that neighbour each start location
                auto baseArea = Map::mapBWEM.GetNearestArea(first);
                for (auto &area : baseArea->AccessibleNeighbours())
                    areasToAvoid.insert(area);
            }

            // Gather the possible enemy start locations
            vector<TilePosition> enemyStartLocations;
            for (auto &start : Map::mapBWEM.StartingLocations()) {
                if (Map::mapBWEM.GetArea(start) != Map::getMainArea())
                    enemyStartLocations.push_back(start);
            }

            // Check if this block is in a good area
            const auto goodArea = [&](TilePosition t) {
                for (auto &start : enemyStartLocations) {
                    if (Map::mapBWEM.GetArea(t) == Map::mapBWEM.GetArea(start))
                        return false;
                }
                for (auto &area : areasToAvoid) {
                    if (Map::mapBWEM.GetArea(t) == area)
                        return false;
                }
                return true;
            };

            // Check if there's a blocking neutral between the positions to prevent bad pathing
            const auto blockedPath = [&](Position source, Position target) {
                for (auto &choke : Map::mapBWEM.GetPath(source, target)) {
                    if (Map::isUsed(TilePosition(choke->Center())) != UnitTypes::None)
                        return true;
                }
                return false;
            };

            // Find the best locations
            vector<Piece> pieces ={ Piece::Large, Piece::Large, Piece::Row, Piece::Small, Piece::Small, Piece::Small, Piece::Small };
            TilePosition tileBest = TilePositions::Invalid;
            multimap<TilePosition, Piece> bestPieceLayout;
            auto distBest = DBL_MAX;
            for (int x = 0; x < Broodwar->mapWidth(); x++) {
                for (int y = 0; y < Broodwar->mapHeight(); y++) {
                    const TilePosition topLeft(x, y);
                    const TilePosition botRight(x + 8, y + 5);
                    vector<TilePosition> layout = wherePieces(topLeft, pieces);
                    auto pieceLayout = generatePieceLayout(pieces, layout);

                    if (!topLeft.isValid()
                        || !botRight.isValid()
                        || !canAddBlock(topLeft, 8, 5, pieceLayout, BlockType::Proxy))
                        continue;

                    const Position blockCenter = Position(topLeft) + Position(160, 96);

                    // Consider each start location
                    auto dist = 0.0;
                    for (auto &base : enemyStartLocations) {
                        const auto baseCenter = Position(base) + Position(64, 48);
                        dist += Map::getGroundDistance(blockCenter, baseCenter);
                        if (blockedPath(blockCenter, baseCenter)) {
                            dist = DBL_MAX;
                            break;
                        }
                    }

                    // Bonus for placing in a good area
                    if (goodArea(topLeft) && goodArea(botRight))
                        dist = log(dist);

                    if (dist < distBest) {
                        distBest = dist;
                        tileBest = topLeft;
                        bestPieceLayout = pieceLayout;
                    }
                }
            }

            //createBlock(tileBest, bestPieceLayout, 8, 5, BlockType::Proxy);
        }
    }

    void eraseBlock(const TilePosition here)
    {
        for (auto &it = allBlocks.begin(); it != allBlocks.end(); ++it) {
            auto &block = *it;
            if (here.x >= block.getTilePosition().x && here.x < block.getTilePosition().x + block.width() && here.y >= block.getTilePosition().y && here.y < block.getTilePosition().y + block.height()) {
                allBlocks.erase(it);
                return;
            }
        }
    }

    void findBlocks()
    {
        initialize();
        findMainDefenseBlock();
        findMainStartBlocks();
        findProxyBlock();
        findProductionBlocks();
    }

    void draw()
    {
        for (auto &block : allBlocks)
            block.draw();
        //for (int x = 0; x < Broodwar->mapWidth(); x++) {
        //    for (int y = 0; y < Broodwar->mapHeight(); y++) {
        //        if (testGrid[x][y])
        //            //Broodwar->drawBoxMap(Position(TilePosition(x, y)), Position(TilePosition(x, y)) + Position(32, 32), Colors::Cyan);
        //            Broodwar->drawTextMap(Position(TilePosition(x, y)), "%d", testGrid[x][y]);
        //    }
        //}

    }

    vector<Block>& getBlocks() {
        return allBlocks;
    }

    Block * getClosestBlock(TilePosition here)
    {
        auto distBest = DBL_MAX;
        Block * bestBlock = nullptr;
        for (auto &block : allBlocks) {
            const auto tile = block.getTilePosition() + TilePosition(block.width() / 2, block.height() / 2);
            const auto dist = here.getDistance(tile);

            if (dist < distBest) {
                distBest = dist;
                bestBlock = &block;
            }
        }
        return bestBlock;
    }
}