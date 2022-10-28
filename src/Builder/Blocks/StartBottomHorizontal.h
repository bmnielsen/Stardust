#include "Block.h"

// This start block is useful in mains with a geyser on left and minerals on right and top, like Outsider 1 o'clock
class StartBottomHorizontal : public Block
{
public:
    explicit StartBottomHorizontal(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 14; }

    [[nodiscard]] int height() const override { return 4; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        // For start blocks the provided tile is the nexus
        auto blockTile = tile + BWAPI::TilePosition(-6, 2);

        // Generate block tiles
        std::vector<BWAPI::TilePosition> usedTiles = {
                blockTile + BWAPI::TilePosition(4, 0),
                blockTile + BWAPI::TilePosition(5, 0),
                blockTile + BWAPI::TilePosition(4, 1),
                blockTile + BWAPI::TilePosition(5, 1),

                blockTile + BWAPI::TilePosition(0, 2),
                blockTile + BWAPI::TilePosition(1, 2),
                blockTile + BWAPI::TilePosition(2, 2),
                blockTile + BWAPI::TilePosition(0, 3),
                blockTile + BWAPI::TilePosition(1, 3),
                blockTile + BWAPI::TilePosition(2, 3),

                blockTile + BWAPI::TilePosition(3, 2),
                blockTile + BWAPI::TilePosition(4, 2),
                blockTile + BWAPI::TilePosition(5, 2),
                blockTile + BWAPI::TilePosition(3, 3),
                blockTile + BWAPI::TilePosition(4, 3),
                blockTile + BWAPI::TilePosition(5, 3),

                blockTile + BWAPI::TilePosition(6, 1),
                blockTile + BWAPI::TilePosition(7, 1),
                blockTile + BWAPI::TilePosition(8, 1),
                blockTile + BWAPI::TilePosition(9, 1),
                blockTile + BWAPI::TilePosition(6, 2),
                blockTile + BWAPI::TilePosition(7, 2),
                blockTile + BWAPI::TilePosition(8, 2),
                blockTile + BWAPI::TilePosition(9, 2),
                blockTile + BWAPI::TilePosition(6, 3),
                blockTile + BWAPI::TilePosition(7, 3),
                blockTile + BWAPI::TilePosition(8, 3),
                blockTile + BWAPI::TilePosition(9, 3),

                blockTile + BWAPI::TilePosition(10, 1),
                blockTile + BWAPI::TilePosition(11, 1),
                blockTile + BWAPI::TilePosition(12, 1),
                blockTile + BWAPI::TilePosition(13, 1),
                blockTile + BWAPI::TilePosition(10, 2),
                blockTile + BWAPI::TilePosition(11, 2),
                blockTile + BWAPI::TilePosition(12, 2),
                blockTile + BWAPI::TilePosition(13, 2),
                blockTile + BWAPI::TilePosition(10, 3),
                blockTile + BWAPI::TilePosition(11, 3),
                blockTile + BWAPI::TilePosition(12, 3),
                blockTile + BWAPI::TilePosition(13, 3)
        };

        // Generate surrounding tiles
        std::vector<BWAPI::TilePosition> borderTiles = {
                blockTile + BWAPI::TilePosition(5, -1),
                blockTile + BWAPI::TilePosition(4, -1),
                blockTile + BWAPI::TilePosition(3, -1),
                blockTile + BWAPI::TilePosition(3, 0),
                blockTile + BWAPI::TilePosition(3, 1),
                blockTile + BWAPI::TilePosition(2, 1),
                blockTile + BWAPI::TilePosition(1, 1),
                blockTile + BWAPI::TilePosition(0, 1),
                blockTile + BWAPI::TilePosition(-1, 1),
                blockTile + BWAPI::TilePosition(-1, 2),
                blockTile + BWAPI::TilePosition(-1, 3),
                blockTile + BWAPI::TilePosition(-1, 4),
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
                blockTile + BWAPI::TilePosition(11, 4),
                blockTile + BWAPI::TilePosition(12, 4),
                blockTile + BWAPI::TilePosition(13, 4),
                blockTile + BWAPI::TilePosition(14, 4),
                blockTile + BWAPI::TilePosition(14, 3),
                blockTile + BWAPI::TilePosition(14, 2),
                blockTile + BWAPI::TilePosition(14, 1),
                blockTile + BWAPI::TilePosition(14, 0),
                blockTile + BWAPI::TilePosition(13, 0),
                blockTile + BWAPI::TilePosition(12, 0),
                blockTile + BWAPI::TilePosition(11, 0),
                blockTile + BWAPI::TilePosition(10, 0)
        };

        if (placeStartBlock(usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartBottomHorizontal>(blockTile, blockTile + BWAPI::TilePosition(4, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
        medium.emplace_back(topLeft + BWAPI::TilePosition(3, 2));
        large.emplace_back(topLeft + BWAPI::TilePosition(6, 1));
        large.emplace_back(topLeft + BWAPI::TilePosition(10, 1));

        cannons.emplace_back(topLeft + BWAPI::TilePosition(8, -4));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(4, -4));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(10, -1));
    }
};