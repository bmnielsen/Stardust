#include "Block.h"

class Block6x3 : public Block
{
public:
    Block6x3(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 6; }

    [[nodiscard]] int height() const override { return 3; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block6x3>(tile, tile);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(2, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(2, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 0), true);
    }
};