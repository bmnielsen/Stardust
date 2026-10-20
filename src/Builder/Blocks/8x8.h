#include "Block.h"

class Block8x8 : public BuildBlock
{
public:
    Block8x8(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 8; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block8x8>(tilePos, tilePos + BWAPI::TilePosition(3, 3));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(3, 3));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 3));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 3));
        large.emplace_back(cornerTL);
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 5));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 5));
        small.emplace_back(cornerTL + BWAPI::TilePosition(1, 3), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(5, 3), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 5), true);
    }
};