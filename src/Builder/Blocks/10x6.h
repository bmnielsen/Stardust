#include "Block.h"

class Block10x6 : public Block
{
public:
    Block10x6(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 10; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block10x6>(tile, tile + BWAPI::TilePosition(4, 2));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 2));
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 4));
        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
        large.emplace_back(topLeft + BWAPI::TilePosition(6, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(6, 3));
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 3), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(6, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(6, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 3), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 3), true);
    }
};