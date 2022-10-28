#include "Block.h"

// This start block is useful in mains with a geyser on left and minerals on bottom and right, like Outsider 5 o'clock
class StartAboveAndBelowLeft : public Block
{
public:
    explicit StartAboveAndBelowLeft(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 14; }

    [[nodiscard]] int height() const override { return 9; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        // For start blocks the provided tile is the nexus
        auto blockTile = tile + BWAPI::TilePosition(-10, -2);

        // Generate block tiles
        std::vector<BWAPI::TilePosition> usedTiles = {
                blockTile + BWAPI::TilePosition(8, 0),
                blockTile + BWAPI::TilePosition(9, 0),
                blockTile + BWAPI::TilePosition(10, 0),
                blockTile + BWAPI::TilePosition(8, 1),
                blockTile + BWAPI::TilePosition(9, 1),
                blockTile + BWAPI::TilePosition(10, 1),
                blockTile + BWAPI::TilePosition(11, 0),
                blockTile + BWAPI::TilePosition(12, 0),
                blockTile + BWAPI::TilePosition(13, 0),
                blockTile + BWAPI::TilePosition(11, 1),
                blockTile + BWAPI::TilePosition(12, 1),
                blockTile + BWAPI::TilePosition(13, 1),
                blockTile + BWAPI::TilePosition(8, 4),
                blockTile + BWAPI::TilePosition(9, 4),
                blockTile + BWAPI::TilePosition(8, 5),
                blockTile + BWAPI::TilePosition(9, 5),
                blockTile + BWAPI::TilePosition(0, 6),
                blockTile + BWAPI::TilePosition(1, 6),
                blockTile + BWAPI::TilePosition(2, 6),
                blockTile + BWAPI::TilePosition(3, 6),
                blockTile + BWAPI::TilePosition(4, 6),
                blockTile + BWAPI::TilePosition(5, 6),
                blockTile + BWAPI::TilePosition(6, 6),
                blockTile + BWAPI::TilePosition(7, 6),
                blockTile + BWAPI::TilePosition(0, 7),
                blockTile + BWAPI::TilePosition(1, 7),
                blockTile + BWAPI::TilePosition(2, 7),
                blockTile + BWAPI::TilePosition(3, 7),
                blockTile + BWAPI::TilePosition(4, 7),
                blockTile + BWAPI::TilePosition(5, 7),
                blockTile + BWAPI::TilePosition(6, 7),
                blockTile + BWAPI::TilePosition(7, 7),
                blockTile + BWAPI::TilePosition(0, 8),
                blockTile + BWAPI::TilePosition(1, 8),
                blockTile + BWAPI::TilePosition(2, 8),
                blockTile + BWAPI::TilePosition(3, 8),
                blockTile + BWAPI::TilePosition(4, 8),
                blockTile + BWAPI::TilePosition(5, 8),
                blockTile + BWAPI::TilePosition(6, 8),
                blockTile + BWAPI::TilePosition(7, 8),
        };

        // Generate surrounding tiles
        std::vector<BWAPI::TilePosition> borderTiles = {
                blockTile + BWAPI::TilePosition(14, 1),
                blockTile + BWAPI::TilePosition(14, 0),
                blockTile + BWAPI::TilePosition(14, -1),
                blockTile + BWAPI::TilePosition(13, -1),
                blockTile + BWAPI::TilePosition(12, -1),
                blockTile + BWAPI::TilePosition(11, -1),
                blockTile + BWAPI::TilePosition(10, -1),
                blockTile + BWAPI::TilePosition(9, -1),
                blockTile + BWAPI::TilePosition(8, -1),
                blockTile + BWAPI::TilePosition(7, -1),
                blockTile + BWAPI::TilePosition(7, 0),
                blockTile + BWAPI::TilePosition(7, 1),
                blockTile + BWAPI::TilePosition(7, 2),
                blockTile + BWAPI::TilePosition(8, 2),
                blockTile + BWAPI::TilePosition(9, 2),
                blockTile + BWAPI::TilePosition(9, 3),
                blockTile + BWAPI::TilePosition(8, 3),
                blockTile + BWAPI::TilePosition(7, 3),
                blockTile + BWAPI::TilePosition(7, 4),
                blockTile + BWAPI::TilePosition(7, 5),
                blockTile + BWAPI::TilePosition(6, 5),
                blockTile + BWAPI::TilePosition(5, 5),
                blockTile + BWAPI::TilePosition(4, 5),
                blockTile + BWAPI::TilePosition(3, 5),
                blockTile + BWAPI::TilePosition(2, 5),
                blockTile + BWAPI::TilePosition(1, 5),
                blockTile + BWAPI::TilePosition(0, 5),
                blockTile + BWAPI::TilePosition(-1, 5),
                blockTile + BWAPI::TilePosition(-1, 6),
                blockTile + BWAPI::TilePosition(-1, 7),
                blockTile + BWAPI::TilePosition(-1, 8),
                blockTile + BWAPI::TilePosition(-1, 9),
                blockTile + BWAPI::TilePosition(0, 9),
                blockTile + BWAPI::TilePosition(1, 9),
                blockTile + BWAPI::TilePosition(2, 9),
                blockTile + BWAPI::TilePosition(3, 9),
                blockTile + BWAPI::TilePosition(4, 9),
                blockTile + BWAPI::TilePosition(5, 9),
                blockTile + BWAPI::TilePosition(6, 9),
                blockTile + BWAPI::TilePosition(7, 9),
                blockTile + BWAPI::TilePosition(8, 9),
                blockTile + BWAPI::TilePosition(8, 8),
                blockTile + BWAPI::TilePosition(8, 7),
                blockTile + BWAPI::TilePosition(8, 6),
                blockTile + BWAPI::TilePosition(9, 6),
                blockTile + BWAPI::TilePosition(10, 6),
                blockTile + BWAPI::TilePosition(10, 5)
        };

        if (placeStartBlock(usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartAboveAndBelowLeft>(blockTile, blockTile + BWAPI::TilePosition(8, 4));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 4));
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(11, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 6));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 6));

        cannons.emplace_back(topLeft + BWAPI::TilePosition(14, 4));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(11, 5));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(14, 1));
    }
};