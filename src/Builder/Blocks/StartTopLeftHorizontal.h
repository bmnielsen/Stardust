#include "Block.h"

// This start block is useful in mains with a geyser on left and minerals on right, like Tau Cross 6 o'clock
class StartTopLeftHorizontal : public Block
{
public:
    explicit StartTopLeftHorizontal(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 11; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        // For start blocks the provided tile is the nexus
        auto blockTile = tile + BWAPI::TilePosition(-7, -4);

        // Generate block tiles
        std::vector<BWAPI::TilePosition> usedTiles = {
                blockTile + BWAPI::TilePosition(0, 0),
                blockTile + BWAPI::TilePosition(1, 0),
                blockTile + BWAPI::TilePosition(2, 0),
                blockTile + BWAPI::TilePosition(3, 0),
                blockTile + BWAPI::TilePosition(4, 0),
                blockTile + BWAPI::TilePosition(5, 0),
                blockTile + BWAPI::TilePosition(6, 0),
                blockTile + BWAPI::TilePosition(7, 0),
                blockTile + BWAPI::TilePosition(8, 0),
                blockTile + BWAPI::TilePosition(9, 0),
                blockTile + BWAPI::TilePosition(10, 0),
                blockTile + BWAPI::TilePosition(0, 1),
                blockTile + BWAPI::TilePosition(1, 1),
                blockTile + BWAPI::TilePosition(2, 1),
                blockTile + BWAPI::TilePosition(3, 1),
                blockTile + BWAPI::TilePosition(4, 1),
                blockTile + BWAPI::TilePosition(5, 1),
                blockTile + BWAPI::TilePosition(6, 1),
                blockTile + BWAPI::TilePosition(7, 1),
                blockTile + BWAPI::TilePosition(8, 1),
                blockTile + BWAPI::TilePosition(9, 1),
                blockTile + BWAPI::TilePosition(10, 1),
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
                blockTile + BWAPI::TilePosition(8, 3),
                blockTile + BWAPI::TilePosition(9, 3),
                blockTile + BWAPI::TilePosition(10, 3),
                blockTile + BWAPI::TilePosition(5, 4),
                blockTile + BWAPI::TilePosition(6, 4),
                blockTile + BWAPI::TilePosition(5, 5),
                blockTile + BWAPI::TilePosition(6, 5)
        };

        // Generate surrounding tiles
        std::vector<BWAPI::TilePosition> borderTiles = {
                blockTile + BWAPI::TilePosition(7, 6),
                blockTile + BWAPI::TilePosition(5, 6),
                blockTile + BWAPI::TilePosition(4, 6),
                blockTile + BWAPI::TilePosition(4, 5),
                blockTile + BWAPI::TilePosition(4, 4),
                blockTile + BWAPI::TilePosition(7, 3),
                blockTile + BWAPI::TilePosition(6, 3),
                blockTile + BWAPI::TilePosition(5, 3),
                blockTile + BWAPI::TilePosition(4, 3),
                blockTile + BWAPI::TilePosition(3, 3),
                blockTile + BWAPI::TilePosition(2, 3),
                blockTile + BWAPI::TilePosition(1, 3),
                blockTile + BWAPI::TilePosition(0, 3),
                blockTile + BWAPI::TilePosition(-1, 3),
                blockTile + BWAPI::TilePosition(-1, 2),
                blockTile + BWAPI::TilePosition(-1, 1),
                blockTile + BWAPI::TilePosition(-1, 0),
                blockTile + BWAPI::TilePosition(-1, -1),
                blockTile + BWAPI::TilePosition(0, -1),
                blockTile + BWAPI::TilePosition(1, -1),
                blockTile + BWAPI::TilePosition(2, -1),
                blockTile + BWAPI::TilePosition(3, -1),
                blockTile + BWAPI::TilePosition(4, -1),
                blockTile + BWAPI::TilePosition(5, -1),
                blockTile + BWAPI::TilePosition(6, -1),
                blockTile + BWAPI::TilePosition(7, -1),
                blockTile + BWAPI::TilePosition(8, -1),
                blockTile + BWAPI::TilePosition(9, -1),
                blockTile + BWAPI::TilePosition(10, -1),
                blockTile + BWAPI::TilePosition(11, -1),
                blockTile + BWAPI::TilePosition(11, 0),
                blockTile + BWAPI::TilePosition(11, 1),
                blockTile + BWAPI::TilePosition(11, 2),
                blockTile + BWAPI::TilePosition(11, 3),
                blockTile + BWAPI::TilePosition(11, 4)
        };

        if (placeStartBlock(usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartTopLeftHorizontal>(blockTile, blockTile + BWAPI::TilePosition(5, 4));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(5, 4));
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 2));
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        large.emplace_back(topLeft);

        cannons.emplace_back(topLeft + BWAPI::TilePosition(10, 7));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(11, 3));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(5, 7));
    }
};