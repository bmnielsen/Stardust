#include "Block.h"

class Block10x6 : public BuildBlock
{
public:
    Block10x6(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 10; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block10x6>(tilePos, tilePos + BWAPI::TilePosition(4, 2));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 2));
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 4));
        large.emplace_back(cornerTL);
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 3));
        large.emplace_back(cornerTL + BWAPI::TilePosition(6, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(6, 3));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 1), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 3), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(6, 1), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(6, 3), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 3), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 3), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(8, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 3), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(8, 3), true);
    }
};