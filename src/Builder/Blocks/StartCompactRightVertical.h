#include "Block.h"

class StartCompactRightVertical : public Block
{
public:
    explicit StartCompactRightVertical(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 6; }

    [[nodiscard]] int height() const override { return 8; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        // For start blocks the provided tile is the nexus
        auto blockTile = tile + BWAPI::TilePosition(4, -3);

        // Generate block tiles
        std::vector<BWAPI::TilePosition> usedTiles = {
                blockTile + BWAPI::TilePosition(2, 0),
                blockTile + BWAPI::TilePosition(3, 0),
                blockTile + BWAPI::TilePosition(4, 0),
                blockTile + BWAPI::TilePosition(5, 0),
                blockTile + BWAPI::TilePosition(2, 1),
                blockTile + BWAPI::TilePosition(3, 1),
                blockTile + BWAPI::TilePosition(4, 1),
                blockTile + BWAPI::TilePosition(5, 1),
                blockTile + BWAPI::TilePosition(0, 2),
                blockTile + BWAPI::TilePosition(1, 2),
                blockTile + BWAPI::TilePosition(2, 2),
                blockTile + BWAPI::TilePosition(3, 2),
                blockTile + BWAPI::TilePosition(4, 2),
                blockTile + BWAPI::TilePosition(5, 2),
                blockTile + BWAPI::TilePosition(0, 3),
                blockTile + BWAPI::TilePosition(1, 3),
                blockTile + BWAPI::TilePosition(2, 3),
                blockTile + BWAPI::TilePosition(3, 3),
                blockTile + BWAPI::TilePosition(4, 3),
                blockTile + BWAPI::TilePosition(5, 3),
                blockTile + BWAPI::TilePosition(0, 4),
                blockTile + BWAPI::TilePosition(1, 4),
                blockTile + BWAPI::TilePosition(2, 4),
                blockTile + BWAPI::TilePosition(3, 4),
                blockTile + BWAPI::TilePosition(4, 4),
                blockTile + BWAPI::TilePosition(5, 4),
                blockTile + BWAPI::TilePosition(0, 5),
                blockTile + BWAPI::TilePosition(1, 5),
                blockTile + BWAPI::TilePosition(2, 5),
                blockTile + BWAPI::TilePosition(3, 5),
                blockTile + BWAPI::TilePosition(4, 5),
                blockTile + BWAPI::TilePosition(5, 5),
                blockTile + BWAPI::TilePosition(0, 6),
                blockTile + BWAPI::TilePosition(1, 6),
                blockTile + BWAPI::TilePosition(2, 6),
                blockTile + BWAPI::TilePosition(3, 6),
                blockTile + BWAPI::TilePosition(4, 6),
                blockTile + BWAPI::TilePosition(5, 6),
                blockTile + BWAPI::TilePosition(0, 7),
                blockTile + BWAPI::TilePosition(1, 7),
                blockTile + BWAPI::TilePosition(2, 7),
                blockTile + BWAPI::TilePosition(3, 7),
                blockTile + BWAPI::TilePosition(4, 7),
                blockTile + BWAPI::TilePosition(5, 7)
        };

        // Generate surrounding tiles
        std::vector<BWAPI::TilePosition> borderTiles = {
                blockTile + BWAPI::TilePosition(-1, 2),
                blockTile + BWAPI::TilePosition(-1, 1),
                blockTile + BWAPI::TilePosition(0, 1),
                blockTile + BWAPI::TilePosition(1, 1),
                blockTile + BWAPI::TilePosition(1, 0),
                blockTile + BWAPI::TilePosition(1, -1),
                blockTile + BWAPI::TilePosition(2, -1),
                blockTile + BWAPI::TilePosition(3, -1),
                blockTile + BWAPI::TilePosition(4, -1),
                blockTile + BWAPI::TilePosition(5, -1),
                blockTile + BWAPI::TilePosition(6, -1),
                blockTile + BWAPI::TilePosition(6, 0),
                blockTile + BWAPI::TilePosition(6, 1),
                blockTile + BWAPI::TilePosition(6, 2),
                blockTile + BWAPI::TilePosition(6, 3),
                blockTile + BWAPI::TilePosition(6, 4),
                blockTile + BWAPI::TilePosition(6, 5),
                blockTile + BWAPI::TilePosition(6, 6),
                blockTile + BWAPI::TilePosition(6, 7),
                blockTile + BWAPI::TilePosition(6, 8),
                blockTile + BWAPI::TilePosition(6, 7),
                blockTile + BWAPI::TilePosition(6, 6),
                blockTile + BWAPI::TilePosition(6, 5),
                blockTile + BWAPI::TilePosition(6, 4),
                blockTile + BWAPI::TilePosition(6, 3),
                blockTile + BWAPI::TilePosition(6, 2),
                blockTile + BWAPI::TilePosition(6, 1),
                blockTile + BWAPI::TilePosition(6, 0),
                blockTile + BWAPI::TilePosition(6, -1),
                blockTile + BWAPI::TilePosition(5, -1),
                blockTile + BWAPI::TilePosition(4, -1)

        };

        if (placeStartBlock(usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartCompactRightVertical>(blockTile, blockTile + BWAPI::TilePosition(0, 4));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 4));
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
        medium.emplace_back(topLeft + BWAPI::TilePosition(0, 6));
        medium.emplace_back(topLeft + BWAPI::TilePosition(3, 6));
        large.emplace_back(topLeft + BWAPI::TilePosition(2, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(2, 3));

        cannons.emplace_back(topLeft + BWAPI::TilePosition(-6, 6));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(-6, 1));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(-2, 6));
        cannons.emplace_back(topLeft + BWAPI::TilePosition(-2, 1));
    }
};