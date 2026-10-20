#include "Block.h"

class Block14x3 : public BuildBlock
{
public:
    Block14x3(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 14; }

    [[nodiscard]] int height() const override { return 3; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block14x3>(tilePos, tilePos + BWAPI::TilePosition(8, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(8, 0));
        large.emplace_back(cornerTL);
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(10, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 0), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 0), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(10, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(10, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(12, 0), true);
    }
};