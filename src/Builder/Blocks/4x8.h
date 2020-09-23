#include "Block.h"

class Block4x8 : public Block
{
public:
    Block4x8(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 4; }

    [[nodiscard]] int height() const override { return 8; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block4x8>(tile, tile + BWAPI::TilePosition(0, 3));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 3));
        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 5));
        medium.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(0, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 5), true);
    }
};