#include "Block.h"

class Block5x2 : public Block
{
public:
    Block5x2(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 5; }

    [[nodiscard]] int height() const override { return 2; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block5x2>(tile, tile);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft);
        medium.emplace_back(topLeft + BWAPI::TilePosition(2, 0));
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 0), true);
    }
};