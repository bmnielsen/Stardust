#include "Block.h"

class Block18x3 : public Block
{
public:
    Block18x3(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 18; }

    [[nodiscard]] int height() const override { return 3; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block18x3>(tile, tile + BWAPI::TilePosition(8, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(10, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(14, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 0), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 0), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(10, 0), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(14, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(12, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(14, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(16, 0), true);
    }
};