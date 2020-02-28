#include "Block.h"

class Block4x5 : public Block
{
public:
    Block4x5(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 4; }

    [[nodiscard]] int height() const override { return 5; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block4x5>(tile, tile);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
        medium.emplace_back(topLeft + BWAPI::TilePosition(0, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 2), true);
    }
};