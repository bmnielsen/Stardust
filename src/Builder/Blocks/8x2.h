#include "Block.h"

class Block8x2 : public BuildBlock
{
public:
    Block8x2(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 2; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block8x2>(tilePos, tilePos + BWAPI::TilePosition(3, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(3, 0));
        medium.emplace_back(cornerTL);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 0));
        small.emplace_back(cornerTL + BWAPI::TilePosition(1, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(5, 0), true);
    }
};