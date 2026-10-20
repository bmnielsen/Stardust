#include "Block.h"

class Block4x5 : public BuildBlock
{
public:
    Block4x5(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 4; }

    [[nodiscard]] int height() const override { return 5; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block4x5>(tilePos, tilePos);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 2), true);
    }
};