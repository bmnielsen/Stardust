#include "Block.h"

class Block18x6 : public Block
{
public:
    Block18x6(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 18; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block18x6>(tile, tile + BWAPI::TilePosition(8, 2));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 2));
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 4));
        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 3));
        large.emplace_back(topLeft + BWAPI::TilePosition(10, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(10, 3));
        large.emplace_back(topLeft + BWAPI::TilePosition(14, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(14, 3));
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 3), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 3), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(10, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(10, 3), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(14, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(14, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(12, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(12, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(14, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(16, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(14, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(16, 3), true);
    }
};