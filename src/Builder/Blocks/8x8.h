#include "Block.h"

class Block8x8 : public Block
{
public:
    Block8x8(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 8; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block8x8>(tile, tile + BWAPI::TilePosition(3, 3));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(3, 3));
        medium.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 3));
        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 5));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 5));
        small.emplace_back(topLeft + BWAPI::TilePosition(1, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(5, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 5), true);
    }
};