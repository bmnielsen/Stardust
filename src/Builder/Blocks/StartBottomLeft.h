#include "Block.h"

// This start block is useful in mains with a geyser on left and minerals on right, like Tau Cross 6 o'clock
class StartBottomLeft : public Block
{
public:
    explicit StartBottomLeft(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 11; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        // For start blocks the provided tile is the nexus
        auto blockTile = tile + BWAPI::TilePosition(-9, 1);

        // Generate block tiles
        std::vector<BWAPI::TilePosition> usedTiles = {
                blockTile + BWAPI::TilePosition(7, 0),
                blockTile + BWAPI::TilePosition(8, 0),
                blockTile + BWAPI::TilePosition(7, 1),
                blockTile + BWAPI::TilePosition(8, 1),
                blockTile + BWAPI::TilePosition(0, 2),
                blockTile + BWAPI::TilePosition(1, 2),
                blockTile + BWAPI::TilePosition(2, 2),
                blockTile + BWAPI::TilePosition(3, 2),
                blockTile + BWAPI::TilePosition(4, 2),
                blockTile + BWAPI::TilePosition(5, 2),
                blockTile + BWAPI::TilePosition(6, 2),
                blockTile + BWAPI::TilePosition(7, 2),
                blockTile + BWAPI::TilePosition(8, 2),
                blockTile + BWAPI::TilePosition(9, 2),
                blockTile + BWAPI::TilePosition(10, 2),
                blockTile + BWAPI::TilePosition(0, 3),
                blockTile + BWAPI::TilePosition(1, 3),
                blockTile + BWAPI::TilePosition(2, 3),
                blockTile + BWAPI::TilePosition(3, 3),
                blockTile + BWAPI::TilePosition(4, 3),
                blockTile + BWAPI::TilePosition(5, 3),
                blockTile + BWAPI::TilePosition(6, 3),
                blockTile + BWAPI::TilePosition(7, 3),
                blockTile + BWAPI::TilePosition(8, 3),
                blockTile + BWAPI::TilePosition(9, 3),
                blockTile + BWAPI::TilePosition(10, 3),
                blockTile + BWAPI::TilePosition(0, 4),
                blockTile + BWAPI::TilePosition(1, 4),
                blockTile + BWAPI::TilePosition(2, 4),
                blockTile + BWAPI::TilePosition(3, 4),
                blockTile + BWAPI::TilePosition(4, 4),
                blockTile + BWAPI::TilePosition(5, 4),
                blockTile + BWAPI::TilePosition(6, 4),
                blockTile + BWAPI::TilePosition(7, 4),
                blockTile + BWAPI::TilePosition(8, 4),
                blockTile + BWAPI::TilePosition(9, 4),
                blockTile + BWAPI::TilePosition(10, 4),
                blockTile + BWAPI::TilePosition(8, 5),
                blockTile + BWAPI::TilePosition(9, 5),
                blockTile + BWAPI::TilePosition(10, 5)
        };

        // Generate surrounding tiles
        std::vector<BWAPI::TilePosition> borderTiles = {
                blockTile + BWAPI::TilePosition(8, -1),
                blockTile + BWAPI::TilePosition(7, -1),
                blockTile + BWAPI::TilePosition(6, -1),
                blockTile + BWAPI::TilePosition(6, 0),
                blockTile + BWAPI::TilePosition(6, 1),
                blockTile + BWAPI::TilePosition(5, 1),
                blockTile + BWAPI::TilePosition(4, 1),
                blockTile + BWAPI::TilePosition(3, 1),
                blockTile + BWAPI::TilePosition(2, 1),
                blockTile + BWAPI::TilePosition(1, 1),
                blockTile + BWAPI::TilePosition(0, 1),
                blockTile + BWAPI::TilePosition(-1, 1),
                blockTile + BWAPI::TilePosition(-1, 2),
                blockTile + BWAPI::TilePosition(-1, 3),
                blockTile + BWAPI::TilePosition(-1, 4),
                blockTile + BWAPI::TilePosition(-1, 5),
                blockTile + BWAPI::TilePosition(0, 5),
                blockTile + BWAPI::TilePosition(1, 5),
                blockTile + BWAPI::TilePosition(2, 5),
                blockTile + BWAPI::TilePosition(3, 5),
                blockTile + BWAPI::TilePosition(4, 5),
                blockTile + BWAPI::TilePosition(5, 5),
                blockTile + BWAPI::TilePosition(6, 5),
                blockTile + BWAPI::TilePosition(7, 5),
                blockTile + BWAPI::TilePosition(7, 6),
                blockTile + BWAPI::TilePosition(8, 6),
                blockTile + BWAPI::TilePosition(9, 6),
                blockTile + BWAPI::TilePosition(10, 6),
                blockTile + BWAPI::TilePosition(11, 6),
                blockTile + BWAPI::TilePosition(11, 5),
                blockTile + BWAPI::TilePosition(11, 4),
                blockTile + BWAPI::TilePosition(11, 3),
                blockTile + BWAPI::TilePosition(11, 2)
        };

        if (placeStartBlock(usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartBottomLeft>(blockTile, blockTile + BWAPI::TilePosition(7, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(7, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 2));
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 4));
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 2));
    }
};