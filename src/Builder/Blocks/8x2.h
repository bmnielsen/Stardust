#include "Block.h"

class Block8x2 : public Block
{
public:
    Block8x2(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 2; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block8x2>(tile, tile + BWAPI::TilePosition(3, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(3, 0));
        medium.emplace_back(topLeft);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 0));
        small.emplace_back(topLeft + BWAPI::TilePosition(1, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(5, 0), true);
    }
};