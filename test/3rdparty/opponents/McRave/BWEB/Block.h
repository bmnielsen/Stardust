#pragma once
#include <set>
#include <BWAPI.h>

namespace BWEB {

    enum class Piece {
        Small, Medium, Large, Addon, Row, Space
    };
    enum class BlockType {
        None, Start, Production, Proxy, Defensive
    };

    class Block
    {
        int w = 0, h = 0;
        BWAPI::TilePosition tile;
        BWAPI::Position center;
        std::set <BWAPI::TilePosition> smallTiles, mediumTiles, largeTiles;
        BlockType type;
    public:
        Block() : w(0), h(0) {};

        Block(BWAPI::TilePosition _tile, std::multimap<BWAPI::TilePosition, Piece> _pieces, int _w, int _h, BlockType _type) {
            tile = _tile;
            type = _type;
            w = _w;
            h = _h;
            center = BWAPI::Position(tile) + BWAPI::Position(w / 2, h / w);

            // Store pieces
            for (auto &[placement, piece] : _pieces) {
                if (piece == Piece::Small) {
                    smallTiles.insert(placement);
                }
                if (piece == Piece::Medium) {
                    mediumTiles.insert(placement);
                }
                if (piece == Piece::Large) {
                    largeTiles.insert(placement);
                }
                if (piece == Piece::Addon) {
                    smallTiles.insert(placement);
                }
            }
        }

        /// <summary> Returns the top left TilePosition of this Block. </summary>
        BWAPI::TilePosition getTilePosition() const { return tile; }

        /// <summary> Returns the center of this Block. </summary>
        BWAPI::Position getCenter() const { return center; }

        /// <summary> Returns the set of TilePositions that belong to 2x2 (small) buildings. </summary>
        std::set<BWAPI::TilePosition>& getSmallTiles() { return smallTiles; }

        /// <summary> Returns the set of TilePositions that belong to 3x2 (medium) buildings. </summary>
        std::set<BWAPI::TilePosition>& getMediumTiles() { return mediumTiles; }

        /// <summary> Returns the set of TilePositions that belong to 4x3 (large) buildings. </summary>
        std::set<BWAPI::TilePosition>& getLargeTiles() { return largeTiles; }

        /// <summary> Returns the set of TilePositions needed for this UnitTypes size. </summary>
        std::set<BWAPI::TilePosition>& getPlacements(BWAPI::UnitType type) {
            if (type.tileWidth() == 4)
                return largeTiles;
            if (type.tileWidth() == 3)
                return mediumTiles;
            return smallTiles;
        }

        /// <summary> Returns true if this Block was generated for proxy usage. </summary>
        bool isProxy() { return type == BlockType::Proxy; }

        /// <summary> Returns true if this Block was generated for defensive usage. </summary>
        bool isDefensive() { return type == BlockType::Defensive; }

        /// <summary> Returns the width of the Block in TilePositions. </summary>
        int width() { return w; }

        /// <summary> Returns the height of the Block in TilePositions. </summary>
        int height() { return h; }

        /// <summary> Inserts a 2x2 (small) building at this location. </summary>
        void insertSmall(const BWAPI::TilePosition here) { smallTiles.insert(here); }

        /// <summary> Inserts a 3x2 (medium) building at this location. </summary>
        void insertMedium(const BWAPI::TilePosition here) { mediumTiles.insert(here); }

        /// <summary> Inserts a 4x3 (large) building at this location. </summary>
        void insertLarge(const BWAPI::TilePosition here) { largeTiles.insert(here); }

        /// <summary> Draws all the features of the Block. </summary>
        void draw();
    };

    namespace Blocks {

        /// <summary> Returns a vector containing every Block </summary>
        std::vector<Block>& getBlocks();

        /// <summary> Returns the closest BWEB::Block to the given TilePosition. </summary>
        Block* getClosestBlock(BWAPI::TilePosition);

        /// <summary> Initializes the building of every BWEB::Block on the map, call it only once per game. </summary>
        void findBlocks();

        /// <summary> Erases any blocks at the specified TilePosition. </summary>
        /// <param name="here"> The TilePosition that you want to delete any BWEB::Block that exists here. </param>
        void eraseBlock(BWAPI::TilePosition here);

        /// <summary> Calls the draw function for each Block that exists. </summary>
        void draw();
    }
}
