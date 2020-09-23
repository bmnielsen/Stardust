#include "Block.h"

class Block2x2 : public Block
{
public:
    Block2x2(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 2; }

    [[nodiscard]] int height() const override { return 2; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block2x2>(tile, tile);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft);
    }
};