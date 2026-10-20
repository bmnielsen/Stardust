#include "Block.h"

class Block4x4 : public BuildBlock
{
public:
    Block4x4(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 4; }

    [[nodiscard]] int height() const override { return 4; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block4x4>(tilePos, tilePos);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 0));
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 2));
    }
};