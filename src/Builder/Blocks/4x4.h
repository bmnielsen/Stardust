#include "Block.h"

class Block4x4 : public Block
{
public:
    Block4x4(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 4; }

    [[nodiscard]] int height() const override { return 4; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block4x4>(tile, tile);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 0));
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 2));
    }
};