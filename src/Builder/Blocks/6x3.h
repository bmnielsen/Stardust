#include "Block.h"

class Block6x3 : public BuildBlock
{
public:
    Block6x3(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 6; }

    [[nodiscard]] int height() const override { return 3; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block6x3>(tilePos, tilePos);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL);
        large.emplace_back(cornerTL + BWAPI::TilePosition(2, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(2, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 0), true);
    }
};